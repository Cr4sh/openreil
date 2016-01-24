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
    from pyopenreil.utils.bin_PE import *    
    
    # check for Z3 module (required for test_kao.py)
    import z3

    # load unit tests that depends on Z3
    from test_kao import TestKao
    
except ImportError, why: print '[!]', str(why)

try:

    # check for BFD python module (required for loading ELF binaries)
    import pybfd

    # load unit tests that depends on pybfd
    from pyopenreil.utils.bin_BFD import *

except ImportError, why: print '[!]', str(why)

# load .c unit tests
from test_fib import *
from test_rc4 import *
from test_md5 import *

def check_program(command, code = 0):

    try:

        p = Popen(command, stdout = PIPE, stderr = PIPE)
        stdout, stderr = p.communicate()
        
        p.stdout.close()
        p.stderr.close()

        if p.returncode != code:

            raise OSError('Process returned %d' % p.returncode)

        return True

    except OSError, why:

        print str(why)
        print 'Error while executing "%s"' % command[0]
        return False

def check_nasm():

    from pyopenreil.utils import asm

    return check_program([ asm.CompilerNasm.nasm_path, '-h' ])

def check_objcopy():

    return check_program([ 'objcopy', '--help' ])

def check_otool():

    return check_program([ 'otool' ], code = 1)

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
    if not check_numpy(): ok = False

    if sys.platform == 'darwin':

        ok = check_otool()

    else:

        og = check_objcopy()

    if ok:

        unittest.main(verbosity = 2)

    else:

        print 'Unable to run tests, check for installed as, objcopy/otool and python-numpy'

if __name__ == '__main__':  

    main()

#
# EoF
#

