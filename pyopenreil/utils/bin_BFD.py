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

#
# EoF
#
