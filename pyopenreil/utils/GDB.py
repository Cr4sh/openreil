from pyopenreil import REIL

class Reader(REIL.Reader):

    def __init__(self, arch, inferior):

        self.arch = arch
        self.inferior = inferior        

        super(REIL.Reader, self).__init__()

    def read(self, addr, size): 

        return str(self.inferior.read_memory(addr, size))

    def read_insn(self, addr): 

        return self.read(addr, REIL.MAX_INST_LEN)

#
# EoF
#
