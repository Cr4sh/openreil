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

#
# EoF
#
