import sys, os
from sets import Set

from pyopenreil.utils import Bin_PE
from pyopenreil.utils import Asm_x86
from pyopenreil.utils.pyasm2.x86 import *

from pyopenreil.VM import *
from pyopenreil.REIL import *
from pyopenreil.arch import x86

CODE  = '\x0f\x32\x0f\x30' # rdmsr, wrmsr
CODE += '\x0f\x01\x08\x0f\x01\x00\x0f\x00\x00' # sidt, sgdt, sldt
CODE += '\x0f\x01\x10\x0f\x01\x18\x0f\x00\x10' # lidt, lgdt, lldt
CODE += '\x0f\x31' # rdtsc
CODE += '\x0f\xa2' # cupid
CODE += '\xcc' # int 3
CODE += '\x90' # nop
CODE += '\xf4' # hlt
CODE += '\xc3\x68\x00\x00\x00\x00\xF3\xA4\xE8\x00\x00\x00\x00\xC2\x04\x00'

ADDR = 0x1337L
ENTRY = 0        


def test_0(argv):
    ''' Instruction tarnslation test. '''

    addr = 0x004016B0
    reader = Bin_PE.Reader('../_tests/fib/fib.exe')
    tr = CodeStorageTranslator('x86', reader)

    print tr.get_insn(addr)


def test_0_1(argv):
    ''' Instruction tarnslation test. '''    

    code = ( mov(eax, ecx),
             add(eax, edx), 
             shl(eax, 12),
             ret()
    )

    reader = Asm_x86.Reader(ADDR, *code)
    tr = CodeStorageTranslator('x86', reader)

    print tr.get_func(ADDR + ENTRY)
    tr.storage.to_file('test_0_1.ir')

    cpu = Cpu('x86')
    abi = Abi(cpu, tr)

    abi.fastcall(ADDR, 1, 2)
    cpu.dump()


def test_0_2(argv):
    ''' Instruction tarnslation test. '''    

    code = ( nop(), nop(),
             nop(), nop(),
             mov(eax, dword [ADDR]), 
             ret()
    )

    reader = Asm_x86.Reader(ADDR, *code)
    tr = CodeStorageTranslator('x86', reader)

    print tr.get_insn(ADDR + ENTRY)

    cpu = Cpu('x86')
    abi = Abi(cpu, tr)

    val = abi.stdcall(ADDR)
    cpu.dump()

    print 'Returned vale is %s' % hex(val)     


def test_1(argv):
    ''' Function translation test. '''

    reader = ReaderRaw(CODE, addr = ADDR)
    tr = CodeStorageTranslator('x86', reader)

    cfg = CFGraphBuilder(tr).traverse(ADDR + ENTRY)
    for node in cfg.nodes.values(): print str(node.item) + '\n'


def test_2(argv):
    ''' Code elimination test. '''

    addr = 0x004010EC
    storage = CodeStorageMem('x86')
    storage.from_file('kao_check_serial.ir')    

    print storage
    print '%d instructions' % storage.size()
    print

    dfg = DFGraphBuilder(storage).traverse(addr)
    dfg.to_dot_file('kao_check_serial.dot')
    
    dfg.eliminate_dead_code()
    print 
    
    dfg.constant_folding()
    print

    storage = CodeStorageMem('x86')

    dfg.to_dot_file('kao_check_serial_o.dot')   
    dfg.store(storage)

    print storage
    print '%d instructions' % storage.size()    
    

def test_3(argv):
    ''' Execution test. '''
    
    addr = 0x004016B0
    reader = Bin_PE.Reader('../_tests/fib/fib.exe')
    tr = CodeStorageTranslator('x86', reader)

    dfg = DFGraphBuilder(tr).traverse(addr)  
    dfg.to_dot_file('fib.dot')
    insn_before = tr.size()

    dfg.eliminate_dead_code()
    dfg.constant_folding()

    dfg.store(tr.storage)
    dfg.to_dot_file('fib_o.dot')
    insn_after = tr.size()

    print tr.storage

    print '%d instructions before optimization and %d after\n' % \
          (insn_before, insn_after)

    cpu = Cpu('x86')
    abi = Abi(cpu, tr)

    testval = 5

    # int fib(int n);
    ret = abi.cdecl(addr, testval)
    cpu.dump()

    print '%d number in Fibonacci sequence is %d' % (testval + 1, ret)    


def test_4(argv):
    
    # rc4.exe VA's of the rc4_set_key() and rc4_crypt()
    rc4_set_key = 0x004016D5
    rc4_crypt = 0x004017B5

    # test input data for encryption
    test_key = 'somekey'
    test_val = 'bar'

    # load PE image
    reader = Bin_PE.Reader('../_tests/rc4/rc4.exe')
    tr = CodeStorageTranslator('x86', reader)

    def code_optimization(addr):
 
        # construct dataflow graph for given function
        dfg = DFGraphBuilder(tr).traverse(addr)  
        
        # run some basic IR optimizations
        dfg.eliminate_dead_code()
        dfg.constant_folding()

        # store resulting instructions
        dfg.store(tr.storage)

    code_optimization(rc4_set_key)
    code_optimization(rc4_crypt)

    # create CPU and ABI
    cpu = Cpu('x86')
    abi = Abi(cpu, tr)

    # allocate arguments for IR calls
    ctx = abi.buff(256 + 4 * 2)
    val = abi.buff(test_val)

    # set RC4 key
    abi.cdecl(rc4_set_key, ctx, test_key, len(test_key))

    # encryption
    abi.cdecl(rc4_crypt, ctx, val, len(test_val))
    
    # read result
    val_1 = abi.read(val, len(test_val))
    print 'Encrypted value:', repr(val_1)

    # compare results with Python build-in RC4 module
    from Crypto.Cipher import ARC4
    rc4 = ARC4.new(test_key)
    val_2 = rc4.encrypt(test_val)

    print
    print 'PASSED' if val_1 == val_2 else 'FAILS'


if __name__ == '__main__':  

    exit(test_0_1(sys.argv))

