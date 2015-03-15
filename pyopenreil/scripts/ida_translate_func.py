import sys, os
import idc
from pyopenreil.REIL import *
from pyopenreil.utils import IDA

DEF_ARCH = ARCH_X86

# get address of the current function
addr = idc.ScreenEA()

arch = DEF_ARCH
path = os.path.join(os.getcwd(), 'sub_%.8X.ir' % addr)

# initialize OpenREIL stuff
reader = IDA.Reader(arch)
storage = CodeStorageMem(arch)
tr = CodeStorageTranslator(reader, storage)

# translate function and enumerate it's basic blocks
func = tr.get_func(addr)

for bb in func.bb_list: print bb

print '[*] Saving instructions into the %s' % path
print '[*] %d IR instructions in %d basic blocks translated' % (len(func), len(func.bb_list))

# save function IR into the JSON file
storage.to_file(path)
