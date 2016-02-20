import sys, os, unittest
import md5

from pyopenreil.REIL import *
from pyopenreil.VM import *

class TestMD5(unittest.TestCase):    
    
    CPU_DEBUG = 0
    FILE_DIR = os.path.abspath(os.path.dirname(__file__))

    is_linux = lambda self: 'linux' in sys.platform
    file_path = lambda self, path: os.path.join(self.FILE_DIR, path)

    def _run_test(self, callfunc, arch, reader, addr_list):

        md5_init, md5_append, md5_finish = addr_list

        # test input for MD5 hashing
        test_val = 'foobar'
        digest_len = 16
    
        # load executable image of the test program
        tr = CodeStorageTranslator(reader)

        # run some basic optimizations
        tr.optimize(addr_list)

        print tr.storage

        # create CPU and ABI
        cpu = Cpu(arch, debug = self.CPU_DEBUG)
        abi = Abi(cpu, tr)

        ctx = abi.buff(6 * 4 + 8 + 64)
        val = abi.buff(test_val)
        digest = abi.buff(digest_len)

        # void md5_init(md5_state_t *)
        callfunc(abi)(md5_init, ctx)        

        # void md5_append(md5_state_t *, const md5_byte_t *, int)
        callfunc(abi)(md5_append, ctx, val, len(test_val))        

        # md5_finish(md5_state_t *, md5_byte_t *)
        callfunc(abi)(md5_finish, ctx, digest)        

        # read results of hash function
        digest = abi.read(digest, digest_len)        
        print 'Hash value:', ''.join(map(lambda b: '%.2x' % ord(b), digest))

        # check for correct result
        assert digest == md5.new(test_val).digest()        


class TestMD5_X86(TestMD5):

    ELF_NAME = 'md5_x86.elf'
    ELF_ADDR = ( 0x08049254, 0x08049296, 0x080493C2 )

    PE_NAME = 'md5_x86.pe'
    PE_ADDR = ( 0x00402417, 0x00402459, 0x00402585 )

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


class TestMD5_ARM(TestMD5):

    ELF_NAME = 'md5_arm.elf'
    ELF_ADDR = ( 0x00009AE0, 0x00009B58, 0x00009D1C )

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
