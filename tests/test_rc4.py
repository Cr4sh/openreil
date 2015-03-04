import sys, os, unittest

file_dir = os.path.abspath(os.path.dirname(sys.argv[0]))
reil_dir = os.path.abspath(os.path.join(file_dir, '..'))
if not reil_dir in sys.path: sys.path.append(reil_dir)

from pyopenreil.REIL import *
from pyopenreil.VM import *

from pyopenreil.arch import x86
from pyopenreil.utils import bin_PE

class TestRC4(unittest.TestCase):

    BIN_PATH = os.path.join(file_dir, 'rc4.exe')

    def test(self):        
    
        # rc4.exe VA's of the rc4_set_key() and rc4_crypt()
        rc4_set_key = 0x004016D5
        rc4_crypt = 0x004017B5

        # test input data for encryption
        test_key = 'somekey'
        test_val = 'bar'

        # load PE image
        reader = bin_PE.Reader(self.BIN_PATH)
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

        assert val_1 == val_2


if __name__ == '__main__':    

    suite = unittest.TestSuite([ TestRC4('test') ])
    unittest.TextTestRunner(verbosity = 2).run(suite)

#
# EoF
#
