from pyopenreil import REIL
import pykd

MAX_INST_LEN = 30

class Reader(REIL.Reader):

    def read(self, addr, size): 

        return pykd.loadChars(addr, size)

    def read_insn(self, addr): 

        return self.read(addr, MAX_INST_LEN)

#
# EoF
#
