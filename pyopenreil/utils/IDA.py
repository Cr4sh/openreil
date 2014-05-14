from pyopenreil import REIL
import idc

class Reader(REIL.Reader):

    def read(self, addr, size): 

        return idc.GetManyBytes(addr, size)

    def read_insn(self, addr): 

        return self.read(addr, idc.ItemSize(addr))

#
# EoF
#
