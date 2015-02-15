import sys, os, unittest

from pyopenreil.utils import bin_PE, bin_BFD
from pyopenreil.utils import asm

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
    reader = bin_PE.Reader(os.getenv('HOME') + '/data/_tests/fib/fib.exe')
    tr = CodeStorageTranslator('x86', reader)

    print tr.get_insn(addr)


def test_0_1(argv):
    ''' Instruction tarnslation test. '''    

    code = ( 'mov eax, ecx',
             'add eax, edx', 
             'shl eax, 12',
             'ret' )

    reader = asm.Reader('x86', ADDR, code)
    tr = CodeStorageTranslator('x86', reader)

    print tr.get_func(ADDR + ENTRY)
    tr.storage.to_file('test_0_1.ir')

    cpu = Cpu('x86')
    abi = Abi(cpu, tr)

    abi.ms_fastcall(ADDR, 1, 2)
    cpu.dump()

    storage = CodeStorageMem('x86')
    storage.from_file('test_0_1.ir')

    print storage

    abi.reset()
    abi.ms_fastcall(ADDR, 1, 2)
    cpu.dump()


def test_0_2(argv):
    ''' Instruction tarnslation test. '''    

    code = ( 'nop', 'nop', 
             'nop', 'nop', 
             'mov eax, dword [%Xh]' % ADDR, 
             'ret' )

    reader = asm.Reader('x86', ADDR, code)
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
    reader = bin_PE.Reader(os.getenv('HOME') + '/data/_tests/fib/fib.exe')
    tr = CodeStorageTranslator('x86', reader)

    cfg = CFGraphBuilder(tr).traverse(addr)
    cfg.to_dot_file('fib_cfg.dot')

    dfg = DFGraphBuilder(tr).traverse(addr)  
    dfg.to_dot_file('fib_dfg.dot')
    insn_before = tr.size()

    dfg.eliminate_dead_code()
    dfg.constant_folding()

    dfg.store(tr.storage)
    dfg.to_dot_file('fib_dfg_o.dot')
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
    reader = bin_PE.Reader(os.getenv('HOME') + '/data/_tests/rc4/rc4.exe')
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


def test_5(argv):
    ''' bb/func test '''
    
    addr = 0x004016B0
    reader = bin_PE.Reader(os.getenv('HOME') + '/data/_tests/fib/fib.exe')
    tr = CodeStorageTranslator('x86', reader)

    bb = tr.get_bb(addr)

    print '%s - %s' % (bb.first.ir_addr(), bb.last.ir_addr())
    print '--------------------------'
    print bb, '\n'

    func = tr.get_func(addr)

    print '%s [ %s ]' % (func.name(), ', '.join(map(lambda c: str(c), func.chunks)))
    print '--------------------------'
    print func, '\n'


def test_6(argv):
    ''' Instruction tarnslation test. '''

    addr = 0x08048434
    reader = bin_BFD.Reader(os.getenv('HOME') + '/data/_tests/fib/fib')
    tr = CodeStorageTranslator('x86', reader)

    print tr.get_func(addr)


def test_7(argv):
    ''' DFG test. '''

    reader = ReaderRaw('\xF3\xA4\xC3')
    tr = CodeStorageTranslator('x86', reader)

    dfg = DFGraphBuilder(tr).traverse(0)
    dfg.to_dot_file('dfg_2.dot')


if __name__ == '__main__':  

    unittest.main(verbosity = 2)
    
    #exit(test_0_2(sys.argv))


