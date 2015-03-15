from pyopenreil import REIL
import idc

class Reader(REIL.Reader):

    def __init__(self, arch):

        self.arch = arch

    def read(self, addr, size): 

        return idc.GetManyBytes(addr, size)

    def read_insn(self, addr): 

        return self.read(addr, idc.ItemSize(addr))

#
# EoF
#
