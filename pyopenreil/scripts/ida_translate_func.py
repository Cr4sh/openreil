import sys, os
import idc
from pyopenreil.REIL import *
from pyopenreil.utils import IDA

DEF_ARCH = ARCH_X86

arch = DEF_ARCH
addr = idc.ScreenEA()
path = os.path.join(os.getcwd(), 'sub_%.8X.ir' % addr)

# initialize OpenREIL stuff
reader = IDA.Reader()
storage = CodeStorageMem(arch)
translator = CodeStorageTranslator(arch, reader, storage)

# translate function and enumerate it's basic blocks
cfg = CFGraphBuilder(translator).traverse(addr)

for node in cfg.nodes.values(): print str(node.item) + '\n'

print 'Saving instructions into the %s' % path

# save serialized IR of the function
storage.to_file(path)
