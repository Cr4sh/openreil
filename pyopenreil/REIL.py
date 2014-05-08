from abc import ABCMeta, abstractmethod
import translator

IOPT_CALL   = 0x00000001
IOPT_RET    = 0x00000002
IOPT_BB_END = 0x00000004

MAX_INST_LEN = 30

def create_globals(items, prefix):

    num = 0
    for it in items:

        globals()[prefix + str(it)] = num
        num += 1

instructions = [
    'NONE', 'JCC', 
    'STR', 'STM', 'LDM', 
    'ADD', 'SUB', 'NEG', 'MUL', 'DIV', 'MOD', 'SMUL', 'SDIV', 'SMOD', 
    'SHL', 'SHR', 'ROL', 'ROR', 'AND', 'OR', 'XOR', 'NOT',
    'EQ', 'NEQ', 'L', 'LE', 'SL', 'SLE', 
    'CAST_L', 'CAST_H', 'CAST_U', 'CAST_S' ]

arguments = [ 'NONE', 'REG', 'TEMP', 'CONST' ]
sizes = [ '1', '8', '16', '32', '64' ]

create_globals(instructions, 'I_')
create_globals(arguments, 'A_')
create_globals(sizes, 'U')

class Arg:

    def __init__(self, t = None, size = None, name = None, val = None):

        self.type = A_NONE if t is None else t
        self.size = None if size is None else size
        self.name = None if name is None else name
        self.val = 0L if val is None else long(val)

    def get_val(self):

        mkval = lambda mask: self.val & mask

        if self.size == U1: return 0 if mkval(0x1) == 0 else 1
        elif self.size == U8: return mkval(0xff)
        elif self.size == U16: return mkval(0xffff)
        elif self.size == U32: return mkval(0xffffffff)
        elif self.size == U64: return mkval(0xffffffffffffffff)

    def __str__(self):

        mkstr = lambda val: '%s:%s' % (val, sizes[self.size])

        if self.type == A_NONE: return ''
        elif self.type == A_REG: return mkstr(self.name)
        elif self.type == A_TEMP: return mkstr(self.name)
        elif self.type == A_CONST: return mkstr('%x' % self.get_val())

    def __repr__(self):

        return self.__str__()

    def unserialize(self, data):

        if len(data) == 3:
            
            self.size = data[1]
            if self.size > len(sizes) - 1:

                raise(Exception('Invalid operand size'))

            value = data[2]
            self.type = data[0]

            if self.type == A_REG: self.name = value
            elif self.type == A_TEMP: self.name = value
            elif self.type == A_CONST: self.val = value
            else: raise(Exception('Invalid operand type'))

        elif len(data) == 0:

            self.type = A_NONE
            self.size = self.name = None 
            self.val = 0L

        else: raise(Exception('Invalid operand'))

    def is_var(self):

        # check for temporary or target architecture register
        return self.type == A_REG or self.type == A_TEMP


# raw translated REIL instruction parsing
Insn_addr  = lambda insn: insn[0][0] # instruction virtual address
Insn_size  = lambda insn: insn[0][1] # assembly code size
Insn_inum  = lambda insn: insn[1]    # IR subinstruction number
Insn_op    = lambda insn: insn[2]    # operation code
Insn_args  = lambda insn: insn[3]    # tuple with 3 arguments
Insn_flags = lambda insn: insn[4]    # instruction flags

class Insn:    

    def __init__(self, op = None, a = None, b = None, c = None):

        serialized = None
        if isinstance(op, tuple): 
            
            serialized = op
            op = None

        self.addr, self.inum, self.ir_addr = 0L, 0, ()
        self.op = I_NONE if op is None else op
        self.a = Arg() if a is None else a
        self.b = Arg() if b is None else b
        self.c = Arg() if c is None else c

        if serialized: self.unserialize(serialized)

    def __str__(self):

        return '%.8x.%.2x %7s %16s, %16s, %16s' % \
               (self.addr, self.inum, instructions[self.op], \
                self.a, self.b, self.c)

    def unserialize(self, data):

        self.addr, self.size = Insn_addr(data), Insn_size(data) 
        self.inum, self.flags = Insn_inum(data), Insn_flags(data)
        self.ir_addr = (self.addr, self.inum)

        self.op = Insn_op(data)
        if self.op > len(instructions) - 1:

            raise(Exception('Invalid operation code'))

        args = Insn_args(data) 
        if len(args) != 3: raise(Exception('Invalid arguments'))

        self.a.unserialize(args[0])
        self.b.unserialize(args[1])
        self.c.unserialize(args[2])
        return self

    def have_flag(self, val):

        return self.flags & val != 0

    def dst(self):

        ret = []

        if self.op != I_JCC and self.op != I_STM and \
           self.c.is_var(): ret.append(self.c)

        return ret

    def src(self):

        ret = []
        
        if self.a.is_var(): ret.append(self.a)
        if self.b.is_var(): ret.append(self.b)

        if (self.op == I_JCC or self.op == I_STM) and \
           self.c.is_var(): ret.append(self.c)

        return ret


class InsnReader:

    __metaclass__ = ABCMeta

    @abstractmethod
    def read(self, addr, size): pass

    @abstractmethod
    def read_insn(self, addr): pass


class RawInsnReader(InsnReader):

    def __init__(self, data, addr = 0L):

        self.addr = addr
        self.data = data
        InsnReader.__init__(self)

    def read(self, addr, size): 

        if addr < self.addr or \
           addr >= self.addr + len(self.data): return None

        addr -= self.addr        
        return self.data[addr : addr + size]

    def read_insn(self, addr): 

        return self.read(addr, MAX_INST_LEN)

        
class InsnStorage:

    __metaclass__ = ABCMeta

    @abstractmethod
    def query(self, addr): pass

    @abstractmethod
    def store(self, insn): pass


class InsnStorageMemory(InsnStorage):

    class InsnIterator:

        def __init__(self, storage):

            self.items = storage.items            
            self.addr, self.inum = 0, 0

            self.keys = self.items.keys()
            self.keys.sort()

        def next(self):

            while True:

                if self.addr >= len(self.keys): 

                    # last assembly instruction
                    raise(StopIteration)

                insn = self.items[self.keys[self.addr]]
                if self.inum >= len(insn): 

                    # last IR instruction
                    self.addr, self.inum = self.addr + 1, 0
                    continue

                ret = insn[insn.keys()[self.inum]]
                self.inum += 1

                return ret

    def __init__(self): 

        self.clear()

    def __iter__(self):

        return self.InsnIterator(self)

    def clear(self):

        self.items = {}

    def to_file(self, path):

        with open(path, 'w') as fd:

            # dump all instructions to the text file
            for insn in self: fd.write(str(insn) + '\n')

    def from_file(self, path):

        with open(path) as fd:        
        
            for line in fd:

                line = eval(line.strip())
                if isinstance(line, tuple): self.store(line)
    
    def query(self, addr): 

        if isinstance(addr, tuple): addr, inum = addr
        else: inum = None

        # get IR insts for assembly inst at given address
        try: insn_list = self.items[addr]
        except KeyError: return None
        
        if inum is not None: 

            # return specified IR instruction
            try: return insn_list[inum]
            except KeyError: return None
        
        else: 

            keys = insn_list.keys()
            keys.sort()

            # return all IR instructions
            return map(lambda inum: insn_list[inum], keys)

    def store(self, insn_or_insn_list): 

        if isinstance(insn_or_insn_list, tuple):

            # store single IR instruction
            addr = Insn_addr(insn_or_insn_list)
            inum = Insn_inum(insn_or_insn_list)
            
            try: insn_list = self.items[addr]
            except KeyError: insn_list = self.items[addr] = {}
            
            insn_list[inum] = insn_or_insn_list

        else:

            insn = insn_or_insn_list[0]
            addr = Insn_addr(insn)

            # save instructions block for specified address
            insn_list = self.items[addr] = {}
            for insn in insn_or_insn_list: insn_list[Insn_inum(insn)] = insn


class AsmInsn:

    class Iterator:

        def __init__(self, insn_list):

            self.insn_list = insn_list
            self.keys = insn_list.keys()
            self.keys.sort()
            self.current = 0

        def next(self):

            try: ret = self.insn_list[self.keys[self.current]]
            except IndexError: raise(StopIteration)

            self.current += 1
            return ret

    def __init__(self, insn_list):

        self.insn_list = {}
        self.insn_list.update( \
            map(lambda insn: ( Insn_inum(insn), insn ), insn_list))

        self.addr = Insn_addr(insn_list[0])
        self.size = Insn_size(insn_list[0])        

        test_flag = lambda v: bool(Insn_flags(self[-1]) & v)
        
        self.is_bb_end = test_flag(IOPT_BB_END)
        self.is_call = test_flag(IOPT_CALL)
        self.is_ret = test_flag(IOPT_RET)  

    def __iter__(self):

        return self.Iterator(self.insn_list)

    def __getitem__(self, i):

        keys = self.insn_list.keys()
        keys.sort()
        
        return self.insn_list[keys[i]]

    def __str__(self):

        return '\n'.join(map(lambda insn: str(Insn(insn)), self))

    def _get_reference(self, fn, t = None):

        ret = {}
        for insn in self:

            insn = Insn(insn)
            for var in fn(insn): 

                if t is None or var.type == t: 

                    if not ret.has_key(var.name): ret[var.name] = []
                    ret[var.name].append(insn.inum)                    

        return ret

    def _get_unused(self, t = None):

        ret = []
        var_def = self.get_def(t = t)
        var_use = self.get_use(t = t)
        for var_name in var_def:

            if not var_use.has_key(var_name): ret.append(var_name)

        return ret

    def _get_branch_addr(self):

        for insn in self:
            
            if Insn_op(insn) == I_JCC:

                insn = Insn(insn)
                if insn.c.type == A_CONST:

                    addr = insn.c.get_val()
                    if addr != insn.addr + insn.size: return addr

        return None

    def get_def(self, t = None):

        ret = self._get_reference(lambda insn: insn.dst(), t)
        for var_name in ret:

            if len(ret[var_name]) > 1:

                raise(Exception('Multiple assignments of %s in instruction %x' % 
                                (var_name, self.addr)))

        return ret

    def get_use(self, t = None):

        return self._get_reference(lambda insn: insn.src(), t)

    def get_def_reg(self): return self.get_def(t = A_REG)
    def get_use_reg(self): return self.get_use(t = A_REG)

    def get_successors(self):

        insn = Insn(self[-1])
        lhs, rhs = insn.addr + insn.size, None

        # check for unconditional jump or return
        if self.is_ret or \
           (insn.op == I_JCC and insn.a.type == A_CONST and insn.inum == 0):

           lhs = None
           if insn.c.type == A_CONST: rhs = insn.c.get_val()

        else:

            # handle conditional branch
            rhs = self._get_branch_addr()

        return lhs, rhs    

    def eliminate_defs(self, def_list, temp_cleanup = False):

        killed = []
        unused = def_list if temp_cleanup else self._get_unused()

        if def_list:

            # remove IR instructions that assigns specified variables
            for var_name in def_list:

                if var_name not in unused:

                    raise(Exception('Error while eliminating defs of ' +
                                    '%s for instruction %x: variable is in use' %
                                    (var_name, self.addr)))

                var_def = self.get_def()
                if var_def.has_key(var_name):

                    self.insn_list.pop(var_def[var_name][0])

            # recursive cleanup of unused variables
            self.eliminate_defs(self._get_unused(t = A_TEMP), temp_cleanup = True)


class InsnTranslator(translator.Translator):

    def __init__(self, arch, reader, storage = None):

        self.reader = reader
        self.storage = storage

        translator.Translator.__init__(self, arch)

    def query(self, addr):

        if self.storage is not None: return self.storage.query(addr)
        else: return None

    def store(self, insn):

        if self.storage is not None: self.storage.store(insn)

    def get_insn(self, addr):

        # try to return already translated code
        insn_list = self.query(addr)
        if insn_list is not None: return insn_list

        if self.reader is None: 

            raise(Exception('Unable to read instruction ' + hex(addr)))

        # read instruction bytes from memory
        data = self.reader.read_insn(addr)
        if data is None: return None

        # translate to REIL and save results
        insn_list = self.to_reil(data, addr = addr)
        self.store(insn_list)
        
        return AsmInsn(insn_list)


class CfgNode():

    def __init__(self, insn_list):

        self.insn_list = {}
        self.insn_list.update( \
            map(lambda insn: ( Insn_inum(insn), insn ), insn_list))

        self.start = self[0].addr
        self.end = self[-1].addr        

    def __iter__(self):

        return AsmInsn.Iterator(self.insn_list)

    def __getitem__(self, i):

        keys = self.insn_list.keys()
        keys.sort()

        return self.insn_list[keys[i]]

    def __str__(self):

        return '\n'.join(map(lambda insn: str(insn), self))

    def get_successors(self):

        return self[-1].get_successors()


class BasicBlockTranslator(InsnTranslator):

    def get_bb(self, addr):

        # translate first instruction
        ret = [ self.get_insn(addr) ]
        insn = ret[-1]

        # translate until the end of basic block
        while not insn.is_bb_end:

            addr += insn.size
            insn = self.get_insn(addr)

            if insn is None: return CfgNode(ret)
            else: ret.append(insn)

        return CfgNode(ret)

        
class FuncTranslator(BasicBlockTranslator):    

    def cfg_traverse_pre_order(self, addr):

        stack, visited = [], []
        stack_top = addr

        def stack_push(bb_start): 

            for node in visited:
    
                # skip processed basic block
                if bb_start == node.start: return
 
            stack.append(bb_start) 

        while True:

            # translate basic block at given address
            node = self.get_bb(stack_top)
            visited.append(node)            

            # inspect child nodes
            lhs, rhs = node.get_successors()
            if rhs is not None: stack_push(rhs)
            if lhs is not None: stack_push(lhs)

            try: stack_top = stack.pop()
            except IndexError: break       

        return visited

    def get_func(self, addr):

        # iterative CFG traversal
        return self.cfg_traverse_pre_order(addr)


class Translator(FuncTranslator): 

    pass

