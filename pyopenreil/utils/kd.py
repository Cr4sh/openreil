from pyopenreil import REIL
import pykd

class Reader(REIL.Reader):

    def __init__(self, arch):

        self.arch = arch

    def read(self, addr, size): 

        return pykd.loadChars(addr, size)

    def read_insn(self, addr): 

        return self.read(addr, REIL.MAX_INST_LEN)

#
# EoF
#
