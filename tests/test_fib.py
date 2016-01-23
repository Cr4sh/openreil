import sys, os, unittest

file_dir = os.path.abspath(os.path.dirname(__file__))
reil_dir = os.path.abspath(os.path.join(file_dir, '..'))
if not reil_dir in sys.path: sys.path = [ reil_dir ] + sys.path

from pyopenreil.REIL import *
from pyopenreil.VM import *
from pyopenreil.utils import bin_PE, bin_BFD

class TestFib(unittest.TestCase):

    X86_PE_PATH = os.path.join(file_dir, 'fib_x86.pe')
    X86_PE_ADDR = 0x004016B0

    X86_ELF_PATH = os.path.join(file_dir, 'fib_x86.elf')
    X86_ELF_ADDR = 0x08048414

    ARM_ELF_PATH = os.path.join(file_dir, 'fib_arm.elf')
    ARM_ELF_ADDR = arm_thumb(0x000083a1)

    def _run_test(self, callfunc, arch, reader, addr):        
    
        # load executable image of the test program
        tr = CodeStorageTranslator(reader)

        # construct dataflow graph for given function
        dfg = DFGraphBuilder(tr).traverse(addr)  
        insn_before = tr.size()

        # run some basic optimizations
        dfg.optimize_all(storage = tr.storage)
        insn_after = tr.size()

        print tr.storage

        print '%d instructions before optimization and %d after\n' % \
              (insn_before, insn_after)

        # create CPU and ABI
        cpu = Cpu(arch)
        abi = Abi(cpu, tr)

        testval = 11

        # int fib(int n);
        ret = callfunc(abi)(addr, testval)
        cpu.dump()

        print '%d number in Fibonacci sequence is %d' % (testval + 1, ret)    

        assert ret == 144

    def test_x86(self):

        try:

            from pyopenreil.utils import bin_PE

            self._run_test(lambda abi: abi.cdecl, 
                ARCH_X86, bin_PE.Reader(self.X86_PE_PATH), self.X86_PE_ADDR)
        
        except ImportError, why: print '[!]', str(why)

        try:

            from pyopenreil.utils import bin_BFD

            self._run_test(lambda abi: abi.cdecl, 
                ARCH_X86, bin_BFD.Reader(self.X86_ELF_PATH), self.X86_ELF_ADDR)

        except ImportError, why: print '[!]', str(why)

    def test_arm(self):

        try:

            from pyopenreil.utils import bin_BFD

            self._run_test(lambda abi: abi.arm_call, 
                           ARCH_ARM, 
                           bin_BFD.Reader(self.ARM_ELF_PATH, arch = ARCH_ARM), 
                           self.ARM_ELF_ADDR)

        except ImportError, why: print '[!]', str(why)

if __name__ == '__main__':    

    suite = unittest.TestSuite([ TestFib('test_x86'), TestFib('test_arm') ])
    unittest.TextTestRunner(verbosity = 2).run(suite)

#
# EoF
#
