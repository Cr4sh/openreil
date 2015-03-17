import sys, os, unittest

file_dir = os.path.abspath(os.path.dirname(__file__))
reil_dir = os.path.abspath(os.path.join(file_dir, '..'))
if not reil_dir in sys.path: sys.path = [ reil_dir ] + sys.path

from pyopenreil.REIL import *
from pyopenreil.VM import *
from pyopenreil.utils import bin_PE

class TestRC4(unittest.TestCase):

    ARCH = ARCH_X86
    BIN_PATH = os.path.join(file_dir, 'rc4.exe')

    # VA's of the test rc4.exe procedures
    RC4_SET_KEY = 0x004016D5 
    RC4_CRYPT = 0x004017B5

    def test(self):        

        # test input data for RC4 encryption
        test_key = 'somekey'
        test_val = 'bar'

        # load PE image of test program
        reader = bin_PE.Reader(self.BIN_PATH)
        tr = CodeStorageTranslator(reader)

        def code_optimization(addr):
     
            # construct dataflow graph for given function
            dfg = DFGraphBuilder(tr).traverse(addr)
            
            # run some basic optimizations
            dfg.eliminate_dead_code()
            dfg.constant_folding()   
            dfg.eliminate_subexpressions()     

            # store resulting instructions
            dfg.store(tr.storage)  

        code_optimization(self.RC4_SET_KEY)
        code_optimization(self.RC4_CRYPT)

        # create CPU and ABI
        cpu = Cpu(self.ARCH)
        abi = Abi(cpu, tr)

        # allocate buffers for arguments of emulated functions
        ctx = abi.buff(256 + 4 * 2)
        val = abi.buff(test_val)

        # emulate rc4_set_key() function call
        abi.cdecl(self.RC4_SET_KEY, ctx, test_key, len(test_key))

        # emulate rc4_crypt() function call
        abi.cdecl(self.RC4_CRYPT, ctx, val, len(test_val))
        
        # read results of RC4 encryption
        val = abi.read(val, len(test_val))
        print 'Encrypted value:', repr(val)

        # check for correct result
        assert val == '\x38\x88\xBC'


if __name__ == '__main__':    

    suite = unittest.TestSuite([ TestRC4('test') ])
    unittest.TextTestRunner(verbosity = 2).run(suite)

#
# EoF
#
