import sys, os
from pyopenreil import REIL

CODE = '\x68\x00\x00\x00\x00\xE8\x00\x00\x00\x00\xC2\x04\x00'
ADDR = 0x1337L
ENTRY = 0

def test_1(argv):

    # initialize OpenREIL stuff
    reader = REIL.RawInsnReader(CODE, addr = ADDR)    
    storage = REIL.InsnStorageMemory()
    translator = REIL.Translator('x86', reader, storage)

    # translate function and enumerate it's basic blocks
    for node in translator.get_func(ADDR + ENTRY):

        print node
        print

    # save serialized function IR
    storage.to_file('test.ir')


def test_3(argv):

    # initialize OpenREIL stuff
    reader = REIL.RawInsnReader('\x03\xC1', addr = ADDR)    
    storage = REIL.InsnStorageMemory()
    translator = REIL.Translator('x86', reader, storage)

    asm = translator.get_insn(ADDR + ENTRY)
    print asm

    print 'DEF:', asm.get_def_reg()
    print 'USE:', asm.get_use_reg()
    print

    asm.eliminate_defs([ 'R_PF', 'R_OF', 'R_AF'])
    print asm
    
if __name__ == '__main__':  

    exit(test_1(sys.argv))

