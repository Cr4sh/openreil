import sys, os, unittest
from subprocess import *

file_dir = os.path.abspath(os.path.dirname(__file__))
reil_dir = os.path.abspath(os.path.join(file_dir, '..'))
if not reil_dir in sys.path: sys.path = [ reil_dir ] + sys.path

from pyopenreil.REIL import *
from pyopenreil.VM import *

try:

    # check for pefile module (required for loading test PE binaries)
    import pefile

    # load unit tests that depends on pefile
    from pyopenreil.utils.bin_PE import *
    from test_fib import *
    from test_rc4 import *
    
except ImportError, why: print '[!]', str(why)

try:

    # check for BFD python module (required for loading ELF binaries)
    import pybfd

    # load unit tests that depends on pybfd
    from pyopenreil.utils.bin_BFD import *

except ImportError, why: print '[!]', str(why)

def check_nasm():

    from pyopenreil.utils import asm

    try:

        p = Popen([ asm.NASM_PATH, '-h' ], stdout = PIPE, stderr = PIPE)
        stdout, stderr = p.communicate()
        
        p.stdout.close()
        p.stderr.close()

        if p.returncode != 0:

            raise OSError('Process returned %d' % p.returncode)

        return True

    except OSError, why:

        print str(why)
        print 'check_nasm(): Error while executing "%s"' % asm.NASM_PATH        
        return False

def check_numpy():

    try:
        
        import numpy
        return True

    except ImportError, why:

        print str(why)
        print 'check_numpy(): Error while loading numpy'
        return False

def main():

    ok = True

    # check for required programs and modules
    if not check_nasm(): ok = False
    if not check_numpy(): ok = False

    if ok:

        unittest.main(verbosity = 2)

    else:

        print 'Unable to run tests, check for installed nasm and python-numpy'

if __name__ == '__main__':  

    main()

#
# EoF
#

