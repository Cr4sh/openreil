from abc import ABCMeta, abstractmethod
from sets import Set

# REIL constants
from IR import *

# supported arhitectures
from arch import x86


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

        return { 'x86': x86 }[arch]

    except KeyError: 

        raise translator.BaseError('Architecture %s is unknown' % arch)


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

        items = [ 'I_' + REIL_NAMES_INSN[self.op] ]
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


ARG_TYPE = 0
ARG_SIZE = 1
ARG_NAME = 2
ARG_VAL  = 2

Arg_type = lambda arg: arg[ARG_TYPE] # argument type (see REIL_ARG)
Arg_size = lambda arg: arg[ARG_SIZE] # argument size (see REIL_SIZE)
Arg_name = lambda arg: arg[ARG_NAME] # argument name (for A_REG and A_TEMP)
Arg_val  = lambda arg: arg[ARG_VAL]  # argument value (for A_CONST)

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

    def size_name(self):

        return REIL_NAMES_SIZE[self.size]

    def __str__(self):

        mkstr = lambda val: '%s:%s' % (val, self.size_name())

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
      
            self.type, self.size = Arg_type(data), Arg_size(data)

            if self.size not in [ U1, U8, U16, U32, U64 ]:

                return False
            
            if self.type == A_REG: self.name = Arg_name(data)
            elif self.type == A_TEMP: self.name = Arg_name(data)
            elif self.type == A_CONST: self.val = Arg_val(data)
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

INSN_ADDR = 0
INSN_INUM = 1
INSN_OP   = 2
INSN_ARGS = 3
INSN_ATTR = 4

INSN_ADDR_ADDR = 0
INSN_ADDR_SIZE = 1

# raw translated REIL instruction parsing
Insn_addr  = lambda insn: insn[INSN_ADDR][INSN_ADDR_ADDR]   # instruction virtual address
Insn_size  = lambda insn: insn[INSN_ADDR][INSN_ADDR_SIZE]   # assembly code size
Insn_inum  = lambda insn: insn[INSN_INUM]   # IR subinstruction number
Insn_op    = lambda insn: insn[INSN_OP]     # operation code
Insn_args  = lambda insn: insn[INSN_ARGS]   # tuple with 3 arguments
Insn_attr  = lambda insn: insn[INSN_ATTR]   # instruction attributes

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

        if ir_addr is not None:

            self.addr, self.inum = ir_addr

        self.a = Arg() if a is None else a
        self.b = Arg() if b is None else b
        self.c = Arg() if c is None else c        

        # unserialize raw IR instruction structure
        if serialized: self.unserialize(serialized)

    def __str__(self):

        return '%.8x.%.2x %7s %16s, %16s, %16s' % \
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

    def args(self):

        return self.src() + self.dst()

    def dst(self, get_all = False):

        ret = []

        if self.op != I_NONE: 

            if get_all: cond = lambda arg: arg.type != A_NONE 
            else: cond = lambda arg: arg.is_var()

            if self.op != I_JCC and self.op != I_STM and \
               cond(self.c): ret.append(self.c)

        elif self.have_attr(IATTR_DST):

            # get operands information from attributes
            ret = map(lambda a: Arg(a), self.get_attr(IATTR_DST))

        return ret

    def src(self, get_all = False):

        ret = []

        if self.op != I_NONE:

            if get_all: cond = lambda arg: arg.type != A_NONE 
            else: cond = lambda arg: arg.is_var()

            if cond(self.a): ret.append(self.a)
            if cond(self.b): ret.append(self.b)

            if (self.op == I_JCC or self.op == I_STM) and \
               cond(self.c): ret.append(self.c)

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

        if self.have_attr(IATTR_NEXT):

            # force to use next instruction that was set inattributes
            return self.get_attr(IATTR_NEXT)

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

    def clone(self):

        return Insn(self.serialize())

    def eliminate(self):

        self.op, self.args = I_NONE, {}
        self.a = Arg(A_NONE)
        self.b = Arg(A_NONE)
        self.c = Arg(A_NONE)        

        self.set_flag(IOPT_ELIMINATED)


class InsnList(list):

    def __str__(self):

        return '\n'.join(map(lambda insn: str(insn), self))


class BasicBlock(object):
    
    def __init__(self, insn_list):

        self.insn_list = insn_list
        self.first, self.last = insn_list[0], insn_list[-1]
        self.ir_addr = self.first.ir_addr()
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

    def eliminate_dead_code(self):

        pass


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

        return '%s %s' % (self.key(), self.item.op_name())

    def key(self):

        return self.item.ir_addr()


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

        self.deleted_nodes = Set()

    def store(self, storage):

        addr_list = Set()
        for node in self.nodes.values() + list(self.deleted_nodes): 

            insn = node.item
            if insn is not None:                

                # collect list of available machine instructions including deleted
                addr_list = addr_list.union([ insn.addr ])

        for addr in addr_list:

            # delete all IR for collected instructions
            try: storage.del_insn(addr)
            except StorageError: pass

        relink = False
        for node in self.nodes.values():

            insn = node.item
            if insn is not None:                
            
                # put each node instruction into the storage
                storage.put_insn(insn.serialize())
                relink = True

        # update inums and flags
        if relink: storage.relink()

        relink = False
        for node in self.deleted_nodes:

            insn = node.item

            # For CFG consistence we need to insert I_NONE
            # instructions instead eliminated ones.
            try: storage.get_insn(( insn.addr, 0 ))
            except StorageError: 

                insn = insn.clone()
                insn.inum = 0

                insn.eliminate()
                storage.put_insn(insn.serialize())
                relink = True

        # update inums and flags
        if relink: storage.relink()
        
    def constant_folding(self, storage = None):

        deleted_nodes = []

        from VM import Math

        def evaluate(insn): 

            val = Math(insn.a, insn.b).eval(insn.op)
            if val is not None:

                return Arg(A_CONST, insn.c.size, val = val)

            else:

                return None

        def need_to_propagate(insn):

            if insn.op == I_JCC: return False

            for arg in insn.src(get_all = True):

                if arg.type != A_CONST: return False

            for arg in insn.dst(get_all = True):

                if arg.type != A_TEMP: return False

            return True

        def propagate(node):

            val = evaluate(node.item)
            if val is None: 

                return False

            for edge in node.out_edges:

                node = edge.node_to
                insn = node.item

                print 'Updating arg %s of DFG node "%s" to %s' % (edge, node, val)

                if insn.a.name == edge.name: insn.a = val
                if insn.b.name == edge.name: insn.b = val

            return True        

        print '*** Folding constants...'

        while True:

            deleted, pending = 0, []            

            for node in self.nodes.values():

                if node != self.entry_node and len(node.in_edges) == 0 and need_to_propagate(node.item):

                    pending.append(node)

            for node in pending:

                print 'DFG node "%s" has no input edges' % node
                
                if propagate(node):

                    # delete node that has no output edges                    
                    self.del_node(node)

                    deleted_nodes.append(node)
                    deleted += 1

            if deleted == 0: 

                # no more nodes to delete
                break

        self.deleted_nodes = self.deleted_nodes.union(deleted_nodes)

        if storage is not None: self.store(storage)

    def eliminate_dead_code(self, storage = None):

        deleted_nodes = []

        print '*** Eliminating dead code...'

        # check for variables that live at the end of the function
        for edge in list(self.exit_node.in_edges):

            dst = edge.node_from.item.dst()
            arg = dst[0] if len(dst) > 0 else None

            if arg is None: continue
            
            if (arg.type == A_TEMP) or \
               (arg.type == A_REG and arg.name in x86.Registers.flags):

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

        self.deleted_nodes = self.deleted_nodes.union(deleted_nodes)

        if storage is not None: self.store(storage)


class DFGraphBuilder(CFGraphBuilder):

    def process_node(self, bb, state, context):

        dfg = context

        for insn in bb:

            src = [ arg.name for arg in insn.src() ]
            dst = [ arg.name for arg in insn.dst() ]

            node = dfg.add_node(insn)

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

                dfg.add_edge(node_from, node, arg)

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

    reader = None

    @abstractmethod
    def get_insn(self, addr, inum = None): pass

    @abstractmethod
    def put_insn(self, insn_or_insn_list): pass

    @abstractmethod
    def size(self): pass

    @abstractmethod
    def clear(self): pass


class CodeStorageMem(CodeStorage):    

    def __init__(self, arch, insn_list = None): 

        self.clear()
        self.arch = get_arch(arch)
        if insn_list is not None: self.put_insn(insn_list)

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
            if insn.have_flag(IOPT_ASM_END): break
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

            # dump all instructions to the text file
            for insn in self: fd.write(str(insn.serialize()) + '\n')

    def from_file(self, path):

        with open(path) as fd:        
        
            for line in fd:

                line = eval(line.strip())
                if isinstance(line, tuple): self._put_insn(line)  

    def relink(self):

        items = {}
        addr, prev, inum = None, None, 0

        for insn in self:
            
            if addr != insn.addr:

                # end of machine instruction
                addr, inum = insn.addr, 0

                if prev is not None: 

                    # set end of asm instruction flag for previous insn
                    prev.set_flag(IOPT_ASM_END)

                    key_prev = ( prev.addr, prev.inum )
                    items[key_prev] = prev.serialize()

            # update inum value for each instruction
            insn.inum = inum

            key_insn = ( insn.addr, insn.inum )
            items[key_insn] = insn.serialize()

            inum += 1
            prev = insn

        self.items = items


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

        import translator
        
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

    def clear(self): 

        self.storage.clear()

    def size(self): 

        return self.storage.size()

    def get_insn(self, ir_addr):

        ir_addr = ir_addr if isinstance(ir_addr, tuple) else (ir_addr, None)
        ret = InsnList()

        try: 

            # query already translated IR instructions for this address
            return self.storage.get_insn(ir_addr)

        except StorageError:

            if self.reader is None: raise ReadError(ir_addr[0])

            # read instruction bytes from memory
            data = self.reader.read_insn(ir_addr[0])
            if data is None: raise ReadError(ir_addr[0])

            # translate to REIL
            ret = self.translate_insn(data, ir_addr[0])

        # save to storage
        for insn in ret: self.storage.put_insn(insn)
        return self.storage.get_insn(ir_addr)

    def put_insn(self, insn_or_insn_list):

        self.storage.put_insn(insn_or_insn_list)

    def get_bb(self, ir_addr):

        cfg = CFGraphBuilder(self)
        
        return InsnList(cfg.get_bb(ir_addr).insn_list)

    def get_func(self, ir_addr):

        cfg = self._CFGraphBuilder(self)
        cfg.traverse(ir_addr)

        return InsnList(cfg.insn_list)
#
# EoF
#
