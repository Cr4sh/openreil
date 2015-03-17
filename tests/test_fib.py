import sys, os, unittest

file_dir = os.path.abspath(os.path.dirname(__file__))
reil_dir = os.path.abspath(os.path.join(file_dir, '..'))
if not reil_dir in sys.path: sys.path = [ reil_dir ] + sys.path

from pyopenreil.REIL import *
from pyopenreil.VM import *
from pyopenreil.utils import bin_PE

class TestFib(unittest.TestCase):

    ARCH = ARCH_X86
    BIN_PATH = os.path.join(file_dir, 'fib.exe')
    PROC_ADDR = 0x004016B0

    def test(self):        
    
        # load PE image of test program
        reader = bin_PE.Reader(self.BIN_PATH)
        tr = CodeStorageTranslator(reader)

        # construct dataflow graph for given function
        dfg = DFGraphBuilder(tr).traverse(self.PROC_ADDR)  
        insn_before = tr.size()

        # run some basic optimizations
        dfg.eliminate_dead_code()
        dfg.constant_folding()
        dfg.eliminate_subexpressions()

        dfg.store(tr.storage)
        insn_after = tr.size()

        print tr.storage

        print '%d instructions before optimization and %d after\n' % \
              (insn_before, insn_after)

        # create CPU and ABI
        cpu = Cpu(self.ARCH)
        abi = Abi(cpu, tr)

        testval = 11

        # int fib(int n);
        ret = abi.cdecl(self.PROC_ADDR, testval)
        cpu.dump()

        print '%d number in Fibonacci sequence is %d' % (testval + 1, ret)    

        assert ret == 144


if __name__ == '__main__':    

    suite = unittest.TestSuite([ TestFib('test') ])
    unittest.TextTestRunner(verbosity = 2).run(suite)

#
# EoF
#
