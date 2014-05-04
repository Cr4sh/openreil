import sys, os
from pyopenreil import REIL

CODE = '\x68\x00\x00\x00\x00\xE8\x00\x00\x00\x00\xC2\x04\x00'
ADDR = 1337L
ENTRY = 0

def main(argv):

    reader = REIL.RawInsnReader(CODE, addr = ADDR)    
    translator = REIL.Translator('x86', reader, REIL.InsnStorageMemory())

    for bb_start, _ in translator.get_func(ADDR + ENTRY):

        for insn in translator.get_bb(bb_start):
            
            insn = REIL.Insn(insn)
            print insn

if __name__ == '__main__':  

    exit(main(sys.argv))

