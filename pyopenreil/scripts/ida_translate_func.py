import sys, os
import idc
from pyopenreil.REIL import *
from pyopenreil.utils import IDA

OUTNAME = 'ida_translate_func.ir'

# initialize OpenREIL stuff
reader = IDA.Reader()
storage = StorageMemory()
translator = Translator('x86', reader, storage)

# translate function and enumerate it's basic blocks
translator.process_func(idc.ScreenEA())

for insn in storage: print insn

# save serialized function IR
storage.to_file(OUTNAME)
