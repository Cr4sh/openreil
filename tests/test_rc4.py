import sys, os, unittest

file_dir = os.path.abspath(os.path.dirname(__file__))
reil_dir = os.path.abspath(os.path.join(file_dir, '..'))
if not reil_dir in sys.path: sys.path = [ reil_dir ] + sys.path

from pyopenreil.REIL import *
from pyopenreil.VM import *

class TestRC4(unittest.TestCase):            

    CPU_DEBUG = 0

    is_linux = lambda self: 'linux' in sys.platform

    def _run_test(self, callfunc, arch, reader, addr_list):  

        rc4_set_key, rc4_crypt = addr_list

        # test input data for RC4 encryption
        test_key = 'somekey'
        test_val = 'bar'

        tr = CodeStorageTranslator(reader)

        # run some basic optimizations
        tr.optimize(addr_list)

        # create CPU and ABI
        cpu = Cpu(arch, debug = self.CPU_DEBUG)
        abi = Abi(cpu, tr)

        # allocate buffers for arguments of emulated functions
        ctx = abi.buff(256 + 4 * 2)
        val = abi.buff(test_val)

        # emulate rc4_set_key() function call
        callfunc(abi)(rc4_set_key, ctx, test_key, len(test_key))

        # emulate rc4_crypt() function call
        callfunc(abi)(rc4_crypt, ctx, val, len(test_val))
        
        # read results of RC4 encryption
        val = abi.read(val, len(test_val))
        print 'Encrypted value:', repr(val)

        # check for correct result
        assert val == '\x38\x88\xBC'


class TestRC4_X86(TestRC4):        

    ELF_PATH = os.path.join(file_dir, 'rc4_x86.elf')
    ELF_ADDR = ( 0x08048522, 0x080485EB )

    PE_PATH = os.path.join(file_dir, 'rc4_x86.pe')
    PE_ADDR = ( 0x004016D5, 0x004017B5 )

    def test_elf(self):

        # BFD available for different operating systems
        # but current test needs Linux.
        if not self.is_linux(): return

        try:

            from pyopenreil.utils import bin_BFD

            self._run_test(lambda abi: abi.cdecl, 
                ARCH_X86, bin_BFD.Reader(self.ELF_PATH), self.ELF_ADDR)

        except ImportError, why: print '[!]', str(why)        

    def test_pe(self):

        try:

            from pyopenreil.utils import bin_PE

            self._run_test(lambda abi: abi.cdecl, 
                ARCH_X86, bin_PE.Reader(self.PE_PATH), self.PE_ADDR)
        
        except ImportError, why: print '[!]', str(why)


class TestRC4_ARM(TestRC4):  

    ELF_PATH = os.path.join(file_dir, 'rc4_arm.elf')
    ELF_ADDR = ( 0x000085B8, 0x000086FC )

    def test_elf(self):        

        # BFD available for different operating systems
        # but current test needs Linux.
        if not self.is_linux(): return

        try:

            from pyopenreil.utils import bin_BFD
            
            self._run_test(lambda abi: abi.arm_call, 
                ARCH_ARM, bin_BFD.Reader(self.ELF_PATH, arch = ARCH_ARM), self.ELF_ADDR)

        except ImportError, why: print '[!]', str(why)


if __name__ == '__main__':    

    unittest.main(verbosity = 2)

#
# EoF
#
