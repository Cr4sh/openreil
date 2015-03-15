import sys, os
import gdb
from pyopenreil.REIL import *
from pyopenreil.utils import GDB

class CommandREILTranslate(gdb.Command):

    DEF_ARCH = ARCH_X86

    translate = {  'insn': lambda tr, addr: tr.get_insn(addr), \
                  'block': lambda tr, addr: tr.get_bb(addr),   \
                   'func': lambda tr, addr: tr.get_func(addr) }

    def __init__(self):

        self.arch = self.DEF_ARCH

        super(CommandREILTranslate, self).__init__('reiltrans', gdb.COMMAND_OBSCURE)

    def usage(self):

        print 'USAGE: reiltrans insn|block|func <address> [options]'

    def invoke(self, arg, from_tty):

        args = arg.split(' ')

        if len(arg.strip()) == 0 or len(args) < 2:

            return self.usage()

        mode, addr = args[0], int(args[1], 16)        
        arch = self.DEF_ARCH
        path = None

        if not mode in self.translate.keys():

            return self.usage()        

        for i in range(2, len(args)):

            val = args[i]
            
            if (val == '-a' or val == '--arch') and len(args) >= i + 1:

                arch = args[i + 1]

            elif (val == '-f' or val == '--to-file') and len(args) >= i + 1:    

                path = args[i + 1]

        # initialize OpenREIL stuff        
        reader = GDB.Reader(arch, gdb.selected_inferior())
        tr = CodeStorageTranslator(reader)

        # translate function and enumerate it's basic blocks
        insn_list = self.translate[mode](tr, addr)
        
        if path is not None: 

            # save serialized function IR
            tr.storage.to_file(path)

            print '%d instructions saved to %s' % (len(insn_list), path)

        else:

            # print translated instructions
            print insn_list


CommandREILTranslate()

#
# EoF
#
