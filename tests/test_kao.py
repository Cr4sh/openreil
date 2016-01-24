import sys, os, random, struct, unittest
import z3

'''
This is a keygen for Kao's Toy Project crackme 
(https://tuts4you.com/download.php?view.3293). 

Detailed article that explains how it works and
how to solve it was published in my blog:

http://blog.cr4.sh/2015/03/automated-algebraic-cryptanalysis-with.html

The following code is essence of this crackme (simple stream cipher with 64-bit key):

void expand(u8 out[32], const u8 in[32], u32 x, u32 y)  
{  
    for (u32 i = 0; i < 32; ++i)  
    {  
        out[i] = (in[i] - x) ^ y;

        x = ROL(x, 1);  
        y = ROL(y, 1);  
    }  
}

It accepts 32-byte plain text as in, cipher key as x and y 32-bit unsigned integers, 
and produces 32-byte ciphered text out. Kao's Toy Project uses installation ID as plain text,
serial number entered by user as x and y, and then compares encryption results with 
hardcoded ciphered text.

We are going to use quick and dirty symbolic execution for finding valid key for
given installation ID.
'''

file_dir = os.path.abspath(os.path.dirname(__file__))
reil_dir = os.path.abspath(os.path.join(file_dir, '..'))
if not reil_dir in sys.path: sys.path = [ reil_dir ] + sys.path

from pyopenreil.REIL import *
from pyopenreil.symbolic import *
import pyopenreil.VM as VM

UNDEF = None

class Val(object):

    def __init__(self, val = UNDEF, exp = None):

        self.val, self.exp = val, exp

    def __str__(self):

        return str(self.exp) if self.is_symbolic() else hex(self.val)

    def is_symbolic(self):

        # check if value is symbolic
        return self.val is None

    def is_concrete(self):

        # check if value is concrete
        return not self.is_symbolic()

    def to_z3(self, state, size):

        #
        # generate Z3 expression that represents this value
        #

        def _z3_size(size):

            return { U1: 1, U8: 8, U16: 16, U32: 32, U64: 64 }[ size ]

        def _z3_exp(exp, size):

            if isinstance(exp, SymVal):

                if state.has_key(exp.name):

                    return state[exp.name]

                else:

                    return z3.BitVec(exp.name, _z3_size(exp.size))                                 

            elif isinstance(exp, SymConst):

                return z3.BitVecVal(exp.val, _z3_size(exp.size))                

            elif isinstance(exp, SymExp):

                a, b = exp.a, exp.b

                assert isinstance(a, SymVal) or isinstance(a, SymConst)
                assert b is None or isinstance(b, SymVal) or isinstance(b, SymConst)
                assert b is None or a.size == b.size                

                a = a if a is None else _z3_exp(a, a.size)
                b = b if b is None else _z3_exp(b, b.size)                     

                # makes 1 bit bitvectors from booleans
                _z3_bool_to_bv = lambda exp: z3.If(exp, z3.BitVecVal(1, 1), z3.BitVecVal(0, 1))

                # z3 expression from SymExp
                ret = { I_ADD: lambda: a + b,
                        I_SUB: lambda: a - b,            
                        I_NEG: lambda: -a,
                        I_MUL: lambda: a * b,
                        I_DIV: lambda: z3.UDiv(a, b),
                        I_MOD: lambda: z3.URem(a, b),
                        I_SHR: lambda: z3.LShR(a, b),
                        I_SHL: lambda: a << b,                     
                         I_OR: lambda: a | b,
                        I_AND: lambda: a & b,
                        I_XOR: lambda: a ^ b,
                        I_NOT: lambda: ~a,
                         I_EQ: lambda: _z3_bool_to_bv(a == b),
                         I_LT: lambda: _z3_bool_to_bv(z3.ULT(a, b)) }[exp.op]()

                size_src = _z3_size(exp.a.size)
                size_dst = _z3_size(size)

                if size_src > size_dst:

                    # convert to smaller value
                    return z3.Extract(size_dst - 1, 0, ret)

                elif size_src < size_dst:

                    # convert to bigger value
                    return z3.Concat(z3.BitVecVal(0, size_dst - size_src), ret)

                else:

                    return ret

            else:

                assert False    

        if self.is_concrete():

            # use concrete value
            return z3.BitVecVal(self.val, _z3_size(size))

        else:

            # build Z3 expression
            return _z3_exp(self.exp, size) 


class Mem(VM.Mem):

    class _Val(Val):

        def __init__(self, size, seed, val = UNDEF, exp = None):

            # Each byte in memory must have size and hash
            # of full value that uses it.
            self.size, self.seed = size, seed

            super(Mem._Val, self).__init__(val = val, exp = exp)            

    def pack(self, size, val):

        ret = []
        seed = random.randrange(0x10000000, 0x7fffffff)

        if val.is_symbolic():

            for i in range(self.map_length[size]):

                ret.append(self._Val(size, seed, exp = val.exp))

        else:

            ret = map(lambda val: self._Val(size, seed, val), 
                      list(super(Mem, self).pack(size, val.val)))

        return ret

    def unpack(self, size, data):

        first = data[0]
        items, symbolic = [], True

        assert len(data) == self.map_length[first.size]

        for val in data:

            # check if all bytes belongs to the same value
            assert val.size == first.size and \
                   val.seed == first.seed

            if val.is_concrete(): 

                items.append(val.val)
                symbolic = False

        if symbolic:

            return Val(exp = first.exp)

        else:

            return Val(super(Mem, self).unpack(size, ''.join(items)))

    def read(self, addr, size):

        return self._read(addr, size)

    def write(self, addr, size, data):

        self._write(addr, size, data)


class Math(VM.Math):

    def eval(self, op, a = None, b = None):

        concrete = True

        a_val = a if a is None else a.val
        b_val = b if b is None else b.val

        # determinate symbolic/concrete operation mode
        if a_val is not None and a_val.is_symbolic(): concrete = False
        if b_val is not None and b_val.is_symbolic(): concrete = False        

        if concrete:

            a_reg = a if a is None else VM.Reg(a.size, a.get_val())
            b_reg = b if b is None else VM.Reg(b.size, b.get_val())

            # compute and return concrete value
            return Val(val = super(Math, self).eval(op, a_reg, b_reg))

        else:

            assert a is not None
            assert op in [ I_STR, I_NOT ] or b is not None            

            # get symbolic representation of the arguments
            a_sym = a if a is None else a.to_symbolic()
            b_sym = b if b is None else b.to_symbolic()

            # make a symbolic expression
            exp = a_sym if op == I_STR else SymExp(op, a_sym, b_sym)

            # return symbolic value
            return Val(exp = exp)


class Reg(VM.Reg):

    def to_symbolic(self):

        # get symbolic representation of register contents
        if self.val.is_concrete():

            # use concrete value
            return SymConst(self.get_val(), self.size)

        else:

            if self.regs_map.has_key(self.name):

                return SymVal(self.regs_map[self.name], self.size)

            # use symbolic value
            return SymVal(self.name, self.size) if self.val.exp is None \
                                                else self.val.exp    

    def get_val(self):

        # get concrete value of the register if it's available
        assert self.val.is_concrete()
        
        return super(Reg, self).get_val(self.val.val)

    def str_val(self):

        return str(self.val.exp) if self.val.is_symbolic() \
                                 else super(Reg, self).str_val(self.val.val)


class Cpu(VM.Cpu):

    DEF_REG = Reg
    DEF_REG_VAL = Val()

    DEF_R_DFLAG = Val(1)

    class State(object):

        def __init__(self, regs = None, mem = None):

            self.regs, self.mem = regs, mem

    def __init__(self, arch):

        self.known_state = []
        self.regs_map, self.regs_cnt, self.regs_list = {}, {}, []        

        super(Cpu, self).__init__(arch, mem = Mem(strict = False), math = Math())

    def reg(self, name, val = None):

        reg = super(Cpu, self).reg(name, val)     
        reg.regs_map = self.regs_map

        if val is not None:

            if self.insn is None: addr = inum = 0    
            else: addr, inum = self.insn.ir_addr()
            
            cnt = 0
            cnt_key = ( reg.name, addr, inum )

            if self.regs_cnt.has_key(cnt_key):

                self.regs_cnt[cnt_key] += 1
                cnt = self.regs_cnt[cnt_key]

            else:

                self.regs_cnt[cnt_key] = 0
            
            new_name = '%s_%x_%x_%x' % (reg.name, addr, inum, cnt)
            reg_copy = super(Cpu, self).reg(new_name, reg.val, reg.size)  

            self.regs_map[reg.name] = new_name
            self.regs_list.append(reg_copy)

        return reg

    def arg(self, arg):

        if arg.type == A_CONST:

            return self.DEF_REG(arg.size, Val(arg.val))

        else:

            return super(Cpu, self).arg(arg)

    def set_ip(self, val):

        super(Cpu, self).set_ip(Val(val))

    def insn_jcc(self, insn, a, b, c):

        if a.val.is_symbolic():

            raise Exception('I_JCC with symbolic condition at ' + 
                            '%s, giving up' % str(insn.ir_addr()))

        elif c is not None and c.val.is_symbolic():

            raise Exception('I_JCC with symbolic location at ' + 
                            '%s, giving up' % str(insn.ir_addr()))
        else:

            return super(Cpu, self).insn_jcc(insn, a, b, c)

    def insn_stm(self, insn, a, b, c):

        if c.val.is_symbolic():

            raise Exception('I_STM with symbolic write address at ' + 
                            '%s, giving up' % str(insn.ir_addr()))
        else:

            # store a to memory
            self.mem.store(c.get_val(), insn.a.size, a.val)
            return None

    def insn_ldm(self, insn, a, b, c):

        if a.val.is_symbolic():

            raise Exception('I_LDM with symbolic read address at ' + 
                            '%s, giving up' % str(insn.ir_addr()))
        else:

            # read from memory to c
            self.reg(insn.c, self.mem.load(a.get_val(), insn.c.size))
            return None

    def execute(self, insn):        

        print insn.to_str()

        return super(Cpu, self).execute(insn)

    def run(self, storage, addr = 0L, stop_at = None):

        print

        super(Cpu, self).run(storage, addr = addr, stop_at = stop_at)

    def to_z3(self, state = None):

        state = {} if state is None else state      

        for reg in self.regs_list:

            # get Z3 expression for each register
            state[reg.name] = reg.val.to_z3(state, reg.size)

        return state


def keygen(kao_binary_path, kao_installation_ID):
    '''
        Assembly code of serial number check from Kao's Toy Project binary,
        X and Y contains serial number that was entered by user.

        .text:004010EC check_serial    proc near
        .text:004010EC
        .text:004010EC ciphered        = byte ptr -21h
        .text:004010EC X               = dword ptr  8
        .text:004010EC Y               = dword ptr  0Ch
        .text:004010EC
        .text:004010EC                 push    ebp
        .text:004010ED                 mov     ebp, esp
        .text:004010EF                 add     esp, 0FFFFFFDCh
        .text:004010F2                 mov     ecx, 20h
        .text:004010F7                 mov     esi, offset installation_ID
        .text:004010FC                 lea     edi, [ebp+text_ciphered]
        .text:004010FF                 mov     edx, [ebp+X]
        .text:00401102                 mov     ebx, [ebp+Y]
        .text:00401105
        .text:00401105 loc_401105:
        .text:00401105                 lodsb
        .text:00401106                 sub     al, bl
        .text:00401108                 xor     al, dl
        .text:0040110A                 stosb
        .text:0040110B                 rol     edx, 1
        .text:0040110D                 rol     ebx, 1
        .text:0040110F                 loop    loc_401105
        .text:00401111                 mov     byte ptr [edi], 0
        .text:00401114                 push    offset text_valid ; "0how4zdy81jpe5xfu92kar6cgiq3lst7"
        .text:00401119                 lea     eax, [ebp+text_ciphered]
        .text:0040111C                 push    eax
        .text:0040111D                 call    lstrcmpA
        .text:00401122                 leave
        .text:00401123                 retn    8
        .text:00401123 check_serial    endp
    '''

    # address of the check_serial() function
    check_serial = 0x004010EC       

    # address of the strcmp() call inside check_serial()
    stop_at = 0x0040111D

    # address of the global buffer with installation ID
    installation_ID = 0x004093A8        

    # load Kao's PE binary
    from pyopenreil.utils import bin_PE        
    tr = CodeStorageTranslator(bin_PE.Reader(kao_binary_path))

    # Run all available code optimizations and 
    # update storage with new the function code.
    tr.optimize(check_serial)     

    print tr.get_func(check_serial)

    # create CPU and ABI
    cpu = Cpu(ARCH_X86)
    abi = VM.Abi(cpu, tr, no_reset = True)   

    # hardcoded ciphered text constant from Kao's binary
    out_data = '0how4zdy81jpe5xfu92kar6cgiq3lst7'
    in_data = ''

    try:

        # convert installation ID into the binary form
        for s in kao_installation_ID.split('-'):
        
            in_data += struct.pack('I', int(s[:8], 16))
            in_data += struct.pack('I', int(s[8:], 16))

        assert len(in_data) == 32

    except:

        raise Exception('Invalid instllation ID string')

    # copy installation ID into the emulator's memory
    for i in range(32):

        cpu.mem.store(installation_ID + i, U8, 
            cpu.mem._Val(U8, 0, ord(in_data[i])))

    ret, ebp = 0x41414141, 0x42424242

    # create stack with symbolic arguments for check_serial()
    stack = abi.pushargs(( Val(exp = SymVal('ARG_0', U32)), \
                           Val(exp = SymVal('ARG_1', U32)) ))

    # dummy return address
    stack.push(Val(ret))

    # initialize emulator's registers
    cpu.reg('ebp', Val(ebp))
    cpu.reg('esp', Val(stack.top))

    # run until stop
    try: cpu.run(tr, check_serial, stop_at = [ stop_at ])
    except VM.CpuStop as e:            

        print 'STOP at', hex(cpu.reg('eip').get_val())
            
        # get Z3 expressions list for current CPU state
        state = cpu.to_z3()
        cpu.dump(show_all = True)                

        # read symbolic expressions for contents of the output buffer
        addr = cpu.reg('eax').val
        data = cpu.mem.read(addr.val, 32)
        
        for i in range(32):

            print '*' + hex(addr.val + i), '=', data[i].exp                  

        # create SMT solver
        solver = z3.Solver()

        for i in range(32):

            # add constraint for each output byte
            solver.add(data[i].to_z3(state, U8) == z3.BitVecVal(ord(out_data[i]), 8))
        
        # solve constraints
        print solver.check()

        # get solution
        model = solver.model()

        # get and print serial number
        serial = map(lambda d: model[d].as_long(), model.decls())
        serial[1] = serial[0] ^ serial[1]

        print '\nSerial number: %s\n' % '-'.join([ '%.8X' % serial[0], 
                                                   '%.8X' % serial[1] ])

        return serial

    assert False


class TestKao(unittest.TestCase):

    BIN_PATH = os.path.join(file_dir, 'toyproject.exe')

    INSTALLATION_ID = '97FF58287E87FB74-979950C854E3E8B3-55A3F121A5590339-6A8DF5ABA981F7CE'
    
    def test(self):

        # run keygen with the reference test data
        serial = keygen(self.BIN_PATH, self.INSTALLATION_ID)

        # check for valid result
        assert serial[0] == 0x47A8A5AA and serial[1] == 0x0EEC4C24


def main():

    if len(sys.argv) >= 2:

        keygen(TestKao.BIN_PATH, sys.argv[1])

    else:

        print 'USAGE: python test_kao.py <your_installation_ID>'

    return 0


if __name__ == '__main__':    

    exit(main())

#
# EoF
#
