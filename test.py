import sys, os, unittest

from pyopenreil.utils import bin_PE, bin_BFD
from pyopenreil.utils import asm

from pyopenreil.VM import *
from pyopenreil.REIL import *
from pyopenreil.arch import x86

ADDR = 0x1337L
ENTRY = 0        

def test_0(argv):
    ''' PE reader test '''    

    addr = 0x004016B0
    reader = bin_PE.Reader(os.getenv('HOME') + '/data/_tests/fib/fib.exe')
    tr = CodeStorageTranslator('x86', reader)

    print tr.get_func(addr)


def test_1(argv):
    ''' BFD reader test '''

    addr = 0x08048434
    reader = bin_BFD.Reader(os.getenv('HOME') + '/data/_tests/fib/fib')
    tr = CodeStorageTranslator('x86', reader)

    print tr.get_func(addr)


def test_2(argv):
    ''' mongodb storage test '''

    from pyopenreil.utils import mongodb
    storage = mongodb.CodeStorageMongo('x86', 'test_fib') 
    storage.clear()

    addr = 0x004016B0
    reader = bin_PE.Reader(os.getenv('HOME') + '/data/_tests/fib/fib.exe')
    tr = CodeStorageTranslator('x86', reader, storage)

    print tr.get_func(addr)


def test_3(argv):
    ''' bb/func translation test '''
    
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


def test_4(argv):
    ''' bogus instructions translation test '''

    code  = '\xCC\x90\xF4'                          # int 3, nop, hlt
    code += '\x0F\x32\x0F\x30'                      # rdmsr, wrmsr
    code += '\x0F\x01\x08\x0F\x01\x00\x0F\x00\x00'  # sidt, sgdt, sldt
    code += '\x0F\x01\x10\x0F\x01\x18\x0F\x00\x10'  # lidt, lgdt, lldt
    code += '\x0F\x31'                              # rdtsc
    code += '\x0F\xA2'                              # cupid
    code += '\xC3'                                  # ret

    reader = ReaderRaw(code, addr = ADDR)
    storage = CodeStorageTranslator('x86', reader)

    cfg = CFGraphBuilder(storage).traverse(ADDR + ENTRY)
    for node in cfg.nodes.values(): print str(node.item) + '\n'


def test_5(argv):
    ''' code elimination test '''

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


def test_6(argv):
    ''' exeution of code that reads itself test '''    

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
    

def test_7(argv):
    ''' execution of fib testcase '''

    from pyopenreil.utils import mongodb
    storage = mongodb.CodeStorageMongo('x86', 'test_fib')        
    storage.clear()
    
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


def test_8(argv):
    ''' execution of rc4 testcase '''
    
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

    return val_1 == val_2


class TestAll(unittest.TestCase):

    def test(self):

        assert test_8(sys.argv)


if __name__ == '__main__':  

    unittest.main(verbosity = 2)

#
# EoF
#
