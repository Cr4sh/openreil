import sys, os
from pyopenreil.REIL import *

CODE = '\x68\x00\x00\x00\x00\xE8\x00\x00\x00\x00\xC2\x04\x00'
ADDR = 0x1337L
ENTRY = 0

def test_1(argv):
    ''' Code translation test. '''

    storage = StorageMemory()
    reader = ReaderRaw(CODE, addr = ADDR)        
    translator = Translator('x86', reader, storage)

    translator.process_func(ADDR + ENTRY)

    for insn in storage: print insn


def test_2(argv):
    ''' Code analysis test. '''

    storage = StorageMemory()
    storage.from_file('/vagrant_data/_tests/fib/ida_translate_func.ir')

    def visitor(insn_list):
    
        for insn in insn_list: print insn
        print

        return True

    CfgParser(storage, visitor).traverse(0x004016B0)


if __name__ == '__main__':  

    exit(test_2(sys.argv))

