from pyopenreil import REIL

MAX_INST_LEN = 30

class Reader(REIL.Reader):

    def __init__(self, inferior):

        self.inferior = inferior

        super(REIL.Reader, self).__init__()

    def read(self, addr, size): 

        return str(self.inferior.read_memory(addr, size))

    def read_insn(self, addr): 

        return self.read(addr, MAX_INST_LEN)

#
# EoF
#
