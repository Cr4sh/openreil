import sys, os, json, base64, unittest, copy
from abc import ABCMeta, abstractmethod
from sets import Set

# REIL constants
from IR import *
from symbolic import *

# supported arhitectures
from arch import x86, arm

arm_thumb = lambda addr: addr | 1    

# architecture constants
ARCH_X86 = 0
ARCH_ARM = 1

# log_init() mask constants
LOG_INFO = 0x00000001 # regular message
LOG_WARN = 0x00000002 # error
LOG_ERR = 0x00000004  # warning
LOG_BIN = 0x00000008  # instruction bytes
LOG_ASM = 0x00000010  # instruction assembly code
LOG_VEX = 0x00000020  # instruction VEX code
LOG_BIL = 0x00000040  # instruction BAP IL code

# enable all debug messages
LOG_ALL = 0x7fffffff

# disable all debug messages
LOG_NONE = 0

LOG_MASK = None
LOG_PATH = None

# translator logging options
def log_init(log_mask, log_path = None):

    global LOG_MASK, LOG_PATH

    LOG_MASK, LOG_PATH = log_mask, log_path


def log_get():

    global LOG_MASK, LOG_PATH

    log_mask, log_path = LOG_MASK, LOG_PATH

    env_path = os.getenv('REIL_LOG_PATH')
    env_mask = os.getenv('REIL_LOG_MASK')
    log_path = log_path if env_path is None else env_path
    log_mask = log_mask if env_mask is None else int(env_mask, 16)

    return log_mask, log_path    


class Error(Exception):

    pass


class StorageError(Error):

    def __init__(self, addr, inum):

        self.addr, self.inum = addr, inum

    def __str__(self):

        return 'Error while reading instruction %s.%.2d from storage' % (hex(self.addr), self.inum)


class ReadError(StorageError):

    def __init__(self, addr):

        self.addr, self.inum = addr, 0

    def __str__(self):

        return 'Error while reading instruction %s' % hex(self.addr)


class ParseError(Error):

    def __str__(self):

        return 'Error while deserializing instruction %s' % hex(self.addr)


def get_arch(arch):

    try: 

        return { ARCH_X86: x86, 
                 ARCH_ARM: arm }[ arch ]

    except KeyError: 

        raise Error('Architecture #%d is unknown' % arch)


class Arg(object):

    def __init__(self, t = None, size = None, name = None, val = None):

        serialized = None        
        if isinstance(t, tuple): 
            
            # tuple with raw data from translator
            serialized, t = t, None        

        self.type = A_NONE if t is None else t
        self.size, self.name, self.val = size, name, val

        # unserialize argument data
        if serialized: 

            if not self.unserialize(serialized):

                raise Error('Invalid serialized data')

    def __hash__(self):

        return hash(( self.type, self.size, self.name, self.val ))    

    def __eq__(self, other):

        return ( self.type, self.size, self.name, self.val ) == \
               ( other.type, other.size, other.name, other.val )

    def __ne__(self, other):

        return not self == other

    def __str__(self):

        mkstr = lambda val: '%s:%s' % (val, self.size_name())

        if self.type == A_NONE:    return ''
        elif self.type == A_REG:   return mkstr(self.name)
        elif self.type == A_TEMP:  return mkstr(self.name)
        elif self.type == A_CONST: return mkstr('%x' % self.get_val())
        elif self.type == A_LOC:   return '%x.%.2x' % self.val

    def get_val(self):

        mkval = lambda mask: long(self.val & mask)

        if self.type != A_CONST:

            raise Error('get_val() is available only for A_CONST')

        if self.size == U1:    return 0 if mkval(0x1) == 0 else 1
        elif self.size == U8:  return mkval(0xff)
        elif self.size == U16: return mkval(0xffff)
        elif self.size == U32: return mkval(0xffffffff)
        elif self.size == U64: return mkval(0xffffffffffffffff)

    def is_var(self):

        # check for temporary or target architecture register
        return self.type == A_REG or self.type == A_TEMP

    def size_name(self):

        return REIL_NAMES_SIZE[self.size]    

    def serialize(self):

        if self.type == A_NONE:              return ()
        elif self.type == A_CONST:           return ( self.type, self.size, self.val )
        elif self.type in [ A_REG, A_TEMP ]: return ( self.type, self.size, self.name )
        elif self.type == A_LOC:             return ( self.type, self.val )

    def unserialize(self, data):

        if len(data) == 3:
      
            self.type, self.size = Arg_type(data), Arg_size(data)

            if self.size not in [ U1, U8, U16, U32, U64 ]:

                return False
            
            if self.type == A_REG:     self.name = Arg_name(data)
            elif self.type == A_TEMP:  self.name = Arg_name(data)
            elif self.type == A_CONST: self.val = Arg_val(data)
            else: 

                return False

        elif len(data) == 2:

            self.type = Arg_type(data)
            addr, inum = Arg_loc(data)

            if self.type == A_LOC: 

                self.val = ( addr, inum )

            else: 

                return False

        elif len(data) == 0:

            self.type = A_NONE

        else: return False

        return True

    def to_symbolic(self, insn, in_state = None):

        if self.type == A_REG or self.type == A_TEMP:

            name = self.name
            if self.type == A_TEMP:

                # use uniqe names for temp registers of each machine instruction
                name += '_%x' % insn.addr

            # register value
            arg = SymVal(name, self.size, is_temp = self.type == A_TEMP)

            if in_state is not None:

                # return expression for this register if state is available
                try: arg = in_state[arg]
                except KeyError: pass

            return arg

        elif self.type == A_CONST:

            # constant value
            return SymConst(self.get_val(), self.size)

        elif self.type == A_LOC:

            # jump location
            return SymIRAddr(*self.val)

        else: return None


class Insn(object):    

    ATTR_DEFS = (( IATTR_FLAGS, 0 ), # optional REIL flags
                 )

    class IRAddr(tuple):

        def __str__(self):

            return '%.x.%.2x' % self

    def __init__(self, op = None, attr = None, size = None, ir_addr = None, 
                       a = None, b = None, c = None):

        json = serialized = None
        if isinstance(op, basestring): 

            # json string            
            json = op
            op = None

        elif isinstance(op, tuple): 
            
            # tuple with raw data from translator
            serialized = op
            op = None

        self.init_attr(attr)

        self.op = I_NONE if op is None else op        
        self.size = 0 if size is None else size        

        self.addr, self.inum = 0L, 0

        if ir_addr is not None:

            self.addr, self.inum = ir_addr

        self.a = Arg() if a is None else a
        self.b = Arg() if b is None else b
        self.c = Arg() if c is None else c        

        # unserialize instruction data
        if json: serialized = InsnJson().from_json(json)
        if serialized: self.unserialize(serialized)

    def __hash__(self):

        return hash(( self.addr, self.inum, self.op,
                      hash(self.a), hash(self.b), hash(self.c) ))

    def __eq__(self, other):

        return ( self.addr, self.inum, self.op, self.a, self.b, self.c ) == \
               ( other.addr, other.inum, other.op, other.a, other.b, other.c )

    def __ne__(self, other):

        return not self == other

    def __str__(self):

        return self.to_str(show_bin = False, show_asm = False)        

    def to_str(self, show_bin = False, show_asm = True):

        ret = ''
        show_asm = show_asm and self.has_attr(IATTR_ASM)
        show_bin = show_bin and self.has_attr(IATTR_BIN)
        show_hdr = show_asm or show_bin

        if show_hdr: ret += ';\n'

        if show_asm:

            mnem, args = self.get_attr(IATTR_ASM)

            ret += ('; asm: %s %s' % (mnem, args)).strip()

            if self.op == I_UNK:

                # print source and destination register arguments for unknown instruction
                src = self.get_attr(IATTR_SRC) if self.has_attr(IATTR_SRC) else []
                dst = self.get_attr(IATTR_DST) if self.has_attr(IATTR_DST) else []

                if len(src) > 0 or len(dst) > 0:

                    info = []
                    to_str = lambda arg: Arg_name(arg)

                    if len(src) > 0: info.append('reads: ' + ', '.join(map(to_str, src)))
                    if len(dst) > 0: info.append('writes: ' + ', '.join(map(to_str, dst)))

                    ret += ' -- %s' % '; '.join(info)

            ret += '\n'

            if not show_bin: 

                ret += '; len: %d\n' % self.size

        if show_bin:

            ret += '; data (%d): %s\n' % (self.size,
                   ' '.join(map(lambda b: '%.2x' % ord(b), self.get_attr(IATTR_BIN))))

        if show_hdr: ret += ';\n'

        return ret + '%.8x.%.2x %7s %16s, %16s, %16s' % \
               (self.addr, self.inum, self.op_name(), \
                self.a, self.b, self.c)

    def op_name(self):

        return REIL_NAMES_INSN[self.op]

    def ir_addr(self): 

        return self.IRAddr(( self.addr, self.inum ))

    def serialize(self):

        info = ( self.addr, self.size )
        args = ( self.a.serialize(), self.b.serialize(), self.c.serialize() )
        
        return ( info, self.inum, self.op, args, self.attr.copy() )

    def unserialize(self, data):

        self.init_attr(Insn_attr(data))
        self.addr, self.inum, self.size = Insn_addr(data), Insn_inum(data), Insn_size(data)

        self.op = Insn_op(data)
        if self.op > len(REIL_INSN) - 1: 

            raise ParseError(self.addr)

        args = Insn_args(data) 
        if len(args) != 3: 

            raise ParseError(self.addr)

        if not self.a.unserialize(args[0]) or \
           not self.b.unserialize(args[1]) or \
           not self.c.unserialize(args[2]): 

           raise ParseError(self.addr)

        return self    

    def init_attr(self, attr):

        self.attr = {} if attr is None else attr

        # initialize missing attributes with default values
        for name, val in self.ATTR_DEFS:

            if not self.has_attr(name): self.set_attr(name, val)

    def get_attr(self, name):

        return self.attr[name]

    def set_attr(self, name, val):

        self.attr[name] = val

    def has_attr(self, name):

        return self.attr.has_key(name)

    def set_flag(self, val):

        self.set_attr(IATTR_FLAGS, self.get_attr(IATTR_FLAGS) | val)

    def del_flag(self, val):

        self.set_attr(IATTR_FLAGS, self.get_attr(IATTR_FLAGS) & ~val)

    def has_flag(self, val):

        return self.get_attr(IATTR_FLAGS) & val != 0    

    def args(self):

        return self.src() + self.dst()

    def dst(self, get_all = False):

        ret = []

        if self.op not in [ I_UNK, I_NONE ]: 

            if get_all: cond = lambda arg: arg.type != A_NONE 
            else: cond = lambda arg: arg.is_var()

            if self.op != I_JCC and self.op != I_STM and \
               cond(self.c): ret.append(self.c)

        if self.op == I_UNK and self.has_attr(IATTR_DST):

            # get operands information from attributes
            ret = map(lambda a: Arg(a), self.get_attr(IATTR_DST))

        return ret

    def src(self, get_all = False):

        ret = []

        if self.op not in [ I_UNK, I_NONE ]: 

            if get_all: cond = lambda arg: arg.type != A_NONE 
            else: cond = lambda arg: arg.is_var()

            if cond(self.a): ret.append(self.a)
            if cond(self.b): ret.append(self.b)

            if (self.op == I_JCC or self.op == I_STM) and \
               cond(self.c): ret.append(self.c)

        if self.op == I_UNK and self.has_attr(IATTR_DST):

            # get operands information from attributes
            ret = map(lambda a: Arg(a), self.get_attr(IATTR_SRC))

        return ret

    def to_symbolic(self, in_state = None):

        # copy input state to output state
        out_state = SymState() if in_state is None else in_state.clone()

        # skip instructions that doesn't update output state
        if not self.op in [ I_NONE, I_UNK ]:

            # convert instruction arguments to symbolic expressions
            a = self.a.to_symbolic(self, out_state)
            b = self.b.to_symbolic(self, out_state)
            c = self.c.to_symbolic(self)

            # move a value to the register
            if self.op == I_STR: out_state.update(c, a)

            # memory read/write
            elif self.op == I_STM: out_state.update_mem_w(c, a, self.a.size)
            elif self.op == I_LDM: out_state.update_mem_r(c, a, self.c.size)            

            # jump
            elif self.op == I_JCC:

                if not self.c.type in [ A_CONST, A_LOC ]:

                    c = out_state.get(c)

                if isinstance(c, SymConst):

                    # make IR addr from numeric constant
                    c = SymIRAddr(c.val, 0)

                if self.a.type == A_CONST:

                    # unconditional
                    if self.a.get_val() != 0: out_state.update(SymIP(), c)

                else:                    

                    true, false = c, SymIRAddr(*self.next())
                    assert true is not None and false is not None

                    # conditional
                    out_state.update(SymIP(), SymCond(a, true, false))

            # other instructions
            else: out_state.update(c, a.to_exp(self.op, b))

        return out_state

    def next(self):

        if self.has_attr(IATTR_NEXT):

            # force to use next instruction that was set in attributes
            return self.get_attr(IATTR_NEXT)

        if self.has_flag(IOPT_RET): 

            # end of function
            return None

        elif self.op == I_JCC and \
             self.a.type == A_CONST and self.a.get_val() != 0 and \
             not self.has_flag(IOPT_CALL):

            # unconditional jump
            return None

        elif self.has_flag(IOPT_ASM_END):

            # go to first IR instruction of next assembly instruction
            return self.IRAddr(( self.next_asm(), 0 ))

        else:

            # go to next IR instruction of current assembly instruction
            return self.IRAddr(( self.addr, self.inum + 1 ))

    def next_asm(self):

        # address of the next assembly instruction
        return self.addr + self.size

    def jcc_loc(self):

        if self.op == I_JCC and self.c.type == A_CONST: 

            return self.IRAddr(( self.c.get_val(), 0 ))

        elif self.op == I_JCC and self.c.type == A_LOC: 

            return self.IRAddr(self.c.val)

        else:

            return None

    def clone(self):

        return Insn(self.serialize())

    def eliminate(self):

        self.op, self.args = I_NONE, {}
        self.a = Arg(A_NONE)
        self.b = Arg(A_NONE)
        self.c = Arg(A_NONE)        

        self.set_flag(IOPT_ELIMINATED)   


class TestInsn(unittest.TestCase):

    def setUp(self):

        attr = { IATTR_FLAGS: IOPT_ASM_END }

        # raw representation of the test instruction
        self.test_data = ((0, 2), 0, I_STR, ((A_REG, U32, 'R_ECX'), (), 
                                             (A_REG, U32, 'R_EAX')), attr)

        # make test instruction
        self.test_insn = Insn(op = I_STR, size = 2, ir_addr = ( 0, 0 ), \
                              a = Arg(A_REG, U32, 'R_ECX'), c = Arg(A_REG, U32, 'R_EAX'), \
                              attr = attr)

    def test_serialize(self):          

        # check instruction serialization
        data = self.test_insn.serialize()
        assert self.test_data == data

        # check instruction unserialization
        insn_1, insn_2 = Insn(), Insn(self.test_data)
        insn_1.unserialize(data)
        assert insn_1.serialize() == insn_2.serialize() == self.test_data

    def test_clone(self):

        # check instruction cloning
        insn_1 = self.test_insn.clone()
        assert insn_1.serialize() == self.test_data

    def test_src_dst(self):  

        # check source and destination args
        assert self.test_insn.src() == [ self.test_insn.a ] and \
               self.test_insn.dst() == [ self.test_insn.c ]

    def test_next(self):  

        # check next instruction address
        insn_1 = Insn(size = 4, ir_addr = (10, 0))
        insn_2 = Insn(size = 4, ir_addr = (10, 1), attr = { IATTR_FLAGS: IOPT_ASM_END })
        assert insn_1.next() == (10, 1) and insn_2.next() == (14, 0)

        insn_2.set_attr(IATTR_NEXT, (10, 2))
        assert insn_2.next() == (10, 2)

    def test_to_symbolic(self):

        sym = Insn(op = I_STR, \
                   a = Arg(A_REG, U32, 'R_ECX'), \
                   c = Arg(A_REG, U32, 'R_EAX')).to_symbolic()

        eax = sym[SymVal('R_EAX', U32)]

        # check for valid store
        assert eax == SymAny() == SymVal('R_ECX', U32)

        sym = Insn(op = I_ADD, \
                   a = Arg(A_REG, U32, 'R_ECX'), \
                   b = Arg(A_REG, U32, 'R_EAX'), \
                   c = Arg(A_REG, U32, 'R_EAX')).to_symbolic()

        eax = sym[SymVal('R_EAX', U32)]

        # check for valid arythmetic expression        
        assert eax == SymAny() \
                   == SymAny() + SymAny() \
                   == SymVal('R_EAX', U32) + SymAny() \
                   == SymVal('R_ECX', U32) + SymAny() \
                   == SymVal('R_EAX', U32) + SymVal('R_ECX', U32)

        sym = Insn(op = I_STM, \
                   a = Arg(A_REG, U32, 'R_EAX'), \
                   c = Arg(A_REG, U32, 'R_ECX')).to_symbolic()

        ecx = sym[SymPtr(SymVal('R_ECX', U32))]

        # check for valid memory write expression
        assert ecx == SymAny() == SymVal('R_EAX', U32)

        sym = Insn(op = I_LDM, \
                   a = Arg(A_REG, U32, 'R_ECX'), \
                   c = Arg(A_REG, U32, 'R_EAX')).to_symbolic()

        eax = sym[SymVal('R_EAX', U32)]

        # check for valid memory read expression
        assert eax == SymAny() \
                   == SymPtr(SymAny()) \
                   == SymPtr(SymVal('R_ECX', U32))


class TestSymState(unittest.TestCase):   

    def test_remove_temp_regs(self): 

        sym = Insn(op = I_STR, \
                   a = Arg(A_REG, U32, 'R_EAX'), \
                   c = Arg(A_REG, U32, 'R_ECX')).to_symbolic()

        sym = Insn(op = I_ADD, \
                   a = Arg(A_REG, U32, 'R_EAX'), \
                   c = Arg(A_TEMP, U32, 'V_01')).to_symbolic(sym)

        sym.remove_temp_regs()  
        assert sym.arg_out() == [ SymVal('R_ECX') ]      

    def test_slice(self):             

        sym = Insn(op = I_STR, \
                   a = Arg(A_REG, U32, 'R_EAX'), \
                   c = Arg(A_REG, U32, 'R_ECX')).to_symbolic()

        sym = Insn(op = I_ADD, \
                   a = Arg(A_REG, U32, 'R_EBX'), \
                   b = Arg(A_CONST, U32, val = 1), \
                   c = Arg(A_REG, U32, 'R_EDX')).to_symbolic(sym)

        sym.slice(val_out = [ 'R_ECX' ])  
        assert sym.arg_out() == [ SymVal('R_ECX') ]      

        sym.slice(val_in = [ 'R_EBX' ])
        assert sym.arg_out() == []      


class InsnJson(): 

    def to_json(self, insn):

        insn = insn.serialize() if isinstance(insn, Insn) else insn
        attr = Insn_attr(insn)

        if attr.has_key(IATTR_BIN): 

            # JSON doesn't support binary data
            attr[IATTR_BIN] = base64.b64encode(attr[IATTR_BIN])

        # JSON doesn't support numeric keys
        attr = [ (key, val) for key, val in attr.items() ]

        return json.dumps(( ( Insn_addr(insn), Insn_size(insn) ), \
                            Insn_inum(insn), Insn_op(insn), Insn_args(insn), attr ))

    def from_json(self, data):                

        attr_new = {}

        # make serialized argument from json data
        arg = lambda a: ( Arg_type(a), \
                          Arg_size(a), \
                          Arg_val(a) if Arg_type(a) == A_CONST else Arg_name(a) ) if len(a) > 0 else ()
        
        insn = json.loads(data)
        attr = Insn_attr(insn)
        args = ( arg(Insn_args(insn)[0]), \
                 arg(Insn_args(insn)[1]), \
                 arg(Insn_args(insn)[2]) )
        
        for key, val in attr: 

            attr_new[key] = val

        if attr_new.has_key(IATTR_BIN): 

            # get instruction binary data from base64
            attr_new[IATTR_BIN] = base64.b64decode(attr_new[IATTR_BIN])

        # return raw instruction data
        return ( ( Insn_addr(insn), Insn_size(insn) ), \
                 Insn_inum(insn), Insn_op(insn), args, attr_new ) 


class TestInsnJson(unittest.TestCase):

    def setUp(self):

        attr = { IATTR_FLAGS: IOPT_ASM_END }

        # raw representation of the test instruction
        self.test_data = ((0, 2), 0, I_STR, ((A_REG, U32, 'R_ECX'), (), 
                                             (A_REG, U32, 'R_EAX')), attr)

        # json representation of the test instruction
        self.json_data = '[[0, 2], 0, %d, [[%d, %d, "%s"], [], [%d, %d, "%s"]], [[%d, %d]]]' % \
                         (I_STR, A_REG, U32, 'R_ECX', \
                                 A_REG, U32, 'R_EAX', 
                                 IATTR_FLAGS, IOPT_ASM_END)

        # make test instruction
        self.test_insn = Insn(op = I_STR, size = 2, ir_addr = ( 0, 0 ), \
                              a = Arg(A_REG, U32, 'R_ECX'), c = Arg(A_REG, U32, 'R_EAX'), \
                              attr = attr)

    def test(self):

        js = InsnJson()

        # check json producing
        assert json.loads(self.json_data) == json.loads(js.to_json(self.test_insn))
        assert json.loads(self.json_data) == json.loads(js.to_json(self.test_data))

        # check json parsing
        assert js.from_json(self.json_data) == self.test_data


class InsnList(list):

    def __str__(self):

        return '\n'.join(map(lambda insn: insn.to_str(show_asm = True, show_bin = True), self)) + '\n'

    def sort(self):

        super(InsnList, self).sort(key = lambda insn: insn.ir_addr())

    def get_range(self, first, last = None):

        if len(self) == 0: return InsnList()

        # use first instruction by default
        first = self[0].ir_addr() if first is None else first
        first = first if isinstance(first, tuple) else ( first, None )

        # query one machine instruction if last wasn't specified
        last = last if last is None else last
        last = last if isinstance(last, tuple) else ( last, None )        

        ret, start = [], False
        first = ( first[0], 0 ) if first[1] is None else first

        for insn in self:

            addr = insn.ir_addr()

            # check for start of the range
            if addr == first: start = True

            if start: ret.append(insn)

            if addr == last or \
               (last[1] is None and addr[0] == last[0] and insn.has_flag(IOPT_ASM_END)):

                # end of the range
                break

        return InsnList(ret)

    def to_symbolic(self, in_state = None, temp_regs = True):

        out_state = None if in_state is None else in_state.copy()

        # update symbolic state with each instruction
        for insn in self: out_state = insn.to_symbolic(out_state)

        # remove temp registers from output state
        if not temp_regs: out_state.remove_temp_regs()

        return out_state


class TestInsnList(unittest.TestCase):

    arch = ARCH_X86

    def setUp(self):

        import translator
        self.tr = translator.Translator(self.arch)

        from pyopenreil.utils import asm
        self.asm = asm.Compiler(self.arch)
        
        self.storage = CodeStorageMem(self.arch)        

    def test_get_range(self):        

        # add test data to the storage
        self.storage.clear()
        self.storage.put_insn(self.tr.to_reil(self.asm.compile('add eax, ecx'), addr = 0L))
        
        # get InsnList instance
        insn = self.storage.get_insn(0)

        a = insn.get_range(None, last = None)        
        b = insn.get_range(None, last = 0)
        c = insn.get_range(0,    last = None)
        d = insn.get_range(0,    last = 0)               
        
        # check get_range with different combinations of None args
        assert a == b == c == d

        b = insn.get_range((0, 1), last = None)
        c = insn.get_range(None,   last = (0, 4))        
        d = insn.get_range((0, 1), last = (0, 4))

        # check get_range with different ranges
        assert a[1:] == b and a[:5] == c and a[1:5] == d

    def test_to_symbolic(self):

        # add test data to the storage
        self.storage.clear()
        self.storage.put_insn(self.tr.to_reil(self.asm.compile('add eax, ecx'), addr = 0L))
        self.storage.put_insn(self.tr.to_reil(self.asm.compile('inc eax'), addr = 2L))

        # get InsnList instance with both machine instructions
        insn = InsnList(self.storage.get_insn(0) + self.storage.get_insn(2))

        # get symbolic representation of EAX value
        sym = insn.to_symbolic()
        eax = sym[SymVal('R_EAX', U32)]

        # check for valid expression
        assert eax == SymVal('R_EAX', U32) + SymVal('R_ECX', U32) + SymConst(1, U32)        

        # expressions fuzzy matching
        assert eax == SymVal('R_EAX', U32) + SymAny() + SymAny() \
                   == SymVal('R_EAX', U32) + SymVal('R_ECX', U32) + SymAny()


class BasicBlock(InsnList):
    
    def __init__(self, insn_list):

        super(BasicBlock, self).__init__(insn_list)

        self.first, self.last = insn_list[0], insn_list[-1]
        self.ir_addr = self.first.ir_addr()
        self.size = self.last.addr + self.last.size - self.ir_addr[0]

    def __str__(self):

        return self.to_str(show_header = True, show_symbolic = True)

    def to_str(self, show_header = True, show_symbolic = False):

        ret = InsnList.__str__(self)

        if show_header:

            ret = '; BB %s : %s\n' % (self.first.ir_addr(), self.last.ir_addr()) + \
                  '; ' + '-' * 32 + '\n' + ret

        if show_symbolic:

            ret += '; ' + '-' * 32 + '\n'

            for item in str(self.to_symbolic(temp_regs = False)).strip().split('\n'):

                ret += '; %s\n' % item

        return ret

    def get_successors(self):

        return self.last.next(), self.last.jcc_loc()    


class TestBasicBlock(unittest.TestCase):    

    def test_x86(self):       

        arch = ARCH_X86
        code = ( 'jne _l', 
                 'nop',
                 '_l: ret' )        
        
        # create translator
        from pyopenreil.utils import asm
        tr = CodeStorageTranslator(asm.Reader(arch, code))

        # translate basic block
        bb = tr.get_bb(0)

        print bb

        # get successors
        lhs, rhs = bb.get_successors()

        # check for valid next instructions of JNE
        assert lhs == Insn.IRAddr(( tr.get_insn(( 0, 0 )).size, 0 ))
        assert rhs == Insn.IRAddr(( tr.get_insn(( 0, 0 )).size + 1, 0 ))

    def test_thumb(self):

        arch = ARCH_ARM
        code = (
            'push     {r7}',
            'cmp      r0, #0',
            'beq      _l',
            'movs     r1, #1',
            '_l: pop  {r7}',
            'mov      pc, lr' )

        # create translator
        from pyopenreil.utils import asm
        tr = CodeStorageTranslator(asm.Reader(arch, code, thumb = True))   

        # translate basic block
        bb = tr.get_bb(arm_thumb(0))

        print bb

        # get successors
        lhs, rhs = bb.get_successors()

        # check for valid next instructions of JNE
        assert lhs == Insn.IRAddr(( 0x05, 4 ))
        assert rhs == Insn.IRAddr(( 0x09, 0 ))


class Func(InsnList):

    class Chunk(object):

        def __init__(self, addr, size):

            self.addr, self.size = addr, size

        def __str__(self):        

            return '0x%x-0x%x' % (self.addr, self.addr + self.size - 1)

        def __eq__(self, other):

            return type(self) == type(other) and \
                   self.addr == other.addr and \
                   self.size == other.size 

        def __hash__(self):

            return hash(( self.addr, self.size ))

    def __init__(self, arch, ir_addr):

        self.arch, self.first, self.last = arch, None, []
        self.addr = ir_addr[0] if isinstance(ir_addr, tuple) else ir_addr        
        self.bb_list, self.chunks = [], []
        self.stack_args = None

    def __str__(self):

        return self.to_str(show_header = True, show_chunks = True)

    def to_str(self, show_header = True, show_chunks = False):        

        ret = ''

        if show_header or show_chunks:

            ret += '; sub_%.8x()\n' % self.addr

            if self.stack_args is not None: 
                  
                ret += '; Stack args size: 0x%x\n' % self.stack_args

            if show_chunks:

                ret += '; Code chunks: %s\n' % ', '.join(map(lambda c: str(c), self.chunks))

            ret += '; ' + '-' * 32 + '\n'

        return ret + InsnList.__str__(self)

    def _add_chunk(self, addr, size):

        last = addr + size

        for chunk in self.chunks:
            
            last_cur = chunk.addr + chunk.size

            # check for overlapping chunks
            if (addr >= chunk.addr and addr <= last_cur) or \
               (last >= chunk.addr and last <= last_cur):

                # merge two chunks
                chunk.addr = min(addr, chunk.addr)
                chunk.size = max(last, last_cur) - chunk.addr

                return

        # add a new chunk
        self.chunks.append(self.Chunk(addr, size))

    def add_chunk(self, addr, size):

        # add/update chunk
        self._add_chunk(addr, size)

        while True:

            chunks, self.chunks = self.chunks, []

            # merge existing chunks
            for chunk in chunks:

                self._add_chunk(chunk.addr, chunk.size)

            if len(chunks) == len(self.chunks): 

                break

    def _get_stack_args_count(self, insn):

        state = insn.to_symbolic()   

        reg = SymVal(self.arch.Registers.sp)
        exp = state.query(reg)

        # check for (R_ESP + X)
        if exp == SymExp(I_ADD, SymAny(), reg):

            arg = 0
            if isinstance(exp.a, SymConst): arg = exp.a.val
            if isinstance(exp.b, SymConst): arg = exp.b.val

            if arg >= self.arch.ptr_len and arg % self.arch.ptr_len == 0:

                return arg / self.arch.ptr_len - 1

        return None

    def add_bb(self, bb):

        if not bb in self.bb_list:

            if bb.ir_addr == ( self.addr, 0 ):

                # set first instructin of the func
                self.first = bb.first

            if bb.last.has_flag(IOPT_RET):

                # set last instruction of the func
                if not bb.last in self.last: self.last.append(bb.last)    

                # update number of stack arguments
                insn_list = bb.get_range(bb.last.addr)
                self.stack_args = self._get_stack_args_count(insn_list)

            # update code chunks and basic blocks information
            self.add_chunk(bb.first.addr, bb.size)
            self.bb_list.append(bb) 
            
            for insn in bb:

                # add bb instruction to func instructions list
                if not insn in self: self.append(insn)            

    def name(self):

        name = 'sub_%x' % self.addr
        if self.stack_args is not None: name += '@%x' % self.stack_args

        return name

    def to_symbolic(self, in_state = None):

        raise Exception('Not available at function level')


class TestFunc(unittest.TestCase):

    arch = ARCH_X86

    def test(self):        

        from pyopenreil.utils import asm       
        
        # dummy stdcall function
        code = ( 'xor eax, eax', 'ret 8' )

        # create translator
        tr = CodeStorageTranslator(asm.Reader(self.arch, code))

        # get function
        fn = tr.get_func(0)

        # check for the number of chunks and stack arguments count
        assert len(fn.chunks) == 1
        assert fn.stack_args == 2


class GraphNode(object):

    def __init__(self, item = None):

        self.item = item
        self.in_edges, self.out_edges = Set(), Set()

    def _find_edge(self, edges, name):

        for edge in edges:

            if edge.name == name: return edge

        return None

    def key(self):

        return id(self)

    def text(self):

        return str(self)

    def present_in_dot_graph(self):

        return True


class GraphEdge(object):

    def __init__(self, node_from, node_to, name = None):
        
        self.node_from, self.node_to = node_from, node_to
        self.name = name

        node_from.out_edges.add(self)
        node_to.in_edges.add(self)

    def __eq__(self, other):

        return hash(self) == hash(other)

    def __ne__(self, other):

        return not self == other

    def __hash__(self):

        return hash(( self.node_from, self.node_to, self.name ))


class Graph(object):

    NODE = GraphNode
    EDGE = GraphEdge

    # for to_dot_file()
    SHAPE = 'box'

    # graph fonts
    NODE_FONT = 'Helvetica'
    EDGE_FONT = 'Helvetica'

    DPI = '120'

    def __init__(self):

        self.reset()

    def reset(self): 

        self.nodes, self.edges = {}, Set()   

    def node(self, key):

        return self.nodes[key]    

    def add_node(self, item):

        node = item if isinstance(item, self.NODE) else self.NODE(item)
        key = node.key()

        try: return self.nodes[key]        
        except KeyError: self.nodes[key] = node

        return node

    def del_node(self, item, remove_from_list = True):

        node = item if isinstance(item, self.NODE) else self.NODE(item)

        edges = Set()
        edges.union_update(node.in_edges)
        edges.union_update(node.out_edges)

        # delete node edges
        for edge in edges: self.del_edge(edge)

        if remove_from_list:

            # delete node from global list
            self.nodes.pop(node.key())

    def add_edge(self, node_from, node_to, name = None):

        if not isinstance(node_from, self.NODE):

            node_from = self.node(node_from)

        if not isinstance(node_to, self.NODE):

            node_to = self.node(node_to)

        edge = self.EDGE(node_from, node_to, name)
        self.edges.add(edge)

        return edge

    def del_edge(self, edge):

        # cleanup output node
        edge.node_from.out_edges.remove(edge)

        # cleanup input node
        edge.node_to.in_edges.remove(edge)

        if edge in self.edges:

            # delete edge
            self.edges.remove(edge)

    def to_dot_file(self, path):

        with open(path, 'w') as fd:

            fd.write('digraph pyopenreil {\n')
            fd.write('dpi="%s"\n' % self.DPI)
            fd.write('edge [fontname="%s"]\n' % self.EDGE_FONT)
            fd.write('node [fontname="%s", shape="%s"]\n' % (self.NODE_FONT, self.SHAPE))

            nodes = self.nodes.values()
            nodes = sorted(nodes, key = lambda node: node.key())

            # write nodes
            for n in range(0, len(nodes)):

                node = nodes[n]
                if node.present_in_dot_graph():

                    fd.write('%d [label="%s"];\n' % (n, node.text()))

            # write edges
            for edge in self.edges:

                name, attr = str(edge), {}
                if len(name) > 0: attr['label'] = '"%s"' % name

                attr = ' '.join(map(lambda a: '%s=%s' % a, attr.items()))

                if edge.node_from.present_in_dot_graph() and \
                   edge.node_to.present_in_dot_graph():

                    fd.write('%d -> %d [%s];\n' % (nodes.index(edge.node_from),
                                                   nodes.index(edge.node_to), attr))

            fd.write('}\n')


class TestGraph(unittest.TestCase):

    def test(self):        

    	# create test graph
    	graph = Graph()

    	a = graph.add_node('A')
    	b = graph.add_node('B')
    	c = graph.add_node('C')

    	graph.add_edge(a, b, name = 'one')
    	graph.add_edge(b, c, name = 'two')
    	graph.add_edge(c, a, name = 'three')

    	# check for added edges
    	assert len(graph.nodes) == 3 and len(graph.edges) == 3
    	assert a.out_edges == b.in_edges and len(b.in_edges) == 1
    	assert b.out_edges == c.in_edges and len(c.in_edges) == 1
    	assert c.out_edges == a.in_edges and len(a.in_edges) == 1

    	# check edge deletion
    	graph.del_edge(list(b.in_edges)[0])
    	graph.del_edge(list(b.out_edges)[0])

    	assert len(graph.nodes) == 3 and len(graph.edges) == 1
    	assert len(a.out_edges) == 0 and len(b.in_edges) == 0 
    	assert len(b.out_edges) == 0 and len(c.in_edges) == 0 

    	# check node deletion
    	graph.del_node(c)

    	assert len(graph.nodes) == 2 and len(graph.edges) == 0
    	assert len(c.out_edges) == 0 and len(a.in_edges) == 0


class CFGraphNode(GraphNode):    

    def __str__(self):

        return '%x.%.2x' % self.item.ir_addr

    def key(self):

        return self.item.ir_addr

    def text(self):

        return '%s - %s' % (self.item.first.ir_addr(), self.item.last.ir_addr())


class CFGraphEdge(GraphEdge):

    def __str__(self):

        return ''


class CFGraph(Graph):    

    NODE = CFGraphNode
    EDGE = CFGraphEdge

    def eliminate_dead_code(self):

        pass    


class CFGraphBuilder(object):

    def __init__(self, storage):

        self.arch = storage.arch
        self.storage = storage

    is_thumb = lambda self, addr: self.arch == arm and (addr & 1) == 1

    def process_node(self, bb, state, context): 

        pass

    def get_insn(self, ir_addr):

        return self.storage.get_insn(ir_addr)    

    def _get_bb(self, addr):

        insn_list = InsnList()
        
        while True:

            # translate single assembly instruction
            insn_list += self.get_insn(addr)
            insn = insn_list[-1]

            addr += insn.size

            # check for end of basic block end
            if insn.has_flag(IOPT_BB_END): 
                
                next = insn.next_asm()

                # check for thumb mode instructions
                if self.is_thumb(insn.addr) and self.is_thumb(next):

                    # check for I_JCC that used to propagate thumb enable bit
                    if insn.op == I_JCC and insn.has_flag(IOPT_ASM_END) and \
                       insn.a.type == A_CONST and insn.a.get_val() != 0 and \
                       insn.c.type == A_LOC and insn.c.val == ( next, 0 ):

                        # don't split this code on basic blocks
                        insn.del_flag(IOPT_BB_END)
                        continue
                
                break            

        return insn_list    

    def get_bb(self, ir_addr):

        ir_addr = ir_addr if isinstance(ir_addr, tuple) else (ir_addr, None)
        addr, inum = ir_addr

        inum = 0 if inum is None else inum
        last = inum

        # translate assembly basic block at given address
        insn_list = self._get_bb(addr)

        # split it into the IR basic blocks
        for insn in insn_list[inum:]:

            last += 1
            if insn.has_flag(IOPT_BB_END): 

                return BasicBlock(insn_list[inum:last])

    def traverse(self, ir_addr, state = None, context = None):

        stack, nodes, edges = [], [], []
        cfg = CFGraph()

        ir_addr = ir_addr if isinstance(ir_addr, tuple) else (ir_addr, None)        
        state = {} if state is None else state        

        def _process_node(bb, state, context):

            if bb.ir_addr not in nodes: 

                nodes.append(bb.ir_addr)
                self.process_node(bb, state, context)

        def _process_edge(edge):

            if edge not in edges: edges.append(edge)

        # iterative pre-order CFG traversal
        while True:

            # query IR for basic block
            bb = self.get_bb(ir_addr)
            assert bb is not None

            cfg.add_node(bb)

            _process_node(bb, state, context)

            # process immediate postdominators
            lhs, rhs = bb.last.next(), bb.last.jcc_loc()

            if rhs is not None: 

                _process_edge(( bb.ir_addr, rhs ))

                if not rhs in nodes:
                    
                    stack.append(( rhs, state.copy() ))

            if lhs is not None: 

                _process_edge(( bb.ir_addr, lhs ))
                stack.append(( lhs, state.copy() ))
            
            try: ir_addr, state = stack.pop()
            except IndexError: break

        # add processed edges to the CFG
        for edge in edges: cfg.add_edge(*edge)
            
        return cfg


class TestCFGraphBuilder(unittest.TestCase):

    arch = ARCH_X86

    def setUp(self):

        import translator
        self.tr = translator.Translator(self.arch)

        from pyopenreil.utils import asm
        self.asm = asm.Compiler(self.arch)
        
        self.storage = CodeStorageMem(self.arch)        

    def test(self):        

        # add test data to the storage
        self.storage.clear()
        self.storage.put_insn(self.tr.to_reil(self.asm.compile('rep movsb'), addr = 0L))
        self.storage.put_insn(self.tr.to_reil(self.asm.compile('ret'), addr = 2L))

        print '\n', self.storage

        # translate basic block
        bb = CFGraphBuilder(self.storage).get_bb(0)
        
        assert len(bb) == 3
        assert bb.get_successors() == (( 0, 3 ), ( 2, 0 ))

        assert bb.last.op == I_JCC and bb.last.ir_addr() == ( 0, 2 )
        assert bb.first.op == I_STR and bb.first.ir_addr() == ( 0, 0 )                

        # construct CFG
        cfg = CFGraphBuilder(self.storage).traverse(0)

        assert len(cfg.nodes) == 3
        assert len(cfg.edges) == 3


class DFGraphNode(GraphNode):    

    def __str__(self):

        return '%s %s' % (self.key(), self.item.op_name())

    def present_in_dot_graph(self):

        if self.item is not None and \
           self.item.has_flag(IOPT_ELIMINATED):

            # don't show DFG nodes of eliminated instructions
            return False

        return True

    def key(self):

        return self.item.ir_addr()


class DFGraphEntryNode(DFGraphNode): 

    class Label(tuple): 

        def __str__(self): 

            return 'ENTRY'

    def __str__(self):

        return str(self.key())

    def key(self):

        return self.Label(( None, 0L ))


class DFGraphExitNode(DFGraphNode): 

    class Label(tuple): 

        def __str__(self): 

            return 'EXIT'

    def __str__(self):

        return str(self.key())

    def key(self):

        return self.Label(( None, 1L ))


class DFGraphEdge(GraphEdge):

    def __str__(self):

        return self.name

    def __repr__(self):

        return '<DFG edge "%s">' % self.name


class DFGraph(Graph):    

    NODE = DFGraphNode
    EDGE = DFGraphEdge

    def reset(self):

        super(DFGraph, self).reset()

        self.entry_node = DFGraphEntryNode()
        self.exit_node = DFGraphExitNode()        

        self.add_node(self.entry_node)
        self.add_node(self.exit_node)

        self.deleted_nodes = Set()

    def del_node(self, node):

        super(DFGraph, self).del_node(node, remove_from_list = False)

        # change instruction of deleted node to I_NONE
        node.item.eliminate()

    def store(self, storage):

        addr_list = Set()

        for node in self.nodes.values() + list(self.deleted_nodes): 

            insn = node.item
            if insn is not None:                

                # collect list of available machine instructions including deleted ones
                addr_list = addr_list.union([ insn.addr ])        

        for addr in addr_list:

            # delete all IR for collected instructions
            try: storage.del_insn(addr)
            except StorageError: pass

        # Instruction with inum == 0 is also contains
        # metainformation about machine insturction.
        # If such instruction was eliminated - we need to
        # provide this information to next IR instruction of
        # machine instruction.
        for node in self.nodes.values():

            insn = node.item
            if insn is None or \
               insn.inum != 0 or not \
               insn.has_flag(IOPT_ELIMINATED): continue            
            
            # find not eliminated IR instruction 
            next = insn  
            while next.has_flag(IOPT_ELIMINATED) and not \
                  next.has_flag(IOPT_ASM_END):

                next = self.nodes[( next.addr, next.inum + 1 )].item

            if next != insn:

                # copy information about machine instruction
                if insn.has_attr(IATTR_BIN): next.set_attr(IATTR_BIN, insn.get_attr(IATTR_BIN))
                if insn.has_attr(IATTR_ASM): next.set_attr(IATTR_ASM, insn.get_attr(IATTR_ASM))

        relink = False
        for node in self.nodes.values():

            insn = node.item
            if insn is not None:                
            
                if not insn.has_flag(IOPT_ELIMINATED):

                    # put each node instruction into the storage
                    storage.put_insn(insn.serialize())

                relink = True

        # update inums and flags
        if relink: storage.fix_inums_and_flags()

        relink = False
        for node in self.deleted_nodes:

            insn = node.item

            # For CFG consistence we need to insert I_NONE
            # instruction if whole machine instruction was eliminated.
            try: storage.get_insn(( insn.addr, 0 ))
            except StorageError: 

                insn = insn.clone()
                insn.inum = 0

                insn.eliminate()
                storage.put_insn(insn.serialize())
                relink = True

        # update inums and flags
        if relink: storage.fix_inums_and_flags()

    def optimize_all(self, storage = None):

        # run all available optimizations                
        self.constant_folding(storage = storage)        
        self.eliminate_subexpressions(storage = storage)
        self.eliminate_dead_code(storage = storage)  
        
    def constant_folding(self, storage = None):

        deleted_nodes = []

        from VM import Math

        def _eliminate(node):

            self.del_node(node)
            deleted_nodes.append(node)
            return 1

        def _evaluate(node): 

            insn = node.item
            val = Math(insn.a, insn.b).eval(insn.op)

            if val is not None:

                return Arg(A_CONST, insn.c.size, val = val)

            else:

                return None

        def _propagate_check(node):

            insn = node.item

            if insn.op in [ I_JCC, I_STM, I_LDM, I_NONE, I_UNK ]: 

                return False

            for arg in insn.src(get_all = True):

                if arg.type != A_CONST: return False

            for arg in insn.dst(get_all = True):

                if arg.type != A_TEMP: return False

            return True

        def _propagate_do(node, arg):            

            for edge in node.out_edges:

                node_next = edge.node_to
                insn_next = node_next.item

                if insn_next.op == I_UNK:

                    # Don't eliminate current instruction if any I_UNK
                    # instructions uses it's results.
                    return False

            for edge in node.out_edges:

                node_next = edge.node_to
                insn_next = node_next.item

                print 'Updating arg %s of DFG node "%s" to %s' % (edge, node_next, arg)

                # Propagate constant value to immediate postdominators
                # of current DFG node.
                if insn_next.a.name == edge.name: insn_next.a = arg
                if insn_next.b.name == edge.name: insn_next.b = arg
                if insn_next.c.name == edge.name: insn_next.c = arg

            return True        

        def _constant_folding():

            deleted, pending = 0, []  

            # Collect list of DFG nodes that reprsesents instructions
            # with constant source arguments and A_TEMP as destination argument.
            for node in self.nodes.values():

                if node != self.entry_node and len(node.in_edges) == 0 and \
                   _propagate_check(node):

                    pending.append(node)

            for node in pending:

                print 'DFG node "%s" has no input edges' % node

                # evaluate constant expression
                arg = _evaluate(node)
                if arg is None: 
                
                    # propagate constants information
                    if _propagate_do(node, arg):

                        # delete node, it has no output edges anymore                        
                        deleted += _eliminate(node)

            return deleted

        print '*** Folding constants...'

        while True:            
        
            # perform constants folding untill there will be some nodes to delete    
            if _constant_folding() == 0: break

        print '*** %d nodes deleted' % len(deleted_nodes)

        # update global set of deleted DFG nodes
        self.deleted_nodes = self.deleted_nodes.union(deleted_nodes)

        if storage is not None: self.store(storage)

    def eliminate_subexpressions(self, storage = None):

        deleted_nodes = []

        from VM import Math

        def _eliminate(node):

            self.del_node(node)
            deleted_nodes.append(node)
            return 1

        def _optimize_temp_regs():

            deleted, pending = 0, [] 
            
            # Collect list of DFG nodes that reprsesents STR instructions
            # with A_TEMP or A_REG destination argument and non-constant source.
            for node in self.nodes.values():

                insn = node.item
                if insn is not None and insn.op == I_STR and \
                   insn.a.type != A_CONST and \
                   insn.c.type in [ A_REG, A_TEMP ]:

                    pending.append(node)

            for node in pending:    
                
                # direction of DFG analysis
                backward = node.item.c.type == A_REG                                           

                print 'DFG node "%s" sets value "%s" of "%s"' % \
                       (node, node.item.a, node.item.c)                
            
                if backward:                                        

                    insn = node.item
                    if insn.a.type != A_TEMP:

                        # don't touch instructions that modifies real CPU registers
                        continue                    

                    for edge in node.in_edges:

                        node_prev = edge.node_from
                        out_edges = filter(lambda edge: edge.node_to != node,
                                           node_prev.out_edges)

                        if len(out_edges) > 0:

                            continue

                        insn_prev = node_prev.item

                        if insn_prev is None or \
                           insn_prev.op == I_UNK: continue                        

                        for edge in node.out_edges:

                            found = False
                            for edge_old in node_prev.out_edges:

                                if edge_old.node_to == edge.node_to and \
                                   edge_old.name == insn.c.name:

                                    # such edge is already exists
                                    found = True
                                    break

                            # update DFG edges
                            if not found: self.add_edge(node_prev, edge.node_to, insn.c.name) 

                        insn_prev.c = insn.c
                        deleted += _eliminate(node)
                        break

                else:

                    skip = False 

                    for edge in node.out_edges:

                        insn = node.item
                        node_next = edge.node_to
                        insn_next = node_next.item

                        if insn_next is None: continue

                        if insn_next.op == I_UNK:

                            # Don't eliminate current instruction if any I_UNK
                            # instructions uses it's results.
                            skip = True
                            break

                        # We need to move register argument a from insn
                        # to insn_next and check that other instructions between 
                        # them are not modifying it's value.
                        while insn.ir_addr != insn_next.ir_addr:

                            if insn.c == node.item.a:

                                skip = True
                                break

                            insn = self.nodes[insn.next()].item

                    if not skip:

                        for edge in node.out_edges:

                            insn = node.item
                            node_next = edge.node_to
                            insn_next = node_next.item

                            if insn_next is None: continue

                            print 'Updating arg %s of DFG node "%s" to %s' % \
                                  (edge, node_next, edge.name)

                            # Propagate value to immediate postdominators
                            # of current DFG node.
                            if insn_next.a.name == edge.name: insn_next.a = insn.a
                            if insn_next.b.name == edge.name: insn_next.b = insn.a
                            if insn_next.c.name == edge.name: insn_next.c = insn.a

                            for edge in node.in_edges:

                                found = False
                                for edge_old in edge.node_from.out_edges:

                                    if edge_old.node_to == node_next and \
                                       edge_old.name == insn.a.name:

                                        # such edge is already exists
                                        found = True
                                        break

                                # update DFG edges
                                if not found: self.add_edge(edge.node_from, node_next, insn.a.name)                            

                        deleted += _eliminate(node)
                    
            return deleted

        print '*** Optimizing temp registers usage...'
        
        while True:

            if _optimize_temp_regs() == 0: break

        print '*** %d nodes deleted' % len(deleted_nodes)

        # update global set of deleted DFG nodes
        self.deleted_nodes = self.deleted_nodes.union(deleted_nodes)

        if storage is not None: self.store(storage)

    def eliminate_dead_code(self, keep_flags = False, storage = None):

        deleted_nodes = []

        print '*** Eliminating dead code...'

        # check for variables that live at the end of the function
        for edge in list(self.exit_node.in_edges):

            dst = edge.node_from.item.dst()
            arg = dst[0] if len(dst) > 0 else None

            if arg is None: continue
            
            if (arg.type == A_TEMP) or \
               (arg.type == A_REG and not keep_flags and (arg.name in x86.Registers.flags or \
                                                          arg.name in arm.Registers.flags)):

                print 'Eliminating %s that live at the end of the function...' % arg.name
                self.del_edge(edge)        

        while True:

            deleted = 0            
            
            for node in self.nodes.values():

                if len(node.out_edges) == 0 and node != self.exit_node and \
                   not node.item.op in [ I_JCC, I_STM, I_NONE ]:

                    print 'DFG node "%s" has no output edges' % node

                    # delete node that has no output edges                    
                    self.del_node(node)
                    
                    deleted_nodes.append(node)
                    deleted += 1

            if deleted == 0: 

                # no more nodes to delete
                break

        print '*** %d nodes deleted' % len(deleted_nodes)
        
        # update global set of deleted DFG nodes
        self.deleted_nodes = self.deleted_nodes.union(deleted_nodes)

        if storage is not None: self.store(storage)


class DFGraphBuilder(object):

    def __init__(self, storage):

        self.arch = storage.arch
        self.storage = storage    

    def get_insn(self, ir_addr):

        return self.storage.get_insn(ir_addr) 

    def _process_state(self, bb, state):

        updated = False

        if not hasattr(bb, 'input'): 

            # we visiting this basic block first time
            bb.input = {}

        # create/update basic block input state information
        for var, insn in state.items():

            ir_addr = insn.ir_addr()
            
            if not bb.input.has_key(var): 

                bb.input[var] = Set()

            if not ir_addr in bb.input[var]: 

                bb.input[var].add(ir_addr)
                updated = True

        return updated

    def _process_bb(self, bb, state, dfg, from_insn = False):

        for insn in bb:            

            node = dfg.add_node(insn)

            src = [ arg.name for arg in insn.src() ]
            dst = [ arg.name for arg in insn.dst() ]            

            if insn.has_flag(IOPT_CALL):

                #
                # Function call.
                #
                # To make all of the things a bit more simpler we assuming that:
                #   - target function can read and write all general purpose registers;
                #   - target function is not using any flags that was set in current
                #     function;
                #
                # Normally this approach works fine for code that was generated by HLL
                # compilers, but some handwritten assembly junk can break it.
                #
                src = dst = self.arch.Registers.general

            for arg in src:
                
                # create/find source node of DFG edge
                try: node_from = dfg.add_node(state[arg])
                except KeyError: node_from = dfg.entry_node                      

                # create DFG edge for source argument of instruction
                dfg.add_edge(node_from, node, arg)
            
            for arg in dst: 

                # Update current state with information about registers
                # that was changed by this instruction.
                state[arg] = insn

        # check for end of the function
        if from_insn or bb.get_successors() == ( None, None ):

            for arg_name, insn in state.items():

                # edge from last instruction that changed register to DFG exit node
                dfg.add_edge(dfg.add_node(insn), dfg.exit_node.key(), arg_name)

        return self._process_state(bb, state)

    def from_insn(self, insn, state = None):

        state = {} if state is None else state

        dfg = DFGraph()
        bb = BasicBlock(insn)

        self._process_bb(bb, state, dfg, from_insn = True) 

        return dfg

    def from_addr(self, addr, state = None):

        return self.from_insn(self.get_insn(addr), state = state)

    def traverse(self, ir_addr, state = None):                

        stack = []
        state = {} if state is None else state
        
        ir_addr = ir_addr if isinstance(ir_addr, tuple) else (ir_addr, 0)                

        dfg = DFGraph()
        cfg = CFGraphBuilder(self.storage).traverse(ir_addr)

        while True:

            bb = cfg.node(ir_addr).item

            # process basic block and update current state
            updated = self._process_bb(bb, state, dfg) 

            #
            # Process immediate postdominators of basic block untill it's
            # input state information keeps updating.
            #
            if updated:

                lhs, rhs = bb.get_successors()                
                if rhs is not None: stack.append(( rhs, state.copy() ))                  
                if lhs is not None: stack.append(( lhs, state.copy() ))
            
            try: ir_addr, state = stack.pop()
            except IndexError: break         

        return dfg


class TestDFGraphBuilder(unittest.TestCase):

    arch = ARCH_X86

    def setUp(self):

        import translator
        self.tr = translator.Translator(self.arch)

        from pyopenreil.utils import asm
        self.asm = asm.Compiler(self.arch)
        
        self.storage = CodeStorageMem(self.arch)

    def test_traverse(self):

        # add test data to the storage
        self.storage.clear()
        self.storage.put_insn(self.tr.to_reil(self.asm.compile('rep movsb'), addr = 0L))
        self.storage.put_insn(self.tr.to_reil(self.asm.compile('ret'), addr = 2L))

        print '\n', self.storage

        # construct DFG 
        dfg = DFGraphBuilder(self.storage).traverse(0)

        edges = Set(map(lambda e: str(e), dfg.entry_node.out_edges))
        assert edges == Set([ 'R_ESP', 'R_EDI', 'R_ESI', 'R_ECX', 'R_DFLAG' ])

        edges = Set(map(lambda e: str(e), dfg.exit_node.in_edges))
        assert edges.issuperset(Set([ 'R_ESP', 'R_EDI', 'R_ESI', 'R_ECX' ]))        

    def test_optimizations(self):

        # add test data to the storage
        self.storage.clear()
        self.storage.put_insn(self.tr.to_reil(self.asm.compile('mov edx, 1'), addr = 0L))
        self.storage.put_insn(self.tr.to_reil(self.asm.compile('add ecx, edx'), addr = 5L))
        self.storage.put_insn(self.tr.to_reil(self.asm.compile('ret'), addr = 7L))

        print '\n', self.storage, '\n'

        # construct DFG 
        dfg = DFGraphBuilder(self.storage).traverse(0)

        edges = Set(map(lambda e: str(e), dfg.exit_node.in_edges))
        assert edges.issuperset(Set([ 'R_OF', 'R_ZF', 'R_CF', 'R_AF', 'R_PF', 'R_SF', \
                                      'R_ECX', 'R_EDX', 'R_ESP' ]))

        # run constants and dead code optimizations
        dfg.eliminate_dead_code()
        dfg.constant_folding()

        edges = Set(map(lambda e: str(e), dfg.exit_node.in_edges))
        assert edges == Set([ 'R_ECX', 'R_EDX', 'R_ESP' ])

        # update storage
        storage = CodeStorageMem(self.arch)
        dfg.store(storage)

        print '\n', storage

        # run common subexpression elimination to optimize temp regs usage
        dfg.eliminate_subexpressions()        

        # update storage
        storage = CodeStorageMem(self.arch)
        dfg.store(storage)

        print '\n', storage

        '''
            Check for correct resulting code:

            00000000.00     STR             1:32,                 ,         R_EDX:32
            00000005.00     ADD         R_ECX:32,         R_EDX:32,         R_ECX:32
            00000007.00     LDM         R_ESP:32,                 ,          V_01:32
            00000007.01     ADD         R_ESP:32,             4:32,         R_ESP:32
            00000007.02     JCC              1:1,                 ,          V_01:32

        '''
        assert storage.get_insn(0) == [ Insn(op = I_STR, ir_addr = ( 0, 0 ), 
                                             a = Arg(A_CONST, U32, val = 1), 
                                             c = Arg(A_REG, U32, 'R_EDX')) ]

        assert storage.get_insn(5) == [ Insn(op = I_ADD, ir_addr = ( 5, 0 ), 
                                             a = Arg(A_REG, U32, 'R_ECX'),
                                             b = Arg(A_REG, U32, 'R_EDX'),
                                             c = Arg(A_REG, U32, 'R_ECX')) ]

        assert storage.get_insn(7) == [ Insn(op = I_LDM, ir_addr = ( 7, 0 ), 
                                             a = Arg(A_REG, U32, 'R_ESP'),
                                             c = Arg(A_TEMP, U32, 'V_01')),

                                        Insn(op = I_ADD, ir_addr = ( 7, 1 ), 
                                             a = Arg(A_REG, U32, 'R_ESP'),
                                             b = Arg(A_CONST, U32, val = 4), 
                                             c = Arg(A_REG, U32, 'R_ESP')),

                                        Insn(op = I_JCC, ir_addr = ( 7, 2 ), 
                                             a = Arg(A_CONST, U1, val = 1), 
                                             c = Arg(A_TEMP, U32, 'V_01')) ]


class Reader(object):

    __metaclass__ = ABCMeta

    @abstractmethod
    def read(self, addr, size): pass

    @abstractmethod
    def read_insn(self, addr): pass


class ReaderRaw(Reader):

    def __init__(self, arch, data, addr = 0L):

        self.arch = arch
        self.addr = addr
        self.data = data

        super(ReaderRaw, self).__init__()

    def read(self, addr, size): 

        if addr < self.addr or \
           addr >= self.addr + len(self.data):

            raise ReadError(addr)

        addr -= self.addr        
        return self.data[addr : addr + size]

    def read_insn(self, addr): 

        return self.read(addr, MAX_INST_LEN)


class CodeStorage(object):

    __metaclass__ = ABCMeta

    reader = None

    @abstractmethod
    def get_insn(self, ir_addr): pass

    @abstractmethod
    def put_insn(self, insn_or_insn_list): pass

    @abstractmethod
    def size(self): pass

    @abstractmethod
    def clear(self): pass


class CodeStorageMem(CodeStorage):    

    def __init__(self, arch, insn_list = None, from_file = None): 

        self.arch = arch
        self.clear()
        
        if insn_list is not None: self.put_insn(insn_list)
        if from_file is not None: self.from_file(from_file)

    def __iter__(self):

        keys = self.items.keys()
        keys.sort()

        for k in keys: yield Insn(self._get_insn(k))   

    def __str__(self):

        return '\n'.join(map(lambda insn: str(insn), self))

    def _get_key(self, insn):

        return Insn_addr(insn), Insn_inum(insn)

    def _get_insn(self, ir_addr):
        
        try: return self.items[ir_addr]
        except KeyError: raise StorageError(*ir_addr)    

    def _del_insn(self, ir_addr):

        try: return self.items.pop(ir_addr)
        except KeyError: raise StorageError(*ir_addr)

    def _put_insn(self, insn):

        self.items[self._get_key(insn)] = insn        
    
    def clear(self): 

        self.items = {}

    def size(self): 

        return len(self.items)

    def get_insn(self, ir_addr): 

        ir_addr = ir_addr if isinstance(ir_addr, tuple) else (ir_addr, None)        
        get_single, ret = True, InsnList()

        addr, inum = ir_addr
        if inum is None: 

            # query all IR instructions for given machine instruction
            inum = 0
            get_single = False

        while True:

            # query single IR instruction
            insn = Insn(self._get_insn(( addr, inum )))
            if get_single: return insn

            # build instructions list
            ret.append(insn)

            # stop on machine instruction end
            if insn.has_flag(IOPT_ASM_END): break
            inum += 1

        return ret

    def del_insn(self, ir_addr):

        ir_addr = ir_addr if isinstance(ir_addr, tuple) else (ir_addr, None)        
        del_single = True

        addr, inum = ir_addr
        if inum is None: 

            # delete all IR instructions for given machine instruction
            for insn in self.get_insn(addr): self._del_insn(insn.ir_addr())

        else:

            # delete single IR instruction
            self._del_insn(ir_addr)

    def put_insn(self, insn_or_insn_list): 

        get_data = lambda insn: insn.serialize() if isinstance(insn, Insn) else insn

        if isinstance(insn_or_insn_list, list):

            # store instructions list
            for insn in insn_or_insn_list: self._put_insn(get_data(insn))

        else:

            # store single IR instruction
            self._put_insn(get_data(insn_or_insn_list))      

    def to_file(self, path):

        with open(path, 'w') as fd:

            # dump all instructions as json
            for insn in self: fd.write(InsnJson().to_json(insn) + '\n')

    def from_file(self, path):

        with open(path) as fd:        
        
            # load instructions from json
            for data in fd: self._put_insn(InsnJson().from_json(data))

    def to_storage(self, other):

        other.clear()

        ret = 0      
        for insn in self: 

            other.put_insn(insn)
            ret += 1

        return ret

    def from_storage(self, other):

        return other.to_storage(self)

    def fix_inums_and_flags(self):

        addr, prev, inum = None, None, 0
        updated, deleted, ir_addr_map = [], [], {}

        for insn in self:
            
            if addr != insn.addr:

                # end of machine instruction
                addr, inum = insn.addr, 0

                if prev is not None and not prev.has_flag(IOPT_ASM_END):

                    # set end of asm instruction flag for previous insn
                    prev.set_flag(IOPT_ASM_END)
                    updated.append(prev.serialize())

            if insn.inum != inum:

                deleted.append(( insn.addr, insn.inum ))

                # remember old inum
                ir_addr_map[( insn.addr, insn.inum )] = ( insn.addr, inum )

                # update inum value for current instruction
                insn.inum = inum
                updated.append(insn.serialize())

            inum += 1
            prev = insn

        # commit instructions changes
        for ir_addr in deleted: self._del_insn(ir_addr)
        for insn in updated: self._put_insn(insn)

        for insn in self:            

            # check for I_JMP that needs to be updated
            if insn.op != I_JCC or insn.c.type != A_LOC or \
               insn.has_flag(IOPT_CALL) or insn.has_flag(IOPT_RET): continue

            if not ir_addr_map.has_key(insn.c.val):

                continue  

            # update jump location value for current instruction
            insn.c.val = ir_addr_map[insn.c.val]
            self._put_insn(insn.serialize())


class TestCodeStorageMem(unittest.TestCase):

    arch = ARCH_X86

    def setUp(self):

        import translator
        self.tr = translator.Translator(self.arch)

        from pyopenreil.utils import asm
        self.asm = asm.Compiler(self.arch)
        
        self.storage = CodeStorageMem(self.arch)                

    def test(self):            

        # add test data to the storage
        self.storage.put_insn(self.tr.to_reil(self.asm.compile('nop'), addr = 0L))
        self.storage.put_insn(self.tr.to_reil(self.asm.compile('ret'), addr = 1L))

        assert self.storage.size() == 6
        assert len(self.storage.get_insn(0)) == 1
        assert len(self.storage.get_insn(1)) == 5

        assert self.storage.get_insn(( 0, 0 )).op == I_NONE
        assert self.storage.get_insn(( 1, 0 )).op == I_STR

        # delete IR instruction
        self.storage.del_insn(( 1, 0 ))
        self.storage.fix_inums_and_flags()

        assert self.storage.size() == 5
        assert len(self.storage.get_insn(1)) == 4

        # delete machine instruction
        self.storage.del_insn(1)
        self.storage.fix_inums_and_flags()

        assert self.storage.size() == 1
        
        try: self.storage.get_insn(1)
        except StorageError as e: assert e.addr == 1

        # delete all
        self.storage.clear()

        assert self.storage.size() == 0


class CodeStorageTranslator(CodeStorage):

    # validate instructions list returned by translator
    DEBUG = True

    #
    # OpenREIL (as well as VEX) uses the least significant bit of 
    # address to determinate encoding mode for ARM instruction:
    # 0 -- it's regular ARM instruction, 1 -- it's Thumb instruction.
    #
    _arm_addr_decode = lambda self, addr: ( addr & 0xfffffffffffffffe, addr & 1 )

    # Thumb enable helper
    arm_thumb = lambda self, addr: arm_thumb(addr)

    translator_postprocess = []

    class CFGraphBuilderFunc(CFGraphBuilder):

        def process_node(self, bb, state, context):

            self.func.add_bb(bb)

        def traverse(self, arch, ir_addr):

            self.func = Func(arch, ir_addr)

            CFGraphBuilder.traverse(self, ir_addr)

            self.func.sort()

    def __init__(self, reader = None, storage = None):        

        arch = None        

        # determinate target architecture
        if reader is not None: arch = reader.arch
        elif storage is not None: arch = storage.arch
        else: raise Error('Storage or reader instance must be specified')

        import translator
        
        log_mask, log_path = log_get()
        log_mask = log_mask if log_mask is not None else translator.LOG_MASK_DEFAULT

        self.translator = translator.Translator(arch, log_path = log_path, \
                                                      log_mask = log_mask)

        self.translator_postprocess = [ self._postprocess_cjmp, 
                                        self._postprocess_xchg,
                                        self._postprocess_unknown ]        
        self.arch = get_arch(arch)        
        self.storage = CodeStorageMem(arch) if storage is None else storage
        self.reader = reader

    ''' is_valid_arg(), is_valid_insn() and is_valid_insn_list() methods are
    used to validate IR instructions format. Any exception here indicates 
    that invalid instruction was generated by libopenreil. '''

    def is_valid_arg(self, arg):

        _is_num = lambda item: isinstance(item, (int, long))
        _is_str = lambda item: isinstance(item, basestring)
        _is_tup = lambda item: isinstance(item, tuple) 

        assert arg.size in [ None, U1, U8, U16, U32, U64 ]

        if arg.type == A_REG or arg.type == A_TEMP:

            assert _is_num(arg.size)
            assert _is_str(arg.name)

            if arg.type == A_REG:

                assert arg.name.find('R_') == 0

            elif arg.type == A_TEMP:

                assert arg.name.find('V_') == 0

        elif arg.type == A_CONST:

            assert _is_num(arg.size)
            assert _is_num(arg.val)

        elif arg.type == A_LOC:

            assert _is_tup(arg.val) and len(arg.val) == 2
            assert _is_num(arg.val[0]) and _is_num(arg.val[1])

        elif arg.type == A_NONE: pass

        else: assert False

    def is_valid_insn(self, insn):

        _is_none = lambda arg: arg.type == A_NONE
        _is_temp = lambda arg: arg.type == A_TEMP or arg.type == A_REG
        _is_const = lambda arg: arg.type == A_CONST
        _is_loc = lambda arg: arg.type == A_LOC        

        # check for valid arguments
        self.is_valid_arg(insn.a)
        self.is_valid_arg(insn.b)
        self.is_valid_arg(insn.c)

        # check for valid arguments for each opcode
        if insn.op == I_NONE:

            assert _is_none(insn.a)
            assert _is_none(insn.b)
            assert _is_none(insn.c)

        elif insn.op == I_UNK:

            assert _is_none(insn.a) or _is_temp(insn.a)
            assert _is_none(insn.b)
            assert _is_none(insn.c) or _is_temp(insn.c)

        elif insn.op in [ I_STR, I_LDM, I_NOT, I_NEG ]:

            assert _is_temp(insn.a) or _is_const(insn.a)
            assert _is_none(insn.b)
            assert _is_temp(insn.c)

        elif insn.op == I_STM:

            assert _is_temp(insn.a) or _is_const(insn.a)
            assert _is_none(insn.b)
            assert _is_temp(insn.c) or _is_const(insn.c)

        elif insn.op in [ I_EQ,  I_LT, \
                          I_ADD, I_SUB, I_SHL, I_SHR, \
                          I_MUL, I_DIV, I_MOD, I_SMUL, I_SDIV, I_SMOD, \
                          I_AND, I_OR,  I_XOR ]:

            assert _is_temp(insn.a) or _is_const(insn.a)
            assert _is_temp(insn.b) or _is_const(insn.b)
            assert _is_temp(insn.c) 

        elif insn.op == I_JCC:

            assert _is_temp(insn.a) or _is_const(insn.a)
            assert _is_none(insn.b)
            assert _is_temp(insn.c) or _is_loc(insn.c)

        else: assert False
        
        # check for valid arguments size
        if (_is_temp(insn.a) or _is_const(insn.a)) and \
           (_is_temp(insn.b) or _is_const(insn.b)):                        

            assert insn.a.size == insn.b.size

            if insn.op in [ I_EQ, I_LT ]:

                assert insn.c.size == U1

            elif insn.op != I_OR:

                assert insn.a.size == insn.b.size == insn.c.size

        elif (_is_temp(insn.a) or _is_const(insn.a)):

            if insn.op == I_LDM:

                assert insn.a.size == U32
                assert insn.c.size != U1

            elif insn.op == I_STM:

                assert insn.a.size != U1
                assert insn.c.size == U32

            elif not insn.op in [ I_JCC, I_UNK ]:

                assert insn.a.size == insn.c.size

    def is_valid_insn_list(self, insn_list):

        first = Insn(insn_list[0])
        last = Insn(insn_list[-1])        

        # check for valid flags and attrs of first and last IR instructions
        assert first.has_attr(IATTR_BIN) and first.has_attr(IATTR_ASM)
        assert last.has_flag(IOPT_ASM_END)

        inum = 0

        for insn in insn_list:

            insn = insn if isinstance(insn, Insn) else Insn(insn)

            # check for valid instruction information
            assert insn.inum == inum
            assert insn.addr == first.addr and insn.size == first.size

            # more instruction checks
            self.is_valid_insn(insn)
            inum += 1

    def optimize(self, addr_list):

        if not isinstance(addr_list, tuple) and \
           not isinstance(addr_list, list):

            addr_list = ( addr_list, )

        if self.storage is not None:

            for addr in addr_list:

                # construct dataflow graph for given function
                dfg = DFGraphBuilder(self).traverse(addr)
                
                # run optimizations
                dfg.optimize_all(storage = self.storage)

    def _postprocess_cjmp(self, addr, insn_list):
        ''' Represent Cjmp + Jmp (libasmir artifact) as Not + Cjmp. '''

        if len(insn_list) > 2 and \
           Insn_op(insn_list[-1]) == I_JCC and \
           Insn_op(insn_list[-2]) == I_JCC:

            jcc_1 = Insn(insn_list[-2])
            jcc_2 = Insn(insn_list[-1])

            if jcc_1.a.type == A_TEMP and \
               jcc_2.a.type == A_CONST and jcc_2.a.get_val() != 0 and \
               jcc_1.c.type == A_LOC and jcc_1.c.val == ( jcc_1.addr + jcc_1.size, 0 ) and \
               jcc_2.c.type == A_LOC and jcc_2.c.val != ( jcc_2.addr + jcc_2.size, 0 ):

                # allocate new temp register
                num = int(jcc_1.a.name[2:])
                tmp = Arg(A_TEMP, jcc_1.a.size, 'V_%.2d' % (num + 1))

                insn_list = insn_list[: len(insn_list) - 2]

                insn_list.append(Insn(op = I_NOT, ir_addr = jcc_1.ir_addr(), size = jcc_1.size, 
                                      a = jcc_1.a, c = tmp).serialize())

                insn_list.append(Insn(op = I_JCC, ir_addr = jcc_2.ir_addr(),  size = jcc_2.size,
                                      a = tmp, c = jcc_2.c, attr = jcc_2.attr).serialize())

        return insn_list

    def _postprocess_xchg(self, addr, insn_list):
        ''' VEX uses loop with compare-and-swap statement to represent 
            atomic xchg operation of x86 which is total overkill:

            IRSB {
               t2 = GET:I32(24)
               t0 = LDle:I32(t2)
               t1 = GET:I32(16)
               t3 = CASle(t2::t0->t1)
               t5 = CasCmpNE32(t3,t0)
               if (t5) { PUT(68) = 0x1337:I32; exit-Boring }
               PUT(16) = t0
               PUT(68) = 0x133A:I32; exit-Boring
            }

            Here we need to convert translated REIL code to more simple form. '''        

        if self.arch != x86:

            return insn_list

        attr = Insn_attr(insn_list[0])

        # check for IR code of xchg instruction
        if not (attr.has_key(IATTR_ASM) and attr[IATTR_ASM][0] == 'xchg'):

            return insn_list

        # unserialize instructions list
        insn_list = map(lambda insn: Insn(insn), insn_list)

        # build dataflow graph for single machine instruction
        dfg = DFGraphBuilder(self).from_insn(insn_list)  

        removed = []

        def _cleanup():

            ret = 0

            for key, node in dfg.nodes.items():

                remove = True

                if isinstance(node, DFGraphEntryNode) or \
                   isinstance(node, DFGraphExitNode) or \
                   node.item.op == I_STM or node in removed:

                    continue

                # determinate if node can be removed from DFG
                for edge in node.out_edges:

                    if edge.node_to in removed:

                        continue

                    if edge.name.find('R_') == 0 or \
                       not isinstance(edge.node_to, DFGraphExitNode):

                        remove = False
                        break

                if remove:

                    removed.append(node)
                    ret += 1

            return ret

        # run cleanup untill new removed nodes available
        while _cleanup() > 0: pass            

        # get IR addresses of removed instructions
        removed = map(lambda node: node.item.ir_addr(), removed)            

        ret, inum = [], 0

        # rebuild final instructions list
        for insn in insn_list:

            if not insn.ir_addr() in removed:

                insn.inum = inum
                inum += 1

                ret.append(insn)

        # copy first and last instruction attributes
        ret[0].attr, ret[-1].attr = insn_list[0].attr, insn_list[-1].attr

        # serialize instructions list back
        return map(lambda insn: insn.serialize(), ret)

    def _postprocess_unknown(self, addr, insn_list):
        ''' Convert untranslated instruction representation into the 
            single I_NONE IR instruction and save operands information
            as it's attributes. '''

        unk_insn = Insn(I_UNK, ir_addr = ( addr, 0 ))
        unk_insn.set_flag(IOPT_ASM_END)  

        src, dst = [], []      

        for insn in insn_list:

            if Insn_inum(insn) == 0:

                attr = Insn_attr(insn)
                if attr.has_key(IATTR_ASM): unk_insn.set_attr(IATTR_ASM, attr[IATTR_ASM])
                if attr.has_key(IATTR_BIN): unk_insn.set_attr(IATTR_BIN, attr[IATTR_BIN])

            if Insn_op(insn) == I_UNK:

                args = Insn_args(insn)
                a, c = Arg(args[0]), Arg(args[2])

                if a.type != A_NONE: src.append(a.serialize())
                if c.type != A_NONE: dst.append(c.serialize())

                unk_insn.size = Insn_size(insn)

            else:

                return insn_list

        if len(src) > 0: unk_insn.set_attr(IATTR_SRC, src)
        if len(dst) > 0: unk_insn.set_attr(IATTR_DST, dst)

        return [ unk_insn.serialize() ]    

    def translate_insn(self, data, addr):        

        # generate IR instructions
        ret = self.translator.to_reil(data, addr = addr)
        
        # validate instructions returned by libopenreil
        if self.DEBUG: self.is_valid_insn_list(ret)

        for func in self.translator_postprocess:

            # perform post-translation transformation
            ret = func(addr, ret)
        
        # validate instructions after post-processing
        if self.DEBUG: self.is_valid_insn_list(ret)

        return ret

    def clear(self): 

        self.storage.clear()

    def size(self): 

        return self.storage.size()

    def get_insn(self, ir_addr):        

        ir_addr = ir_addr if isinstance(ir_addr, tuple) else (ir_addr, None)
        addr, inum = ir_addr

        try:

            # query already translated IR instructions for this address
            return self.storage.get_insn(ir_addr)

        except StorageError:

            pass

        if self.arch == arm:

            #
            # We need to clear least significant bit of instruction address 
            # in case of ARM Thumb mode.
            #
            real_addr, _ = self._arm_addr_decode(ir_addr[0])

            ir_addr = ( real_addr, inum )   

        else:     

            real_addr = addr

        if self.reader is None: raise ReadError(real_addr)

        # read instruction bytes from memory
        data = self.reader.read_insn(real_addr)

        #
        # Translate to REIL.
        # Please note, that in case of ARM we need to pass 
        # a memory address with thumb enabled/disabled bit included.
        #
        ret = InsnList()
        
        for insn in self.translate_insn(data, addr):

            # save translated instructions
            self.storage.put_insn(insn)
            ret.append(Insn(insn))

        return ret if inum is None else ret[inum]

    def put_insn(self, insn_or_insn_list):

        self.storage.put_insn(insn_or_insn_list)

    def get_bb(self, ir_addr):
        
        return CFGraphBuilder(self).get_bb(ir_addr)

    def get_func(self, ir_addr):

        cfg = self.CFGraphBuilderFunc(self)
        cfg.traverse(self.arch, ir_addr)

        return cfg.func


class TestCodeStorageTranslator(unittest.TestCase):

    arch = ARCH_X86

    def setUp(self):        

        # test assembly code
        code = ( 'xor eax, eax', 'ret' )
        
        from pyopenreil.utils import asm
        self.tr = CodeStorageTranslator(asm.Reader(self.arch, code))

    def test_init(self):

        try:

            # must raise Error if storage and reader are not set
            CodeStorageTranslator()
            assert False

        except Error: pass

        reader = ReaderRaw(ARCH_X86, '\x33\xC0\xC3')
        storage = CodeStorageMem(ARCH_X86)        

        insn_list = self.tr.get_insn(0)
        storage.put_insn(insn_list)        

        # test with storage only
        tr = CodeStorageTranslator(storage = storage)
        assert tr.arch == x86 and tr.get_insn(0) == insn_list        

        # test with reader only
        tr = CodeStorageTranslator(reader)
        assert tr.arch == x86 and tr.get_insn(0) == insn_list

        # test with reader + storage
        tr = CodeStorageTranslator(reader, storage)
        assert tr.arch == x86 and tr.get_insn(0) == insn_list    

    def test_get_insn(self):

        print '\n', self.tr.get_insn(0)

    def test_get_bb(self):

        print '\n', self.tr.get_bb(0)

    def test_get_func(self):

        print '\n', self.tr.get_func(0)    


class TestArchX86(unittest.TestCase):

    arch = ARCH_X86

    def setUp(self):        

        pass

    def test_unknown_insn(self):

        from pyopenreil.utils import asm

        def _translate(code):

            tr = CodeStorageTranslator(asm.Reader(self.arch, code))
            insn = tr.get_insn(0)

            print insn

            # check for single IR instruction
            assert len(insn) == 1
            return insn[0]

        def _check_args(args, names):

            if len(args) != len(names): return False

            for arg in args:

                if not Arg_name(arg) in names: return False

            return True

        insn = _translate(( 'int 3' ))        
        assert insn.op == I_UNK
        assert not insn.has_attr(IATTR_SRC) and not insn.has_attr(IATTR_DST)

        insn = _translate(( 'hlt' ))        
        assert insn.op == I_UNK
        assert not insn.has_attr(IATTR_SRC) and not insn.has_attr(IATTR_DST)
            
        insn = _translate(( 'nop' ))
        assert insn.op == I_NONE
        assert not insn.has_attr(IATTR_SRC) and not insn.has_attr(IATTR_DST)

        insn = _translate(( 'rdmsr' ))
        assert insn.op == I_UNK
        assert _check_args(insn.get_attr(IATTR_SRC), [ 'R_ECX' ])
        assert _check_args(insn.get_attr(IATTR_DST), [ 'R_EAX', 'R_EDX' ])
        
        insn = _translate(( 'wrmsr' ))
        assert insn.op == I_UNK
        assert _check_args(insn.get_attr(IATTR_SRC), [ 'R_ECX', 'R_EAX', 'R_EDX' ])
        assert not insn.has_attr(IATTR_DST)

        insn = _translate(( 'rdtsc' ))        
        assert insn.op == I_UNK
        assert _check_args(insn.get_attr(IATTR_DST), [ 'R_EAX', 'R_EDX' ])
        assert not insn.has_attr(IATTR_SRC)

        insn = _translate(( 'cpuid' ))
        assert insn.op == I_UNK
        assert _check_args(insn.get_attr(IATTR_SRC), [ 'R_EAX', 'R_ECX' ])
        assert _check_args(insn.get_attr(IATTR_DST), [ 'R_EAX', 'R_EBX', 'R_ECX', 'R_EDX' ])

        insn = _translate(( 'sidt [ecx]' ))
        assert insn.op == I_UNK
        assert _check_args(insn.get_attr(IATTR_SRC), [ 'R_ECX' ])
        assert _check_args(insn.get_attr(IATTR_DST), [ 'R_IDT' ])

        insn = _translate(( 'sgdt [ecx]' ))
        assert insn.op == I_UNK
        assert _check_args(insn.get_attr(IATTR_SRC), [ 'R_ECX' ])
        assert _check_args(insn.get_attr(IATTR_DST), [ 'R_GDT' ])

        insn = _translate(( 'sldt [ecx]' ))
        assert insn.op == I_UNK
        assert _check_args(insn.get_attr(IATTR_SRC), [ 'R_ECX' ])
        assert _check_args(insn.get_attr(IATTR_DST), [ 'R_LDT' ])

        insn = _translate(( 'lidt [ecx]' ))
        assert insn.op == I_UNK
        assert _check_args(insn.get_attr(IATTR_SRC), [ 'R_ECX', 'R_IDT' ])
        assert not insn.has_attr(IATTR_DST)

        insn = _translate(( 'lgdt [ecx]' ))
        assert insn.op == I_UNK
        assert _check_args(insn.get_attr(IATTR_SRC), [ 'R_ECX', 'R_GDT' ])
        assert not insn.has_attr(IATTR_DST)

        insn = _translate(( 'lldt [ecx]' ))
        assert insn.op == I_UNK
        assert _check_args(insn.get_attr(IATTR_SRC), [ 'R_ECX', 'R_LDT' ])
        assert not insn.has_attr(IATTR_DST)

    def test_seg_access(self):

        from pyopenreil.utils import asm

        code = ( 'mov edi, dword ptr fs:[0x30]', 
                 'mov esi, dword ptr fs:[ecx]',
                 'mov dword ptr fs:[0x30], edx', 
                 'mov dword ptr fs:[ecx], edx', 
                 'ret' )

        tr = CodeStorageTranslator(asm.Reader(self.arch, code))
        bb = tr.get_bb(0)

        print bb

        # get symbolic expressions for given bb
        sym = bb.to_symbolic(temp_regs = False)

        fs_base = SymVal('R_FS_BASE', U32)
        
        assert sym.get(SymVal('R_EDI', U32)) == SymPtr(fs_base + SymConst(0x30, U32))
        assert sym.get(SymVal('R_ESI', U32)) == SymPtr(fs_base + SymVal('R_ECX', U32))

        assert sym.get(SymPtr(fs_base + SymConst(0x30, U32))) == SymVal('R_EDX', U32)
        assert sym.get(SymPtr(fs_base + SymVal('R_ECX', U32))) == SymVal('R_EDX', U32)

    def test_xchg(self):

        from pyopenreil.utils import asm

        code = ( 'xchg dword ptr [esp], edx', 
                 'ret' )

        tr = CodeStorageTranslator(asm.Reader(self.arch, code))
        bb = tr.get_bb(0)

        print bb

        # get symbolic expressions for given bb
        sym = bb.to_symbolic(temp_regs = False)

        assert sym.get(SymVal('R_EDX', U32)) == SymPtr(SymVal('R_ESP', U32))
        assert SymPtr(SymVal('R_ESP', U32)) == sym.get(SymVal('R_EDX', U32))


class TestArchArm(unittest.TestCase):

    arch = ARCH_ARM

    def setUp(self):        

        pass

    def _code_optimization(self, tr, addr):
 
        # construct dataflow graph for given function
        dfg = DFGraphBuilder(tr).traverse(addr)
        
        # run available optimizations
        dfg.optimize_all(storage = tr.storage)  

    def test_asm_thumb(self):

        from pyopenreil.utils import asm

        code = (
            'push    {r7}',
            'cmp     r0, #0',
            'ittee   eq',
            'moveq   r1, #1', # if r0 == 0
            'moveq   r2, #1', # if r0 == 0
            'movne   r1, #0', # if r0 != 0
            'movne   r2, #0', # if r0 != 0
            'pop     {r7}',
            'mov     pc, lr' )

        reader = asm.Reader(self.arch, code, thumb = True)
        tr = CodeStorageTranslator(reader)

        print repr(reader.data)
        print tr.get_func(arm_thumb(0))

        self._code_optimization(tr, arm_thumb(0))     

        print tr.get_func(arm_thumb(0))

    def test_asm_arm(self):

        from pyopenreil.utils import asm
        
        code = (
            'push    {r7}',
            'cmp     r0, #0',
            'movseq  r1, #1', # if r0 == 0
            'pop     {r7}',
            'mov     pc, lr' )
        
        reader = asm.Reader(self.arch, code)
        tr = CodeStorageTranslator(reader)   

        print repr(reader.data)
        print tr.get_func(0)
        
        code = (
            'push    {r7}',
            'cmp     r0, #0',
            'moveq   r1, #1', # if r0 == 0
            'moveq   r2, #1', # if r0 == 0
            'movne   r1, #0', # if r0 != 0
            'movne   r2, #0', # if r0 != 0
            'pop     {r7}',
            'mov     pc, lr' )

        reader = asm.Reader(self.arch, code)
        tr = CodeStorageTranslator(reader)   

        print repr(reader.data)
        print tr.get_func(0)
        
        self._code_optimization(tr, 0)        

        print tr.get_func(0)

if __name__ == '__main__':

    unittest.main(verbosity = 2)

#
# EoF
#
