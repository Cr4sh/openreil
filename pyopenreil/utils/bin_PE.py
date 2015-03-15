import sys, os, unittest

file_dir = os.path.abspath(os.path.dirname(__file__))
test_dir = os.path.abspath(os.path.join(file_dir, '..', '..', 'tests'))

from pyopenreil import REIL
import pefile

MAX_INST_LEN = 30

class Reader(REIL.Reader):

    def __init__(self, path):

        # load PE image
        self.pe = pefile.PE(path, fast_load = True)

        super(REIL.Reader, self).__init__()

    def read(self, addr, size): 

        if addr < self.pe.OPTIONAL_HEADER.ImageBase or \
           addr > self.pe.OPTIONAL_HEADER.ImageBase + self.pe.OPTIONAL_HEADER.SizeOfImage:

            # invalid VA
            return None

        # convert VA to RVA
        addr -= self.pe.OPTIONAL_HEADER.ImageBase

        return self.pe.get_data(rva = addr, length = size)

    def read_insn(self, addr): 

        return self.read(addr, MAX_INST_LEN)


class TestPE(unittest.TestCase):

    BIN_PATH = os.path.join(test_dir, 'fib.exe')
    PROC_ADDR = 0x004016B0 

    def test_reader(self): 

        if os.path.isfile(self.BIN_PATH):

            reader = Reader(self.BIN_PATH)
            tr = REIL.CodeStorageTranslator(REIL.ARCH_X86, reader)

            print tr.get_func(self.PROC_ADDR)

#
# EoF
#
