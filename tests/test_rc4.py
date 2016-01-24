import sys, os, unittest

file_dir = os.path.abspath(os.path.dirname(__file__))
reil_dir = os.path.abspath(os.path.join(file_dir, '..'))
if not reil_dir in sys.path: sys.path = [ reil_dir ] + sys.path

from pyopenreil.REIL import *
from pyopenreil.VM import *

class TestRC4(unittest.TestCase):    

    X86_PE_PATH = os.path.join(file_dir, 'rc4_x86.pe')
    X86_PE_RC4_SET_KEY = 0x004016D5 
    X86_PE_RC4_CRYPT = 0x004017B5

    X86_ELF_PATH = os.path.join(file_dir, 'rc4_x86.elf')
    X86_ELF_RC4_SET_KEY = 0x08048522
    X86_ELF_RC4_CRYPT = 0x080485EB

    ARM_ELF_PATH = os.path.join(file_dir, 'rc4_arm.elf')
    ARM_ELF_RC4_SET_KEY = 0x000085B8
    ARM_ELF_RC4_CRYPT = 0x000086FC

    CPU_DEBUG = 0

    def _run_test(self, callfunc, arch, reader, func_addr):  

        rc4_set_key, rc4_crypt = func_addr

        # test input data for RC4 encryption
        test_key = 'somekey'
        test_val = 'bar'

        tr = CodeStorageTranslator(reader)

        def code_optimization(addr):
     
            # construct dataflow graph for given function
            dfg = DFGraphBuilder(tr).traverse(addr)
            
            # run some basic optimizations
            dfg.optimize_all(storage = tr.storage)

        code_optimization(rc4_set_key)
        code_optimization(rc4_crypt)

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

    def test_x86(self):

        try:

            from pyopenreil.utils import bin_PE

            self._run_test(lambda abi: abi.cdecl, 
                ARCH_X86, bin_PE.Reader(self.X86_PE_PATH), 
                ( self.X86_PE_RC4_SET_KEY, self.X86_PE_RC4_CRYPT ))
        
        except ImportError, why: print '[!]', str(why)        

        try:

            from pyopenreil.utils import bin_BFD

            self._run_test(lambda abi: abi.cdecl, 
                ARCH_X86, bin_BFD.Reader(self.X86_ELF_PATH), 
                ( self.X86_ELF_RC4_SET_KEY, self.X86_ELF_RC4_CRYPT ))

        except ImportError, why: print '[!]', str(why)        

    def test_arm(self):        

        try:

            from pyopenreil.utils import bin_BFD
            
            self._run_test(lambda abi: abi.arm_call, 
                           ARCH_ARM, bin_BFD.Reader(self.ARM_ELF_PATH, arch = ARCH_ARM), 
                           ( self.ARM_ELF_RC4_SET_KEY, self.ARM_ELF_RC4_CRYPT ))

        except ImportError, why: print '[!]', str(why)

if __name__ == '__main__':    

    suite = unittest.TestSuite([ TestRC4('test_arm'), TestRC4('test_x86') ])
    unittest.TextTestRunner(verbosity = 2).run(suite)

#
# EoF
#
