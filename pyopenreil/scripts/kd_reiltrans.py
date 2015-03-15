import sys, os
import pykd
from pyopenreil.REIL import *
from pyopenreil.utils import kd

DEF_ARCH = ARCH_X86

translate = {  'insn': lambda tr, addr: tr.get_insn(addr), \
              'block': lambda tr, addr: tr.get_bb(addr),   \
               'func': lambda tr, addr: tr.get_func(addr) }

def usage(argv):

    print 'USAGE: %s insn|block|func <address> [options]' % os.path.basename(argv[0])

def main(argv):

    if len(argv) < 3:

        return usage(argv)

    mode, addr = argv[1], pykd.expr(argv[2])        
    arch = DEF_ARCH
    path = None

    if not mode in translate.keys():

        return usage(argv)    

    for i in range(3, len(argv)):

        val = argv[i]

        if (val == '-a' or val == '--arch') and len(argv) >= i + 1:

            arch = argv[i + 1]

        elif (val == '-f' or val == '--to-file') and len(argv) >= i + 1:    

            path = argv[i + 1]

    # initialize OpenREIL stuff   
    reader = kd.Reader(arch)
    tr = CodeStorageTranslator(reader)

    # translate function and enumerate it's basic blocks
    insn_list = translate[mode](tr, addr)
    
    if path is not None: 

        # save serialized function IR
        tr.storage.to_file(path)

        print '%d instructions saved to %s' % (len(insn_list), path)

    else:

        # print translated instructions
        print insn_list

main(sys.argv)

#
# EoF
#

