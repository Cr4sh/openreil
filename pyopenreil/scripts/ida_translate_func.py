import sys, os
import idc
from pyopenreil.REIL import *
from pyopenreil.utils import IDA

OUTNAME = 'ida_translate_func.ir'
DEF_ARCH = 'x86'

arch = DEF_ARCH

# initialize OpenREIL stuff
reader = IDA.Reader()
storage = CodeStorageMem(arch)
translator = CodeStorageTranslator(arch, reader, storage)

# translate function and enumerate it's basic blocks
cfg = CFGraphBuilder(translator).traverse(idc.ScreenEA())

for node in cfg.nodes.values(): print str(node.item) + '\n'

# save serialized function IR
storage.to_file(OUTNAME)
