import sys, os, unittest
import pybfd.bfd

file_dir = os.path.abspath(os.path.dirname(__file__))
test_dir = os.path.abspath(os.path.join(file_dir, '..', '..', 'tests'))

from pyopenreil import REIL

class Reader(REIL.Reader):

    def __init__(self, path, arch = None):

        # load image
        self.bfd = pybfd.bfd.Bfd(path)
        self.arch = arch        

        if self.arch is None:

            if self.bfd.architecture == 0:

                # because bfd is piece of shit
                raise Exception('Unknown bfd architecture, you also may try to specify it manually')

            try:

                # get REIL arch by file arch
                # FIXME: check for arm as well
                self.arch = { pybfd.bfd.ARCH_I386: REIL.ARCH_X86 }[ self.bfd.architecture ]
            
            except KeyError:

                raise Exception('Unsupported architecture %s' % self.bfd.architecture_name)

        super(REIL.Reader, self).__init__()

    def read(self, addr, size): 

        for sec in self.bfd.sections.values():

            # lookup for image section by address
            if addr >= sec.vma and addr < sec.vma + sec.size:

                # return section data
                addr -= sec.vma
                return sec.content[addr : addr + size]

        # invalid VA
        print 'Reader.read(): Address 0x%x is outside of executable image' % addr
        raise REIL.ReadError(addr)

    def read_insn(self, addr): 

        return self.read(addr, REIL.MAX_INST_LEN)


class TestBFD(unittest.TestCase):

    BIN_PATH = os.path.join(test_dir, 'fib')
    PROC_ADDR = 0x08048434

    def test_reader(self): 

        if os.path.isfile(self.BIN_PATH):
        
            reader = Reader(self.BIN_PATH)
            tr = REIL.CodeStorageTranslator(reader)

            print tr.get_func(self.PROC_ADDR)

#
# EoF
#
