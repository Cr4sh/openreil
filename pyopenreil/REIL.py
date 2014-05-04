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
    'SHL', 'SHR', 'ROL', 'ROR', 
    'AND', 'OR', 'XOR', 'NOT', 'BAND', 'BOR', 'BXOR', 'BNOT',
    'EQ', 'L', 'LE', 'SL', 'SLE', 'CAST_LO', 'CAST_HI', 'CAST_U', 'CAST_S' ]

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
        elif self.type == A_CONST: return mkstr(self.get_val())

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

        self.addr, self.inum = 0L, 0
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

    def cfg_node_out_edges(self):

        lhs = rhs = None
        end = self.have_flag(IOPT_RET)

        if not self.have_flag(IOPT_CALL) and self.op == I_JCC:
       
            if self.c.type == A_CONST:

                # basic block at branch location
                rhs = self.c.get_val()

            if self.a.type == A_CONST and self.a.get_val() != 0:

                # stop on unconditional branch
                end = True

        # address of next basic block
        if not end: lhs = self.addr + self.size

        return lhs, rhs


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

    def __init__(self): 

        self.items = {}
    
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

        # return all IR instructions
        else: return [] + insn_list.values()

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

        # read instruction bytes from memory
        data = self.reader.read_insn(addr)
        if data is None: return None

        # translate to REIL and save results
        insn_list = self.to_reil(data, addr = addr)
        self.store(insn_list)
        
        return insn_list


class BasicBlockTranslator(InsnTranslator):

    def get_bb(self, addr):

        # translate first instruction
        ret = self.get_insn(addr)
        insn = ret[-1]

        # translate until the end of basic block
        while not (Insn_flags(insn) & IOPT_BB_END):

            addr += Insn_size(insn)
            insn_list = self.get_insn(addr)
            if insn_list is None: return ret

            ret += insn_list 
            insn = ret[-1]

        return ret


class FuncTranslator(BasicBlockTranslator):

    class CfgNode(tuple): pass

    def cfg_traverse_pre_order(self, addr):

        stack, visited = [], []
        stack_top = addr

        def stack_push(bb): 

            if bb is not None and not bb in visited: stack.append(bb) 

        while True:

            # translate basic block at given address
            insn_list = self.get_bb(stack_top)

            first = insn_list[0]
            last = insn_list[-1]

            visited.append(self.CfgNode(( 
                Insn_addr(first), 
                Insn_addr(last))))

            # inspect child nodes
            lhs, rhs = Insn(last).cfg_node_out_edges()
            stack_push(rhs)
            stack_push(lhs)

            try: stack_top = stack.pop()
            except IndexError: break       

        return visited

    def get_func(self, addr):   

        # iterative CFG traversal
        return self.cfg_traverse_pre_order(addr)


class Translator(FuncTranslator): 

    pass

