import sys, os
from pyopenreil.REIL import *

CODE = '\x68\x00\x00\x00\x00\xF3\xA4\xE8\x00\x00\x00\x00\xC2\x04\x00'
ADDR = 0x1337L
ENTRY = 0

def test_1(argv):
    ''' Code translation test. '''

    reader = ReaderRaw(CODE, addr = ADDR)

    cfg = CfgParser(CodeStorageTranslator('x86', reader))
    bb_list = cfg.traverse(ADDR + ENTRY)

    for bb in bb_list: print str(bb) + '\n'


def test_2(argv):
    ''' Code analysis test. '''

    storage = CodeStorageMem()
    storage.from_file('/vagrant_data/_tests/fib/ida_translate_func.ir')

    cfg = CfgParser(storage)
    bb_list = cfg.traverse(0x004016B0)

    for bb in bb_list: print str(bb) + '\n'

if __name__ == '__main__':  

    exit(test_2(sys.argv))

