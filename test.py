import sys, os
from sets import Set

from pyopenreil.REIL import *
from pyopenreil.arch import x86

CODE  = '\x0f\x32\x0f\x30' # rdmsr, wrmsr
CODE += '\x0f\x01\x08\x0f\x01\x00\x0f\x00\x00' # sidt, sgdt, sldt
CODE += '\x0f\x01\x10\x0f\x01\x18\x0f\x00\x10' # lidt, lgdt, lldt
CODE += '\x0f\x31' # rdtsc
CODE += '\x0f\xa2' # cupid
CODE += '\xcc' # int 3
CODE += '\x90' # nop
CODE += '\xf4' # hlt
CODE += '\xc3\x68\x00\x00\x00\x00\xF3\xA4\xE8\x00\x00\x00\x00\xC2\x04\x00'

ADDR = 0x1337L
ENTRY = 0        

CODE = '\xe2\xf4'

def test_1(argv):
    ''' Code translation test. '''

    reader = ReaderRaw(CODE, addr = ADDR)
    storage = CodeStorageTranslator('x86', reader)

    for insn in storage.get_insn(ADDR + ENTRY): print str(insn)

    #cfg = CFGraphBuilder(storage).traverse(ADDR + ENTRY)
    #for node in cfg.nodes.values(): print str(node.item) + '\n'


def test_2(argv):
    ''' Code elimination test. '''

    storage = CodeStorageMem('x86')
    storage.from_file('/vagrant_data/_tests/fib/ida_translate_func.ir')

    cfg = CFGraphBuilder(storage).traverse(0x004016B0)

    for node in cfg.nodes.values(): 

        print str(node.item) + '\n'

    dfg = DFGraphBuilder(storage).traverse(0x004016B0)

    deleted_nodes = dfg.eliminate_dead_code()

    dfg.to_dot_file('test.dot')

    for insn in storage:

        if insn.ir_addr not in deleted_nodes: print insn


if __name__ == '__main__':  

    exit(test_1(sys.argv))

