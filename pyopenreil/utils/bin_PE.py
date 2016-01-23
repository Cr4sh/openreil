import sys, os, unittest
import pefile

file_dir = os.path.abspath(os.path.dirname(__file__))
test_dir = os.path.abspath(os.path.join(file_dir, '..', '..', 'tests'))

from pyopenreil import REIL

class Reader(REIL.Reader):

    def __init__(self, path):

        # load PE image
        self.pe = pefile.PE(path, fast_load = True)
        
        try:

            # get REIL arch by file arch    
            machine = self.pe.FILE_HEADER.Machine
            machine = pefile.MACHINE_TYPE[machine]
        
            self.arch = { 'IMAGE_FILE_MACHINE_I386': REIL.ARCH_X86 }[ machine ]
        
        except KeyError:

            raise Exception('Unsupported architecture')

        super(REIL.Reader, self).__init__()

    def read(self, addr, size): 

        if addr < self.pe.OPTIONAL_HEADER.ImageBase or \
           addr > self.pe.OPTIONAL_HEADER.ImageBase + self.pe.OPTIONAL_HEADER.SizeOfImage:

            # invalid VA
            print 'Reader.read(): Address 0x%x is outside of executable image' % addr
            raise REIL.ReadError(addr)

        # convert VA to RVA
        addr -= self.pe.OPTIONAL_HEADER.ImageBase

        try:

            return self.pe.get_data(rva = addr, length = size)

        except e, why:

            print 'Reader.read(): Exception:', str(why)
            raise REIL.ReadError(addr)

    def read_insn(self, addr): 

        return self.read(addr, REIL.MAX_INST_LEN)


class TestPE(unittest.TestCase):

    BIN_PATH = os.path.join(test_dir, 'fib.exe')
    PROC_ADDR = 0x004016B0 

    def test_reader(self): 

        if os.path.isfile(self.BIN_PATH):

            reader = Reader(self.BIN_PATH)
            tr = REIL.CodeStorageTranslator(reader)

            print tr.get_func(self.PROC_ADDR)

#
# EoF
#
