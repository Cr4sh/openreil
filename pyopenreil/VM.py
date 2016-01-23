import sys, os, struct, random
import numpy

from REIL import *

class MemError(Error):

    def __init__(self, addr):

        self.addr = addr

    def __str__(self):

        return 'Error while acessing memory at address %s' % hex(self.addr)


class MemReadError(MemError):

    def __str__(self):

        return 'Error while reading memory at address %s' % hex(self.addr)


class MemWriteError(MemError):

    def __str__(self):

        return 'Error while writing memory at address %s' % hex(self.addr)


class MemNullPointerError(MemError):

    def __str__(self):

        return 'NULL pointer access detected at address %s' % hex(self.addr)


class CpuError(Error):

    def __init__(self, addr, inum = 0):

        self.addr, self.inum, = addr, inum


class CpuStop(CpuError):

    def __str__(self):

        return 'Stop at instruction %s.%.2d' % (hex(self.addr), self.inum)


class CpuReadError(CpuError):

    def __str__(self):

        return 'Error while reading instruction %s.%.2d' % (hex(self.addr), self.inum)


class CpuInstructionError(CpuError):

    def __str__(self):

        return 'Invalid instruction at %s.%.2d' % (hex(self.addr), self.inum)


class Mem(object):

    # enable NULL pointers detection if value is not None
    LOWEST_USER_ADDR = 0x1000

    # start address for memory allocations
    DEF_ALLOC_BASE = 0x11000000

    # REIL type to length map
    map_length = { U8: 1, U16: 2, U32: 4, U64: 8 }

    # length to REIL type map
    map_size = { 1: U8, 2: U16, 4: U32, 8: U64 }

    # REIL type to struct format map
    map_format = { U8: 'B', U16: 'H', U32: 'I', U64: 'Q' }    

    def __init__(self, data = None, reader = None, strict = True):

        self.data = {} if data is None else None
        self.reader, self.strict = reader, strict
        self.alloc_base = self.DEF_ALLOC_BASE
        self.alloc_last = self.alloc_base

    def pack(self, size, val):

        return struct.pack(self.map_format[size], val)

    def unpack(self, size, data):

        return struct.unpack(self.map_format[size], data)[0] 

    def clear(self):

        self.data = {}

    def _is_valid_addr(self, addr):

        if self.LOWEST_USER_ADDR is not None and \
           self.LOWEST_USER_ADDR > addr:

            raise MemNullPointerError(addr)

    def _read(self, addr, size):

        self._is_valid_addr(addr)

        ret = []

        for i in range(size):

            if not self.data.has_key(addr + i):

                raise MemReadError(addr)

            ret.append(self.data[addr + i])

        return ret

    def _write(self, addr, size, data):

        self._is_valid_addr(addr)

        for i in range(size):

            if self.strict and not self.data.has_key(addr + i):

                raise MemWriteError(addr)

            self.data[addr + i] = data[i]

    def read(self, addr, size):

        try:

            return ''.join(self._read(addr, size))

        except MemReadError:

            #
            # Invalid read address, try to get the data 
            # from external memory reader if possible.
            #
            if self.reader is None: raise            
            
            try: data = self.reader.read(addr, size)
            except ReadError: raise MemReadError(addr)

            # copy data from external storage
            self.alloc(addr, data = data)
            return data

    def write(self, addr, size, data):

        try:

            self._write(addr, size, list(data))

        except MemWriteError:

            if self.strict:

                if self.reader is None: raise

                # invalid address, check if memory reader knows it
                if self.reader.read(addr, size) is None: 

                    raise

            # allocate memory at adderss that was not available
            self.alloc(addr, data = data)

    def alloc_addr(self, size):

        ret = self.alloc_last
        self.alloc_last += size

        return ret

    def alloc(self, addr = None, size = None, data = None):

        if addr is not None: self._is_valid_addr(addr)

        size = len(data) if size is None and not data is None else size
        addr = self.alloc_addr(size) if addr is None else addr

        for i in range(0, size):

            # fill target memory range with specified data (or zeros)
            byte = '\0' if data is None or i >= len(data) else data[i]
            self.data[addr + i] = byte

        return addr

    def store(self, addr, size, val):

        self.write(addr, self.map_length[size], self.pack(size, val))

    def load(self, addr, size):

        val = self.read(addr, self.map_length[size])

        return self.unpack(size, val)

    def dump_hex(self, data, width = 16, addr = None):

        def quoted(data):

            # replace non-alphanumeric characters
            return ''.join(map(lambda b: b if b.isalnum() else '.', data))

        while data:

            line = data[: width]
            data = data[width :]

            s = map(lambda b: '%.2x' % ord(b), line)
            s += [ '  ' ] * (width - len(line))

            s = '%s | %s' % (' '.join(s), quoted(line))
            if addr is not None: s = '%.8x: %s' % (addr, s)

            print s

            addr += len(line)

    def dump(self, addr, size):

        # read and dump memory contents
        self.dump_hex(self.read(addr, size), addr = addr)


class TestMem(unittest.TestCase):

    def test_access(self):             

        mem = Mem(strict = False)

        val = 0x1111111122224488
        addr = 0x10000000

        mem.store(addr, U64, 0x1111111111111111)
        mem.store(addr, U32, 0x22222222)
        mem.store(addr, U16, 0x4444)
        mem.store(addr, U8, 0x88)
        
        assert mem.data == { addr + 0: chr(0x88), addr + 1: chr(0x44), addr + 2: chr(0x22), addr + 3: chr(0x22),
                             addr + 4: chr(0x11), addr + 5: chr(0x11), addr + 6: chr(0x11), addr + 7: chr(0x11) }
        
        assert mem.load(addr, U64) == val and \
               mem.load(addr, U32) == val & 0xffffffff and \
               mem.load(addr, U16) == val & 0xffff and \
               mem.load(addr, U8) == val & 0xff

    def test_null_ptr(self):       

        mem = Mem(strict = False)

        #
        # try to access memory lower that NULL pointer detection limit
        #
        try: 

            mem.store(mem.LOWEST_USER_ADDR - 1, U8, 0)
            assert False

        except MemNullPointerError: pass

        try: 

            mem.load(mem.LOWEST_USER_ADDR - 1, U8)
            assert False

        except MemNullPointerError: pass

        # try to access higher memory address
        mem.store(mem.LOWEST_USER_ADDR, U8, 0)
        assert mem.load(mem.LOWEST_USER_ADDR, U8) == 0


class Math(object):

    def __init__(self, a = None, b = None):

        self.a, self.b = a, b    

    def val(self, arg):

        return None if arg is None else arg.get_val()

    def val_u(self, arg):

        # Arg to numpy unsigned integer
        return None if arg is None else {
            
             U1: numpy.uint8, 
             U8: numpy.uint8, 
            U16: numpy.uint16,
            U32: numpy.uint32, 
            U64: numpy.uint64  

        }[arg.size](self.val(arg))

    def val_s(self, arg):

        # Arg to numpy signed integer
        return None if arg is None else { 

             U1: numpy.int8, 
             U8: numpy.int8, 
            U16: numpy.int16,
            U32: numpy.int32, 
            U64: numpy.int64  

        }[arg.size](self.val_u(arg))

    def eval(self, op, a = None, b = None):

        a = self.a if a is None else a
        b = self.b if b is None else b

        # evaluale unsigned/unsigned expressions
        eval_u = lambda fn: fn(self.val_u(a), self.val_u(b)).item()
        eval_s = lambda fn: fn(self.val_s(a), self.val_s(b)).item()        

        ret = { 

            I_STR: lambda: a.get_val(),            
            I_ADD: lambda: eval_u(lambda a, b: a +  b ),
            I_SUB: lambda: eval_u(lambda a, b: a -  b ),            
            I_NEG: lambda: eval_u(lambda a, b:     -a ),
            I_MUL: lambda: eval_u(lambda a, b: a *  b ),
            I_DIV: lambda: eval_u(lambda a, b: a /  b ),
            I_MOD: lambda: eval_u(lambda a, b: a %  b ),
           I_SMUL: lambda: eval_s(lambda a, b: a *  b ),
           I_SDIV: lambda: eval_s(lambda a, b: a /  b ),
           I_SMOD: lambda: eval_s(lambda a, b: a %  b ),
            I_SHL: lambda: eval_u(lambda a, b: a << b ),
            I_SHR: lambda: eval_u(lambda a, b: a >> b ),
            I_AND: lambda: eval_u(lambda a, b: a &  b ),
             I_OR: lambda: eval_u(lambda a, b: a |  b ),
            I_XOR: lambda: eval_u(lambda a, b: a ^  b ),            
            I_NOT: lambda: eval_u(lambda a, b:     ~a ),
             I_EQ: lambda: eval_u(lambda a, b: a == b ),
             I_LT: lambda: eval_u(lambda a, b: a <  b ) 

        }[op]()

        # hack for one bit arguments
        if (a is None or a.size == U1) and \
           (b is None or b.size == U1):

            ret &= 1

        return ret


class TestMath(unittest.TestCase):

    def test(self):     

        pass


class Reg(object):

    def __init__(self, size, val, name = None, temp = False):

        self.size, self.val, self.name, self.temp = size, val, name, temp

    def __str__(self):

        return self.name if self.name is not None \
                         else '%.16x' % self.get_val()

    def get_val(self, val = None):

        val = self.val if val is None else val
        mkval = lambda mask: long(val & mask)

        if self.size == U1: return 0 if mkval(0x1) == 0 else 1
        elif self.size == U8:  return mkval(0xff)
        elif self.size == U16: return mkval(0xffff)
        elif self.size == U32: return mkval(0xffffffff)
        elif self.size == U64: return mkval(0xffffffffffffffff)

    def str_val(self, val = None):

        return hex(self.get_val() if val is None else val)


class Cpu(object):

    DEF_REG = Reg
    DEF_REG_VAL = 0L

    DEF_R_DFLAG = 1L

    #
    # Debugging options
    #
    DBG_TRACE_INSN = 1        # show infromation about each instruction being executed
    DBG_TRACE_INSN_ARGS = 2   # show information about instruction arguments

    def __init__(self, arch, mem = None, math = None, debug = 0):

        self.mem = Mem() if mem is None else mem
        self.math = Math() if math is None else math
        self.debug = debug
        self.arch = get_arch(arch)
        self.reset()

    def set_storage(self, storage = None):

        self.mem.reader = None if storage is None else storage.reader

    def reset(self, regs = None, mem = None):

        self.insn, self.regs = None, {}
        self.set_ip(0)   

        if self.arch == x86:

            # By default DF is zero, so we need to set R_DFLAG to 1
            # for indexes auto-incrementing (ffffffffh is used when DF == 1).
            self.reg('R_DFLAG', self.DEF_R_DFLAG)

        if regs is not None:

            # set up caller specified registers set
            for name, val in regs.items(): self.reg(name, val)        

        if mem is not None:

            self.mem = mem

    def reset_temp(self):

        for name, reg in self.regs.items():

            if reg.temp: self.regs.pop(name)

    def reg(self, name, val = None, size = None):

        if isinstance(name, Arg):

            assert not name.type in [ A_NONE, A_CONST ]

            # find register by instruction argument
            name, size, temp = name.name, name.size, name.type == A_TEMP          

        else:

            # find register by name
            size = self.arch.size if size is None else size
            temp = False

        if not name[:2] in [ 'R_', 'V_' ]:

            # make canonical register name
            name = '%s_%s' % ( 'V' if temp else 'R', name.upper() )

        if self.regs.has_key(name): 

            reg = self.regs[name]

            if val is not None:

                # set value of existing register
                reg.val = val            

        else: 

            # create register with specified or default value
            val = self.DEF_REG_VAL if val is None else val
            reg = self.regs[name] = self.DEF_REG(size, val, name = name, temp = temp)

        return reg

    def arg(self, arg):

        if arg.type in [ A_NONE, A_LOC ]: 

            return None

        elif arg.type in [ A_REG, A_TEMP ]: 

            return self.reg(arg)

        elif arg.type == A_CONST: 

            return self.DEF_REG(arg.size, arg.val)

    def insn_none(self, insn, a, b, c):

        return None

    def insn_jcc(self, insn, a, b, c):

        # return address of the next instruction to execute if condition was taken
        if a.get_val() != 0:
            
            if insn.c.type == A_LOC:

                return insn.c.val

            else:

                return ( c.get_val(), 0 )

        else:

            return None

    def insn_stm(self, insn, a, b, c):

        # store a to memory
        self.mem.store(c.get_val(), insn.a.size, a.get_val())
        return None

    def insn_ldm(self, insn, a, b, c):

        # read from memory to c
        self.reg(insn.c, self.mem.load(a.get_val(), insn.c.size))
        return None

    def insn_other(self, insn, a, b, c):

        # evaluate all other instructions
        self.reg(insn.c, self.math.eval(insn.op, a, b))
        return None

    def _log_insn(self, insn):        

        if not self.debug & self.DBG_TRACE_INSN:

            return

        print insn

        self._log_insn_arg(insn.a)
        self._log_insn_arg(insn.b)
        self._log_insn_arg(insn.c)

        if not self.debug & self.DBG_TRACE_INSN_ARGS:

            return

        print

    def _log_insn_arg(self, arg):

        if not self.debug & self.DBG_TRACE_INSN_ARGS:

            return

        if arg.type in [ A_REG, A_TEMP ]: 

            reg = self.reg(arg)
            val = reg.get_val()

            print ' %.10s = 0x%x' % (arg.name, val)

    def execute(self, insn):

        # print instruction information
        self._log_insn(insn)

        # get arguments values
        a, b, c = self.arg(insn.a), self.arg(insn.b), self.arg(insn.c)
        
        if not insn.op in REIL_INSN:

            # invalid opcode
            raise CpuInstructionError(insn.addr, insn.inum)

        try:
            
            return {

                I_NONE: self.insn_none,
                 I_JCC: self.insn_jcc,
                 I_STM: self.insn_stm,
                 I_LDM: self.insn_ldm

            # call opcode-specific handler
            }[insn.op](insn, a, b, c)

        except KeyError:

            return self.insn_other(insn, a, b, c)

    def get_ip(self):

        return self.reg(self.arch.Registers.ip).get_val()

    def set_ip(self, val):

        self.reg(self.arch.Registers.ip, val)

    def run(self, storage, addr = 0L, stop_at = None):

        next = ( addr, 0 )

        # use specified storage instance
        self.set_storage(storage)                

        while True:

            addr, inum = next

            # update IP register
            self.set_ip(addr)

            # remove temp registers from previous machine instructions
            if inum == 0: self.reset_temp()
            
            try:   

                # query IR instruction from the storage               
                self.insn = storage.get_insn(next)

            except StorageError:

                raise CpuReadError(*next)            

            if stop_at is not None and (addr in stop_at or next in stop_at):

                raise CpuStop(*next)

            # execute single instruction
            next = self.execute(self.insn)            

            if next is None:

                # JCC was not taken, execute instruction at next address            
                next = self.insn.next()            

        self.set_storage()

    def dump(self, show_flags = True, show_temp = False, show_all = False):

        # dump general purpose registers
        for name, reg in self.regs.items():

            if name in [ self.arch.Registers.ip ] + list(self.arch.Registers.general) or \
               show_all:

                print '%8s: %s' % (name, reg.str_val())

        if show_all:

            return

        if show_flags:

            # dump flags
            for name, reg in self.regs.items():

                if name in self.arch.Registers.flags:

                    print '%8s: %s' % (name, reg.str_val())

        if show_temp:

            # dump temp registers
            for reg in self.regs.values():

                if reg.temp:

                    print '%8s: %s' % (reg.name, reg.str_val())

    def dump_mem(self, addr, size): 

        self.mem.dump(addr, size)


class TestCpu(unittest.TestCase):

    arch = ARCH_X86

    def test(self):     

        code = ( 'mov eax, edx',
                 'add eax, ecx', 
                 'ret' )

        addr, stack = 0x41414141, 0x42424242

        # create reader and translator
        from pyopenreil.utils import asm
        tr = CodeStorageTranslator(asm.Reader(self.arch, code, addr = addr))
        
        cpu = Cpu(self.arch)

        # set up stack pointer and input args
        cpu.reg('esp', stack)
        cpu.reg('ecx', 1)
        cpu.reg('edx', 2)

        # run untill ret
        try: cpu.run(tr, addr)
        except MemReadError as e: 

            # exception on accessing to the stack
            if e.addr != stack: raise

        # check for correct return value
        assert cpu.reg('eax').val == 3

    def test_code_read(self):

        addr, stack = 0x41414141, 0x42424242
    
        # test code that reads itself
        code = ( 'nop', 'nop', 
                 'nop', 'nop', 
                 'mov eax, dword ptr [0x%X]' % addr, 
                 'ret' )

        # create reader and translator
        from pyopenreil.utils import asm
        tr = CodeStorageTranslator(asm.Reader(self.arch, code, addr = addr))

        cpu = Cpu(ARCH_X86)

        # set up stack pointer
        cpu.reg('esp', stack)
        
        # run untill ret
        try: cpu.run(tr, addr)
        except MemReadError as e: 

            # exception on accessing to the stack
            if e.addr != stack: raise

        # check for correct return value
        assert cpu.reg('eax').val == 0x90909090


class Stack(object):

    # start address of stack memory
    DEF_STACK_BASE = 0x12000000

    def __init__(self, mem, item_size, addr = None, size = None):

        self.size = 0x1000 if size is None else size
        self.addr = mem.alloc(addr = self.DEF_STACK_BASE, size = self.size)

        self.top = self.bottom = self.addr + self.size
        self.mem, self.item_size = mem, item_size

    def push(self, val):

        self.top -= self.item_size
        self.mem.store(self.top, Mem.map_size[self.item_size], val)

    def pop(self):

        val = self.mem.load(self.top, Mem.map_size[self.item_size])
        self.top += self.item_size

        return val


class TestStack(unittest.TestCase):

    arch = ARCH_X86

    def test(self):     

        code = ( 'pop ecx', 
                 'pop eax', 
                 'jmp ecx'  )

        addr, arg, ret = 0x41414141, 0x42424242, 0x43434343        

        # create reader and translator        
        from pyopenreil.utils import asm
        tr = CodeStorageTranslator(asm.Reader(self.arch, code, addr = addr))

        cpu = Cpu(self.arch)

        from pyopenreil.arch import x86
        stack = Stack(cpu.mem, x86.ptr_len)

        # set up stack arg and return address
        stack.push(arg)
        stack.push(ret)
        cpu.reg(x86.Registers.sp, stack.top)

        try:

            # run untill ret
            cpu.run(tr, addr)

        except CpuReadError as e: 

            # exception on accessing to the stack
            if e.addr != ret: raise

        # check for correct return value
        cpu.reg('eax').val == arg


class Abi(object):

    DUMMY_RET_ADDR = 0xcafebabe

    def __init__(self, cpu, storage, no_reset = False):

        self.cpu = cpu
        self.mem, self.arch = cpu.mem, cpu.arch
        self.storage = storage

        if not no_reset: self.reset()

    def align(self, val):

        return val + (self.arch.ptr_len - val % self.arch.ptr_len)

    def read(self, addr, size):

        return self.mem.read(addr, size)

    def buff(self, data, addr = None, fill = None):

        if isinstance(data, basestring):

            # data was passed 
            size = len(data)

        else:

            # length was passed
            size = data
            data = None if fill is None else fill * size

        return self.mem.alloc(addr = addr, size = self.align(size), data = data)

    def string(self, data):

        # allocate null terminated buffer
        return self.buff(data + '\0\0\0\0')

    def stack(self, size = None):

        return Stack(self.mem, self.arch.ptr_len, size = size)

    def makearg(self, val):

        return self.string(val) if isinstance(val, basestring) else val

    def pushargs(self, args):

        args = list(args)
        args.reverse()

        # push arguments into the stack
        stack = self.stack()        
        for a in args:

            # copy buffers into the memory
            stack.push(self.makearg(a))

        return stack

    def initial_regs(self):

        regs = {}
        regs.update(map(lambda name: (name, 0L), self.arch.Registers.general + \
                                                 self.arch.Registers.flags))
        return regs

    def reset(self):

        # clear memory
        self.mem.clear()

        # reset cpu state
        self.cpu.reset(self.initial_regs())

    def reg(self, name, val = None):

        # get/set register value
        if val is None: return self.cpu.reg(name).val
        else: self.cpu.reg(name).val = val

    def call(self, addr, *args):

        # prepare stack for call
        stack = self.pushargs(args)

        if self.cpu.arch == x86:

            # push return address to the stack
            stack.push(self.DUMMY_RET_ADDR)

        elif self.cpu.arch == arm:

            # save return address in link register
            self.reg(arm.Registers.lr, self.DUMMY_RET_ADDR)

        else: assert False

        self.reg(self.arch.Registers.sp, stack.top)

        try:

            # run untill cpu will stop on DUMMY_RET_ADDR
            self.cpu.run(self.storage, addr)

        except CpuError as e:

            if e.addr != self.DUMMY_RET_ADDR:

                raise
    
    def stdcall(self, addr, *args):        

        # init cpu and call target function
        self.reg(self.arch.Registers.accum, 0)        
        self.call(addr, *args)

        # return accumulator value
        return self.reg(self.arch.Registers.accum)

    def cdecl(self, addr, *args):

        # we never need to care about stack cleanup
        return self.stdcall(addr, *args)

    def ms_fastcall(self, addr, *args):

        if len(args) > 0:

            self.reg('ecx', self.makearg(args[0]))
            args = args[1:]

        if len(args) > 0:
        
            self.reg('edx', self.makearg(args[0]))
            args = args[1:]

        return self.stdcall(addr, *args)

    def arm_call(self, addr, *args):

        if len(args) > 0:

            self.reg('r0', self.makearg(args[0]))
            args = args[1:]

        if len(args) > 0:
        
            self.reg('r1', self.makearg(args[0]))
            args = args[1:]

        if len(args) > 0:

            self.reg('r2', self.makearg(args[0]))
            args = args[1:]

        if len(args) > 0:
        
            self.reg('r3', self.makearg(args[0]))
            args = args[1:]

        self.call(addr, *args)

        # return accumulator value
        return self.reg(self.arch.Registers.accum)


class TestAbi(unittest.TestCase):

    arch = ARCH_X86

    def test(self):     

        code = ( 'pop ecx', 
                 'pop eax', 
                 'jmp ecx'  )   

        addr, arg = 0x41414141, 0x42424242

        # create reader and translator        
        from pyopenreil.utils import asm
        tr = CodeStorageTranslator(asm.Reader(self.arch, code, addr = addr))

        cpu = Cpu(self.arch)
        abi = Abi(cpu, tr)

        # check for correct return value
        assert abi.stdcall(addr, arg) == arg

#
# EoF
#
