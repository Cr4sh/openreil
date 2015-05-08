from REIL import *

class Sym(object):

    def __init__(self):

        pass    

    def __ne__(self, other):

        return not self == other

    def __add__(self, other): 

        return self.to_exp(I_ADD, other)

    def __radd__(self, other): 

        return other.to_exp(I_ADD, self)

    def __sub__(self, other): 

        return self.to_exp(I_SUB, other)    

    def __rsub__(self, other): 

        return other.to_exp(I_SUB, self)    

    def __mul__(self, other): 

        return self.to_exp(I_MUL, other)

    def __rmul__(self, other): 

        return other.to_exp(I_MUL, self)

    def __mod__(self, other): 

        return self.to_exp(I_MOD, other)    

    def __rmod__(self, other): 

        return other.to_exp(I_MOD, self)

    def __div__(self, other): 

        return self.to_exp(I_DIV, other)        

    def __rdiv__(self, other): 

        return other.to_exp(I_DIV, self)   

    def __and__(self, other): 

        return self.to_exp(I_AND, other)    

    def __rand__(self, other): 

        return other.to_exp(I_AND, self)   

    def __xor__(self, other): 

        return self.to_exp(I_XOR, other)    

    def __rxor__(self, other): 

        return other.to_exp(I_XOR, self)    

    def __or__(self, other): 

        return self.to_exp(I_OR, other)  

    def __ror__(self, other): 

        return other.to_exp(I_OR, self)  

    def __lshift__(self, other): 

        return self.to_exp(I_SHL, other)    

    def __rlshift__(self, other): 

        return other.to_exp(I_SHL, self) 

    def __rshift__(self, other): 

        return self.to_exp(I_SHR, other) 

    def __rrshift__(self, other): 

        return other.to_exp(I_SHR, self) 

    def __invert__(self): 

        return self.to_exp(I_NOT)    

    def __neg__(self): 

        return self.to_exp(I_NEG)      

    def to_exp(self, op, arg = None):

        return SymExp(op, self, arg)      

    def parse(self, visitor):

        return visitor(self)


class SymAny(Sym):

    def __str__(self):

        return '@'

    def __eq__(self, other):

        return True

    def __ne__(self, other):

        return False

    def __hash__(self):

        return hash(str(self))


class SymVal(Sym):

    def __init__(self, name, size = None, is_temp = False):

        self.name, self.size = name, size
        self.is_temp = is_temp

    def __str__(self):        

        return self.name

    def __eq__(self, other):

        if type(other) == SymAny: return True
        if type(other) != SymVal: return False

        return self.name == other.name

    def __hash__(self):

        return hash(self.name)


class SymPtr(Sym):

    def __init__(self, val, size = None):

        self.val, self.size = val, size

    def __str__(self):

        return '*' + str(self.val)

    def __eq__(self, other):

        if type(other) == SymAny: return True
        if type(other) != SymPtr: return False

        return self.val == other.val

    def __hash__(self):

        return ~hash(self.val)

    def parse(self, visitor):

        self.val = self.val.parse(visitor)

        return visitor(self)


class SymConst(Sym):

    def __init__(self, val, size = None):

        self.val, self.size = val, size

    def __str__(self):

        return '0x%x' % self.val

    def __eq__(self, other):

        if type(other) == SymAny: return True
        if type(other) != SymConst: return False

        return self.val == other.val

    def __hash__(self):

        return hash(self.val)


class SymIRAddr(Sym):

    def __init__(self, addr, inum):

        self.addr, self.inum = addr, inum

    def __str__(self):

        return '%.8x.%.2x' % (self.addr, self.inum)

    def __eq__(self, other):

        if type(other) == SymAny: return True
        if type(other) != SymIRAddr: return False

        return self.addr == other.addr and self.inum == other.inum

    def __hash__(self):

        return hash(self.addr) ^ hash(self.inum)


class SymIP(Sym):

    def __str__(self):

        return '@IP'

    def __eq__(self, other):

        return type(other) == SymAny or type(other) == SymIP

    def __hash__(self):

        return hash(str(self))


class SymCond(Sym):

    def __init__(self, cond, true, false):

        self.cond, self.true, self.false = cond, true, false

    def __str__(self):

        return '(%s) ? %s : %s' % (str(self.cond), \
               str(self.true), str(self.false))

    def __eq__(self, other):

        if type(other) == SymAny: return True
        if type(other) != SymCond: return False

        return self.cond == other.cond and \
               self.true == other.true and \
               self.false == other.false

    def __hash__(self):

        return hash(self.cond) ^ hash(self.true) ^ hash(self.false)

    def parse(self, visitor):        
        
        self.cond = self.cond.parse(visitor)
        self.true = self.true.parse(visitor)
        self.false = self.false.parse(visitor)

        return visitor(self)


class SymExp(Sym):

    commutative = ( I_ADD, I_SUB, I_AND, I_XOR, I_OR )

    def __init__(self, op, a, b = None):
        
        self.op, self.a, self.b = op, a, b

    def __str__(self):

        op_str = { I_ADD:   '+', I_SUB:   '-', I_NEG:   '-', 
                   I_MUL:   '*', I_DIV:   '/', I_MOD:   '%', 
                   I_SMUL: '@*', I_SDIV: '@/', I_SMOD: '@%', 
                   I_SHL:  '<<', I_SHR:  '>>', I_AND:   '&',
                   I_OR:    '|', I_XOR:   '^', I_NOT:   '~',
                   I_EQ:   '==', I_LT:    '<' }[self.op]

        if self.b is not None:

            return '(' + str(self.a) + ' ' + op_str + ' ' + str(self.b) + ')'

        else:

            return op_str + str(self.a)

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

        return hash(self.op) ^ hash(self.a) ^ hash(self.b)

    def parse(self, visitor):        
        
        if self.a is not None: self.a = self.a.parse(visitor)
        if self.b is not None: self.b = self.b.parse(visitor)

        return visitor(self)


class TestSymExp(unittest.TestCase):    

    def test(self):             

        a, b = SymVal('R_EAX', U32), SymVal('R_ECX', U32)

        assert a != b
        assert a == SymAny()
        assert a == SymVal('R_EAX', U32)

        # check commutative operands
        assert a + b == b + a
        assert a - b == b - a
        assert a & b == b & a
        assert a | b == b | a
        assert a ^ b == b ^ a

        # check non-commutative operands
        assert a * b != b * a
        assert a % b != b % a
        assert a / b != b / a
        assert a << b != b << a
        assert a >> b != b >> a


class SymState(object):

    def __init__(self, other = None):

        if other is None: self.clear()
        else: self.state = [] + other.state

    def __getitem__(self, target_val):

        try:

            # get expression by value name
            return next(exp for val, exp in self.state if val == target_val)

        except StopIteration:

            raise KeyError()

    def __setitem__(self, val, exp):

        for i in range(len(self.state)):

            current_val, current_exp = self.state[i]
            if current_val == val:

                self.state[i] = ( val, exp )
                return

        self.state.append(( val, exp ))

    def __iter__(self):

        for val, exp in self.state: yield val, exp

    def __str__(self):

        return '\n'.join(map(lambda item: '%s = %s' % item, self.state))

    def clear(self, val = None):

        if val is not None:

            for i in range(len(self.state)):

                current_val, current_exp = self.state[i]
                if current_val == val:

                    # remove single variable from state
                    self.state.pop(i)
                    return

        else:

            # clear state
            self.state = []

    def get(self, val):

        try: 

            return self[val]

        except KeyError: 

            return None

    def query(self, val):

        ret = self.get(val)

        return val if ret is None else ret 

    def arg_in(self):

        ret = []

        def visitor(val):

            if isinstance(val, SymVal) or \
               isinstance(val, SymPtr):

                if not val in ret: ret.append(val)

            return val

        # enumerate available expressions
        for val, exp in self:

            exp.parse(visitor)

        return ret

    def arg_out(self):

        return [ val for val, _ in self.state ]

    def update(self, val, exp):

        self[val] = exp

    def update_mem_r(self, val, exp, size):

        self.update(val, SymPtr(exp, size))

    def update_mem_w(self, val, exp, size):

        self.update(SymPtr(self.query(val), size), exp)

    def clone(self):

        return SymState(self)

    def remove_temp_regs(self):

        for val in self.arg_out():

            if isinstance(val, SymVal) and val.is_temp: 

                self.clear(val)

    def slice(self, val_in = None, val_out = None):

        prepare_val = lambda val: SymVal(val) if isinstance(val, basestring) else val

        val_in = [] if val_in is None else val_in
        val_out = [] if val_out is None else val_out        

        val_in = map(prepare_val, val_in)
        val_out = map(prepare_val, val_out)

        for val, exp in self:

            if len(val_out) > 0 and not val in val_out: 

                # remove expression of unnecessary output value
                self.clear(val)
                continue

            if len(val_in) > 0:

                class ValueFound(Exception): pass

                def _visitor(e):

                    if e in val_in: raise ValueFound()

                try: 

                    exp.parse(_visitor)

                    # Expression is not using given input values, 
                    # remove it from state.
                    self.clear(val)

                except ValueFound: pass                

#
# EoF
#
