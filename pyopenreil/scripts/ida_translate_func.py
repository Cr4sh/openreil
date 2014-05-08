import sys, os
import idc
from pyopenreil import REIL
from pyopenreil.utils import IDA

OUTNAME = 'ida_translate_func.ir'

# initialize OpenREIL stuff
reader = IDA.InsnReader()
storage = REIL.InsnStorageMemory()
translator = REIL.Translator('x86', reader, storage)

# translate function and enumerate it's basic blocks
for node in translator.get_func(idc.ScreenEA()):

    # enumerate asm instructions in basic block
    for asm_insn in node.insn_list:
        
        # enumerate ir instructions
        for insn in asm_insn.insn_list:

            insn = REIL.Insn(insn)
            print insn

        print

# save serialized function IR
storage.to_file(OUTNAME)
