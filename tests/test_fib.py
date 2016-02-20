import sys, os, unittest

from pyopenreil.REIL import *
from pyopenreil.VM import *

class TestFib(unittest.TestCase):    
    
    CPU_DEBUG = 0
    FILE_DIR = os.path.abspath(os.path.dirname(__file__))

    is_linux = lambda self: 'linux' in sys.platform
    file_path = lambda self, path: os.path.join(self.FILE_DIR, path)

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
        cpu = Cpu(arch, debug = self.CPU_DEBUG)
        abi = Abi(cpu, tr)

        testval = 11

        # int fib(int n);
        ret = callfunc(abi)(addr, testval)
        cpu.dump()

        print '%d number in Fibonacci sequence is %d' % (testval + 1, ret)    

        assert ret == 144


class TestFib_X86(TestFib):    

    ELF_NAME = 'fib_x86.elf'
    ELF_ADDR = 0x08048414

    PE_NAME = 'fib_x86.pe'
    PE_ADDR = 0x004016B0

    def test_elf(self):

        # BFD available for different operating systems
        # but current test needs Linux.
        if not self.is_linux(): return

        try:

            from pyopenreil.utils import bin_BFD

            self._run_test(lambda abi: abi.cdecl, 
                ARCH_X86, bin_BFD.Reader(self.file_path(self.ELF_NAME)), self.ELF_ADDR)

        except ImportError, why: print '[!]', str(why)

    def test_pe(self):

        try:

            from pyopenreil.utils import bin_PE

            self._run_test(lambda abi: abi.cdecl, 
                ARCH_X86, bin_PE.Reader(self.file_path(self.PE_NAME)), self.PE_ADDR)
        
        except ImportError, why: print '[!]', str(why)    


class TestFib_ARM(TestFib):

    ELF_NAME = 'fib_arm.elf'
    ELF_ADDR = arm_thumb(0x000083a1)

    def test_elf(self):

        # BFD available for different operating systems
        # but current test needs Linux.
        if not self.is_linux(): return

        try:

            from pyopenreil.utils import bin_BFD

            self._run_test(lambda abi: abi.arm_call, 
                           ARCH_ARM, bin_BFD.Reader(self.file_path(self.ELF_NAME), arch = ARCH_ARM), 
                           self.ELF_ADDR)

        except ImportError, why: print '[!]', str(why)

#
# EoF
#
