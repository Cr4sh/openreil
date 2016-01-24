import sys, os, unittest
import md5

file_dir = os.path.abspath(os.path.dirname(__file__))
reil_dir = os.path.abspath(os.path.join(file_dir, '..'))
if not reil_dir in sys.path: sys.path = [ reil_dir ] + sys.path

from pyopenreil.REIL import *
from pyopenreil.VM import *
from pyopenreil.utils import bin_PE, bin_BFD

class TestMD5(unittest.TestCase):

    X86_ELF_PATH = os.path.join(file_dir, 'md5_x86.elf')
    X86_ELF_MD5_INIT = 0x08049254
    X86_ELF_MD5_APPEND = 0x08049296
    X86_ELF_MD5_FINISH = 0x080493C2

    ARM_ELF_PATH = os.path.join(file_dir, 'md5_arm.elf')
    ARM_ELF_MD5_INIT = 0x00009AE0
    ARM_ELF_MD5_APPEND = 0x00009B58
    ARM_ELF_MD5_FINISH = 0x00009D1C

    X86_PE_PATH = os.path.join(file_dir, 'md5_x86.pe')
    X86_PE_MD5_INIT = 0x00402417
    X86_PE_MD5_APPEND = 0x00402459
    X86_PE_MD5_FINISH = 0x00402585

    CPU_DEBUG = 0

    def _run_test(self, callfunc, arch, reader, func_addr):

        md5_init, md5_append, md5_finish = func_addr

        # test input for MD5 hashing
        test_val = 'foobar'
        digest_len = 16
    
        # load executable image of the test program
        tr = CodeStorageTranslator(reader)

        def code_optimization(addr):
     
            # construct dataflow graph for given function
            dfg = DFGraphBuilder(tr).traverse(addr)
            
            # run some basic optimizations
            dfg.optimize_all(storage = tr.storage)

        code_optimization(md5_init)
        code_optimization(md5_append)
        code_optimization(md5_finish)   

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

    def test_x86(self):

        try:

            from pyopenreil.utils import bin_PE

            self._run_test(lambda abi: abi.cdecl, 
                ARCH_X86, bin_PE.Reader(self.X86_PE_PATH), 
                ( self.X86_PE_MD5_INIT, self.X86_PE_MD5_APPEND, self.X86_PE_MD5_FINISH ))
        
        except ImportError, why: print '[!]', str(why)

        try:

            from pyopenreil.utils import bin_BFD

            self._run_test(lambda abi: abi.cdecl, 
                ARCH_X86, bin_BFD.Reader(self.X86_ELF_PATH), 
                ( self.X86_ELF_MD5_INIT, self.X86_ELF_MD5_APPEND, self.X86_ELF_MD5_FINISH ))

        except ImportError, why: print '[!]', str(why)

    def test_arm(self):

        try:

            from pyopenreil.utils import bin_BFD

            self._run_test(lambda abi: abi.arm_call, 
                ARCH_ARM, bin_BFD.Reader(self.ARM_ELF_PATH, arch = ARCH_ARM), 
                ( self.ARM_ELF_MD5_INIT, self.ARM_ELF_MD5_APPEND, self.ARM_ELF_MD5_FINISH ))

        except ImportError, why: print '[!]', str(why)

if __name__ == '__main__':    

    suite = unittest.TestSuite([ TestMD5('test_x86'), TestMD5('test_arm') ])
    unittest.TextTestRunner(verbosity = 2).run(suite)

#
# EoF
#
