from pyopenreil import REIL
from pyasm2 import x86 as pyasm2

class Reader(REIL.ReaderRaw):

    def __init__(self, addr, *args):

        block = pyasm2.block(*args)

        super(Reader, self).__init__(str(block), addr = addr)

#
# EoF
#
