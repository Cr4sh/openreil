import sys, os, unittest

file_dir = os.path.abspath(os.path.dirname(__file__))
test_dir = os.path.abspath(os.path.join(file_dir, '..', '..', 'tests'))

from pyopenreil import REIL
from pybfd.bfd import Bfd

MAX_INST_LEN = 30

class Reader(REIL.Reader):

    def __init__(self, path):

        # load image
        self.bfd = Bfd(path)

        super(REIL.Reader, self).__init__()

    def read(self, addr, size): 

        for sec in self.bfd.sections.values():

            # lookup for image section by address
            if addr >= sec.vma and addr < sec.vma + sec.size:

                # return section data
                addr -= sec.vma
                return sec.content[addr : addr + size]

        raise Exception('Unable to find image section for address 0x%x' % addr)


    def read_insn(self, addr): 

        return self.read(addr, MAX_INST_LEN)


class TestBFD(unittest.TestCase):

    BIN_PATH = os.path.join(test_dir, 'fib')
    PROC_ADDR = 0x08048434

    def test_reader(self): 
        
        reader = Reader(self.BIN_PATH)
        tr = REIL.CodeStorageTranslator(REIL.ARCH_X86, reader)

        print tr.get_func(self.PROC_ADDR)

#
# EoF
#
