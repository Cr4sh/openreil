import sys, os
from sets import Set

from pyopenreil.REIL import *
from pyopenreil.arch import x86

CODE = '\x68\x00\x00\x00\x00\xF3\xA4\xE8\x00\x00\x00\x00\xC2\x04\x00'
ADDR = 0x1337L
ENTRY = 0
        

def test_1(argv):
    ''' Code translation test. '''

    reader = ReaderRaw(CODE, addr = ADDR)
    storage = CodeStorageTranslator('x86', reader)

    cfg = CFGraphBuilder(storage).traverse(ADDR + ENTRY)

    for node in cfg.nodes.values(): print str(node.item) + '\n'


def test_2(argv):
    ''' Code elimination test. '''

    storage = CodeStorageMem()
    storage.from_file('/vagrant_data/_tests/fib/ida_translate_func.ir')

    cfg = CFGraphBuilder(storage).traverse(0x004016B0)

    for node in cfg.nodes.values(): print str(node.item) + '\n'

    dfg = DFGraphBuilder(storage).traverse(0x004016B0)

    deleted_nodes = []

    for edge in list(dfg.exit_node.in_edges):

        arg = edge.node_from.item.dst()[0]
        if (arg.type == A_TEMP) or \
           (arg.type == A_REG and arg.name in x86.Registers.flags):

            print 'Eliminating %s that live at the end of the function...' % arg.name
            dfg.del_edge(edge)

    while True:

        print 'Cleanup...'

        deleted = 0
        for node in dfg.nodes.values():

            if len(node.out_edges) == 0 and node != dfg.exit_node and \
               not node.item.op in [ I_JCC, I_STM ]:

                print 'DFG node "%s" has no output edges' % node
                dfg.del_node(node)
                deleted_nodes.append(node.item.ir_addr)
                deleted += 1

        if deleted == 0: break

    print len(dfg.edges)
    dfg.to_dot_file('test.dot')

    for insn in storage:

        if insn.ir_addr not in deleted_nodes: print insn


if __name__ == '__main__':  

    exit(test_1(sys.argv))

