from abc import ABCMeta, abstractmethod
from sets import Set

IATTR_FLAGS  = 'F'
IATTR_SRC    = 'S'
IATTR_DST    = 'D'

IOPT_CALL    = 0x00000001
IOPT_RET     = 0x00000002
IOPT_BB_END  = 0x00000004
IOPT_ASM_END = 0x00000008

MAX_INST_LEN = 30

def create_globals(items, prefix):

    num = 0
    for it in items:

        globals()[prefix + str(it)] = num
        num += 1

REIL_INSN = [ 'NONE', 'JCC', 
              'STR', 'STM', 'LDM', 
              'ADD', 'SUB', 'NEG', 'MUL', 'DIV', 'MOD', 'SMUL', 'SDIV', 'SMOD', 
              'SHL', 'SHR', 'SAL', 'SAR', 'ROL', 'ROR', 
              'AND', 'OR', 'XOR', 'NOT',
              'EQ', 'NEQ', 'L', 'LE', 'SL', 'SLE', 
              'CAST_L', 'CAST_H', 'CAST_U', 'CAST_S' ]

REIL_SIZE = [ '1', '8', '16', '32', '64' ]

REIL_ARG = [ 'NONE', 'REG', 'TEMP', 'CONST' ]

create_globals(REIL_INSN, 'I_')
create_globals(REIL_SIZE, 'U')
create_globals(REIL_ARG, 'A_')


import translator
from arch import x86


class ReadError(translator.BaseError):

    def __init__(self, addr):

        self.addr = addr

    def __str__(self):

        return 'Error while reading instruction %s' % hex(self.addr)


class ParseError(translator.BaseError):

    def __str__(self):

        return 'Error while deserializing instruction %s' % hex(self.addr)


def get_arch(arch):

    try: 

        return { 'x86': x86 }[arch]

    except KeyError: 

        raise(translator.BaseError('Architecture %s is unknown' % arch))


class SymVal(object):

    def __init__(self, val, size):

        self.val = val
        self.size = size

    def __str__(self):

        return str(self.val)

    def __eq__(self, other):

        if type(other) == SymAny: return True
        if type(other) != SymVal: return False

        return self.val == other.val

    def __ne__(self, other):

        return not self == other

    def __hash__(self):

        return hash(self.val)

    def __add__(self, other): 

        return self.to_exp(I_ADD, other)    

    def __sub__(self, other): 

        return self.to_exp(I_SUB, other)    

    def __mul__(self, other): 

        return self.to_exp(I_MUL, other)

    def __mod__(self, other): 

        return self.to_exp(I_MOD, other)    

    def __div__(self, other): 

        return self.to_exp(I_DIV, other)        

    def __and__(self, other): 

        return self.to_exp(I_AND, other)    

    def __xor__(self, other): 

        return self.to_exp(I_XOR, other)    

    def __or__(self, other): 

        return self.to_exp(I_OR, other)  

    def __lshift__(self, other): 

        return self.to_exp(I_SHL, other)    

    def __rshift__(self, other): 

        return self.to_exp(I_SHR, other) 

    def __invert__(self): 

        return self.to_exp(I_NOT)    

    def __neg__(self): 

        return self.to_exp(I_NEG)      

    def to_exp(self, op, arg = None):

        return SymExp(op, self, arg)  


class SymAny(SymVal):

    def __init__(self): 

        pass

    def __str__(self):

        return '@'

    def __eq__(self, other):

        return True


class SymPtr(SymVal):

    def __init__(self, val): 

        self.val = val

    def __str__(self):

        return '*' + str(self.val)

    def __eq__(self, other):

        if type(other) == SymAny: return True
        if type(other) != SymPtr: return False

        return self.val == other.val

    def __hash__(self):

        return ~hash(self.val)


class SymConst(SymVal):

    def __str__(self):

        return '0x%x' % self.val

    def __eq__(self, other):

        if type(other) == SymAny: return True
        if type(other) != SymConst: return False

        return self.val == other.val


class SymExp(SymVal):

    commutative = ( I_ADD, I_SUB, I_AND, I_XOR, I_OR )

    def __init__(self, op, a, b = None):

        self.op, self.a, self.b = op, a, b

    def __str__(self):

        items = [ 'I_' + REIL_INSN[self.op] ]
        if self.a is not None: items.append(str(self.a))
        if self.b is not None: items.append(str(self.b))

        return '(%s)' % ' '.join(items)

    def __eq__(self, other):

        if type(other) == SymAny: return True
        if type(other) != SymExp: return False

        if self.op == other.op and self.op in self.commutative:

            # equation for commutative operations
            return (self.a == other.a and self.b == other.b) or \
                   (self.b == other.a and self.a == other.b)

        return self.op == other.op and \
               self.a == other.a and self.b == other.b

    def __hash__(self):

        return hash(self.op) + hash(self.a) + hash(self.a)


class SymState(object):

    def __init__(self, other = None):

        if other is None: self.clear()
        else: self.items = other.items.copy()

    def __getitem__(self, n):

        return self.items[n]

    def __str__(self):

        return '\n'.join(map(lambda k: '%s: %s' % (k, self.items[k]), self.items))

    def clear(self):

        self.items = {}

    def update(self, val, exp):

        self.items[val] = exp

    def update_mem(self, val, exp):

        self.update(SymPtr(self.items[val]), exp)

    def clone(self):

        return SymState(self)


class Arg(object):

    def __init__(self, t = None, size = None, name = None, val = None):

        serialized = None
        if isinstance(t, tuple): 
            
            serialized = t
            t = None

        self.type = A_NONE if t is None else t
        self.size = None if size is None else size
        self.name = None if name is None else name
        self.val = 0L if val is None else long(val)

        # unserialize raw IR instruction argument structure
        if serialized: self.unserialize(serialized)

    def get_val(self):

        mkval = lambda mask: long(self.val & mask)

        if self.size == U1:    return 0 if mkval(0x1) == 0 else 1
        elif self.size == U8:  return mkval(0xff)
        elif self.size == U16: return mkval(0xffff)
        elif self.size == U32: return mkval(0xffffffff)
        elif self.size == U64: return mkval(0xffffffffffffffff)

    def __str__(self):

        mkstr = lambda val: '%s:%s' % (val, REIL_SIZE[self.size])

        if self.type == A_NONE:    return ''
        elif self.type == A_REG:   return mkstr(self.name)
        elif self.type == A_TEMP:  return mkstr(self.name)
        elif self.type == A_CONST: return mkstr('%x' % self.get_val())

    def serialize(self):

        if self.type in [ A_REG, A_TEMP ]: return self.type, self.size, self.name
        elif self.type == A_NONE: return ()
        else: return self.type, self.size, self.val

    def unserialize(self, data):

        if len(data) == 3:

            value = data[2]            
            self.type, self.size = data[0], data[1]            

            if self.size not in [ U1, U8, U16, U32, U64 ]:

                return False
            
            if self.type == A_REG: self.name = value
            elif self.type == A_TEMP: self.name = value
            elif self.type == A_CONST: self.val = value
            else: 

                return False

        elif len(data) == 0:

            self.type = A_NONE
            self.size = self.name = None 
            self.val = 0L

        else: return False

        return True

    def is_var(self):

        # check for temporary or target architecture register
        return self.type == A_REG or self.type == A_TEMP


# raw translated REIL instruction parsing
Insn_addr  = lambda insn: insn[0][0] # instruction virtual address
Insn_size  = lambda insn: insn[0][1] # assembly code size
Insn_inum  = lambda insn: insn[1]    # IR subinstruction number
Insn_op    = lambda insn: insn[2]    # operation code
Insn_args  = lambda insn: insn[3]    # tuple with 3 arguments
Insn_attr  = lambda insn: insn[4]    # instruction attributes

class Insn(object):    

    ATTR_DEFS = (( IATTR_FLAGS, 0 ), # optional REIL flags
                 )

    class IRAddr(tuple):

        def __str__(self):

            return '%.x.%.2x' % self

    def __init__(self, op = None, attr = None, size = None, ir_addr = None, 
                       a = None, b = None, c = None):

        serialized = None
        if isinstance(op, tuple): 
            
            serialized = op
            op = None                

        self.init_attr(attr)

        self.op = I_NONE if op is None else op        
        self.size = 0 if size is None else size        

        self.addr, self.inum = 0L, 0
        self.ir_addr = ()

        if ir_addr is not None:

            self.ir_addr = ir_addr
            self.addr, self.inum = ir_addr

        self.a = Arg() if a is None else a
        self.b = Arg() if b is None else b
        self.c = Arg() if c is None else c        

        # unserialize raw IR instruction structure
        if serialized: self.unserialize(serialized)

    def __str__(self):

        return '%.8x.%.2x %7s %16s, %16s, %16s' % \
               (self.addr, self.inum, REIL_INSN[self.op], \
                self.a, self.b, self.c)    

    def serialize(self):

        info = ( self.addr, self.size )
        args = ( self.a.serialize(), self.b.serialize(), self.c.serialize() )
        
        return ( info, self.inum, self.op, args, self.attr )

    def unserialize(self, data):

        self.init_attr(Insn_attr(data))
        self.addr, self.inum, self.size = Insn_addr(data), Insn_inum(data), Insn_size(data)
        self.ir_addr = self.IRAddr(( self.addr, self.inum ))

        self.op = Insn_op(data)
        if self.op > len(REIL_INSN) - 1: 

            raise(ParseError(self.addr))

        args = Insn_args(data) 
        if len(args) != 3: 

            raise(ParseError(self.addr))

        if not self.a.unserialize(args[0]) or \
           not self.b.unserialize(args[1]) or \
           not self.c.unserialize(args[2]): 

           raise(ParseError(self.addr))

        return self

    def init_attr(self, attr):

        self.attr = {} if attr is None else attr

        # initialize missing attributes with default values
        for name, val in self.ATTR_DEFS:

            if not self.have_attr(name): self.set_attr(name, val)

    def get_attr(self, name):

        return self.attr[name]

    def set_attr(self, name, val):

        self.attr[name] = val

    def have_attr(self, name):

        return self.attr.has_key(name)

    def set_flag(self, val):

        self.set_attr(IATTR_FLAGS, self.get_attr(IATTR_FLAGS) | val)

    def have_flag(self, val):

        return self.get_attr(IATTR_FLAGS) & val != 0    

    def dst(self):

        ret = []

        if self.op != I_NONE: 

            if self.op != I_JCC and self.op != I_STM and \
               self.c.is_var(): ret.append(self.c)

        elif self.have_attr(IATTR_DST):

            # get operands information from attributes
            ret = map(lambda a: Arg(a), self.get_attr(IATTR_DST))

        return ret

    def src(self):

        ret = []

        if self.op != I_NONE: 
        
            if self.a.is_var(): ret.append(self.a)
            if self.b.is_var(): ret.append(self.b)

            if (self.op == I_JCC or self.op == I_STM) and \
               self.c.is_var(): ret.append(self.c)

        elif self.have_attr(IATTR_SRC):

            # get operands information from attributes
            ret = map(lambda a: Arg(a), self.get_attr(IATTR_SRC))

        return ret

    def to_symbolic(self, in_state = None):

        # copy input state to output state
        out_state = SymState() if in_state is None else in_state.clone()

        # skip instructions that doesn't update output state
        if self.op in [ I_JCC, I_NONE ]: return out_state

        def _to_symbolic_arg(arg):

            if arg.type == A_REG or arg.type == A_TEMP:

                # register value
                arg = SymVal(arg.name, arg.size)

                try: return out_state[arg]
                except KeyError: return arg

            elif arg.type == A_CONST:

                # constant value
                return SymConst(arg.get_val(), arg.size)

            else: return None

        # convert source arguments to symbolic expressions
        a = _to_symbolic_arg(self.a)
        b = _to_symbolic_arg(self.b)
        c = SymVal(self.c.name, self.c.size)

        # constant argument should always be second
        if type(a) == SymConst and type(b) == SymVal: a, b = b, a

        # move from one register to another
        if self.op == I_STR: out_state.update(c, a)

        # memory read
        elif self.op == I_LDM: out_state.update(c, SymPtr(a))

        # memory write
        elif self.op == I_STM: out_state.update_mem(c, a)

        # expression
        else: out_state.update(c, a.to_exp(self.op, b))

        return out_state

    def next(self):

        if self.have_flag(IOPT_RET): 

            # end of function
            return None

        elif self.op == I_JCC and \
             self.a.type == A_CONST and self.a.get_val() != 0 and \
             not self.have_flag(IOPT_CALL):

            # unconditional jump
            return None

        elif self.have_flag(IOPT_ASM_END):

            # go to first IR instruction of next assembly instruction
            return self.addr + self.size, 0

        else:

            # go to next IR instruction of current assembly instruction
            return self.addr, self.inum + 1

    def jcc_loc(self):

        if self.op == I_JCC and self.c.type == A_CONST: return self.c.get_val(), 0
        return None


class BasicBlock(object):
    
    def __init__(self, insn_list):

        self.insn_list = insn_list
        self.first, self.last = insn_list[0], insn_list[-1]
        self.ir_addr = self.first.ir_addr
        self.size = self.last.addr + self.last.size - self.ir_addr[0]

    def __iter__(self):

        for insn in self.insn_list: yield insn

    def __str__(self):

        return '\n'.join(map(lambda insn: str(insn), self))

    def get_successors(self):

        return self.last.next(), self.last.jcc_loc()


class GraphNode(object):

    def __init__(self, item = None):

        self.item = item
        self.in_edges, self.out_edges = Set(), Set()


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

    def __init__(self):

        self.nodes, self.edges = {}, Set()

    def node(self, key):

        return self.nodes[key]

    def add_node(self, item):

        node = item if isinstance(item, self.NODE) else self.NODE(item)
        key = node.key()

        try: return self.nodes[key]        
        except KeyError: self.nodes[key] = node

        return node

    def del_node(self, item):

        node = item if isinstance(item, self.NODE) else self.NODE(item)

        edges = Set()
        edges.union_update(node.in_edges)
        edges.union_update(node.out_edges)

        # delete node edges
        for edge in edges: self.del_edge(edge)

        # delete node
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

        # delete edge
        self.edges.remove(edge)

    def to_dot_file(self, path):

        with open(path, 'w') as fd:

            fd.write('digraph pyopenreil {\n')

            for edge in self.edges:

                name, attr = str(edge), {}
                if len(name) > 0: 

                    attr['label'] = '"%s"' % name

                attr = ' '.join(map(lambda a: '%s=%s' % a, attr.items()))
                data = '"%s" -> "%s" [%s];\n' % (str(edge.node_from), str(edge.node_to), attr)

                fd.write(data)

            fd.write('}\n')


class CFGraphNode(GraphNode):    

    def __str__(self):

        return '%x.%.2x' % self.item.ir_addr

    def key(self):

        return self.item.ir_addr


class CFGraphEdge(GraphEdge):

    def __str__(self):

        return ''


class CFGraph(Graph):    

    NODE = CFGraphNode
    EDGE = CFGraphEdge


class CFGraphBuilder(object):

    def __init__(self, storage):

        self.arch = storage.arch
        self.storage = storage

    def process_node(self, bb, state, context): 

        return True

    def get_insn(self, ir_addr):

        return self.storage.get_insn(ir_addr)    

    def _get_bb(self, addr):

        insn_list = []
        
        while True:

            # translate single assembly instruction
            insn_list += self.get_insn(addr)
            insn = insn_list[-1]

            # check for basic block end
            if insn.have_flag(IOPT_BB_END): break

            addr += insn.size

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
            if insn.have_flag(IOPT_BB_END): 

                return BasicBlock(insn_list[inum:last])

    def traverse(self, ir_addr, state = None, context = None):

        stack, nodes, edges = [], [], []
        cfg = CFGraph()

        ir_addr = ir_addr if isinstance(ir_addr, tuple) else (ir_addr, None)        
        state = {} if state is None else state        

        def _process_node(bb, state, context):

            if bb.ir_addr not in nodes: nodes.append(bb.ir_addr)
            
            return self.process_node(bb, state, context)

        def _process_edge(edge):

            if edge not in edges: edges.append(edge)

        # iterative pre-order CFG traversal
        while True:

            # query IR for basic block
            bb = self.get_bb(ir_addr)
            cfg.add_node(bb)

            if not _process_node(bb, state, context): return False

            # process immediate postdominators
            lhs, rhs = bb.last.next(), bb.last.jcc_loc()
            if rhs is not None and not rhs in nodes: 

                _process_edge(( bb.ir_addr, rhs ))
                stack.append(( rhs, state.copy() ))

            if lhs is not None: 

                _process_edge(( bb.ir_addr, lhs ))
                stack.append(( lhs, state.copy() ))
            
            try: ir_addr, state = stack.pop()
            except IndexError: break

        # add processed edges to the CFG
        for edge in edges: cfg.add_edge(*edge)
            
        return cfg


class DFGraphNode(GraphNode):    

    def __str__(self):

        return '%s %s' % (self.key(), REIL_INSN[self.item.op])

    def key(self):

        return self.item.ir_addr


class DFGraphEntryNode(DFGraphNode): 

    class Label(tuple): 

        def __str__(self): return 'ENTRY'

    def __str__(self):

        return str(self.key())

    def key(self):

        return self.Label(( None, 0L ))


class DFGraphExitNode(DFGraphNode): 

    class Label(tuple): 

        def __str__(self): return 'EXIT'

    def __str__(self):

        return str(self.key())

    def key(self):

        return self.Label(( None, 1L ))


class DFGraphEdge(GraphEdge):

    def __str__(self):

        return self.name


class DFGraph(Graph):    

    NODE = DFGraphNode
    EDGE = DFGraphEdge

    def __init__(self):

        super(DFGraph, self).__init__()

        self.entry_node = DFGraphEntryNode()
        self.exit_node = DFGraphExitNode()        

        self.add_node(self.entry_node)
        self.add_node(self.exit_node)


class DFGraphBuilder(CFGraphBuilder):

    def process_node(self, bb, state, context):

        dfg = context

        for insn in bb:

            src = [ arg.name for arg in insn.src() ]
            dst = [ arg.name for arg in insn.dst() ]

            if insn.have_flag(IOPT_CALL):

                #
                # Function call instruction.
                #
                # To make all the things a bit more simpler we assuming that:
                #
                #   - target function can read and write all general purpose registers;
                #   - target function is not using any flag values that was set in current function;
                #
                # Normally this approach will works fine for code that was generated by high level
                # language compiler, but some handwritten assembly junk can break it.
                #
                src = dst = self.arch.Registers.general

            for arg in src:

                # propagate register usage information to immediate dominator
                try: node_from = dfg.add_node(state[arg])
                except KeyError: node_from = dfg.entry_node                      

                dfg.add_edge(node_from, dfg.add_node(insn), arg)

            # update current state
            for arg in dst: state[arg] = insn

        if bb.get_successors() == ( None, None ):

            # end of the function, make exit edges for current state
            for arg_name, insn in state.items():

                dfg.add_edge(dfg.add_node(insn), dfg.exit_node.key(), arg_name)

        return True
    
    def traverse(self, ir_addr):        

        dfg = DFGraph()
        super(DFGraphBuilder, self).traverse(ir_addr, state = {}, context = dfg)

        return dfg


class Reader(object):

    __metaclass__ = ABCMeta

    @abstractmethod
    def read(self, addr, size): pass

    @abstractmethod
    def read_insn(self, addr): pass


class ReaderRaw(Reader):

    def __init__(self, data, addr = 0L):

        self.addr = addr
        self.data = data

        super(ReaderRaw, self).__init__()

    def read(self, addr, size): 

        if addr < self.addr or \
           addr >= self.addr + len(self.data): return None

        addr -= self.addr        
        return self.data[addr : addr + size]

    def read_insn(self, addr): 

        return self.read(addr, MAX_INST_LEN)


class CodeStorage(object):

    __metaclass__ = ABCMeta

    @abstractmethod
    def get_insn(self, addr, inum = None): pass

    @abstractmethod
    def put_insn(self, insn_or_insn_list): pass


class CodeStorageMem(CodeStorage):

    def __init__(self, arch, insn_list = None): 

        self.clear()
        self.arch = get_arch(arch)
        if insn_list is not None: self.put_insn(insn_list)

    def __iter__(self):

        keys = self.items.keys()
        keys.sort()

        for k in keys: yield Insn(self.items[k])    

    def _get_key(self, insn):

        return Insn_addr(insn), Insn_inum(insn)

    def _put_insn(self, insn):

        self.items[self._get_key(insn)] = insn

    def clear(self):

        self.items = {}

    def to_file(self, path):

        with open(path, 'w') as fd:

            # dump all instructions to the text file
            for insn in self: fd.write(str(insn.serialize()) + '\n')

    def from_file(self, path):

        with open(path) as fd:        
        
            for line in fd:

                line = eval(line.strip())
                if isinstance(line, tuple): self._put_insn(line)
    
    def get_insn(self, ir_addr): 

        ir_addr = ir_addr if isinstance(ir_addr, tuple) else (ir_addr, None)
        addr, inum = ir_addr

        query_single, ret = True, []

        if inum is None: 

            inum = 0
            query_single = False

        while True:

            # query single IR instruction
            insn = Insn(self.items[(addr, inum)])
            if query_single: return insn

            next = insn.next()
            ret.append(insn)

            # stop on assembly instruction end
            if insn.have_flag(IOPT_ASM_END): break
            inum += 1

        return ret

    def put_insn(self, insn_or_insn_list): 

        if isinstance(insn_or_insn_list, list):

            # store instructions list
            for insn in insn_or_insn_list: self._put_insn(insn)

        else:

            # store single IR instruction
            self._put_insn(insn_or_insn_list)


class CodeStorageTranslator(CodeStorage):

    class _CFGraphBuilder(CFGraphBuilder):

        def __init__(self, storage):

            self.insn_list, self.visited = [], [] 

            CFGraphBuilder.__init__(self, storage)

        def process_node(self, bb, state, context):

            if bb.ir_addr not in self.visited:
            
                self.insn_list += bb.insn_list
                self.visited.append(bb.ir_addr)

            return True

    def __init__(self, arch, reader = None, storage = None):
        
        self.arch = get_arch(arch)
        self.translator = translator.Translator(arch)
        self.storage = CodeStorageMem(arch) if storage is None else storage
        self.reader = reader

    def translate_insn(self, data, addr):                

        src, dst = [], []
        ret_insn = Insn(I_NONE, ir_addr = ( addr, 0 ))
        ret_insn.set_flag(IOPT_ASM_END)        

        # generate IR instructions
        ret = self.translator.to_reil(data, addr = addr)

        #
        # Convert untranslated instruction representation into the 
        # single I_NONE IR instruction and save operands information
        # into it's attributes.
        #
        for insn in ret:

            if Insn_op(insn) == I_NONE:

                args = Insn_args(insn)
                a, c = Arg(args[0]), Arg(args[2])

                if a.type != A_NONE: src.append(a.serialize())
                if c.type != A_NONE: dst.append(c.serialize())

                ret_insn.size = Insn_size(insn)

            else:

                return ret

        if len(src) > 0: ret_insn.set_attr(IATTR_SRC, src)
        if len(dst) > 0: ret_insn.set_attr(IATTR_DST, dst)

        return [ ret_insn.serialize() ]

    def get_insn(self, ir_addr):

        ir_addr = ir_addr if isinstance(ir_addr, tuple) else (ir_addr, None)
        ret = []

        try: 

            # query already translated IR instructions for this address
            return self.storage.get_insn(ir_addr)

        except KeyError:

            if self.reader is None: raise(ReadError(ir_addr[0]))

            # read instruction bytes from memory
            data = self.reader.read_insn(ir_addr[0])
            if data is None: raise(ReadError(ir_addr[0]))

            # translate to REIL
            ret = self.translate_insn(data, ir_addr[0])

        # save to storage
        for insn in ret: self.storage.put_insn(insn)
        return self.storage.get_insn(ir_addr)

    def put_insn(self, insn_or_insn_list):

        self.storage.put_insn(insn_or_insn_list)

    def get_bb(self, ir_addr):

        cfg = CFGraphBuilder(self)
        
        return cfg.get_bb(ir_addr).insn_list

    def get_func(self, ir_addr):

        cfg = self._CFGraphBuilder(self)
        cfg.traverse(ir_addr)

        return cfg.insn_list
#
# EoF
#
