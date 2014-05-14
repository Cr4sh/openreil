import sys, os
from pyopenreil.REIL import *

CODE = '\x68\x00\x00\x00\x00\xF3\xA4\xE8\x00\x00\x00\x00\xC2\x04\x00'
ADDR = 0x1337L
ENTRY = 0

def visitor(insn_list):
    
    print '\n'.join(map(lambda insn: str(insn), insn_list)) + '\n'
    return True

def test_1(argv):
    ''' Code translation test. '''

    storage = StorageMemory()
    reader = ReaderRaw(CODE, addr = ADDR)        

    translator = Translator('x86', reader, storage)
    translator.process_func(ADDR + ENTRY)    

    CfgParser(storage, visitor).traverse(ADDR + ENTRY)


def test_2(argv):
    ''' Code analysis test. '''

    storage = StorageMemory()
    storage.from_file('/vagrant_data/_tests/fib/ida_translate_func.ir')

    CfgParser(storage, visitor).traverse(0x004016B0)


if __name__ == '__main__':  

    exit(test_1(sys.argv))

