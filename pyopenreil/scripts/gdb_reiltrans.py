import sys, os
import gdb
from pyopenreil.REIL import *
from pyopenreil.utils import GDB

class CommandREILTranslate(gdb.Command):

    DEF_ARCH = 'x86'

    translate = {  'insn': lambda t, a: t.get_insn(a), \
                  'block': lambda t, a: t.get_bb(a),   \
                   'func': lambda t, a: t.get_func(a)  }

    def __init__(self):

        self.arch = self.DEF_ARCH

        super(CommandREILTranslate, self).__init__('reiltrans', gdb.COMMAND_OBSCURE)

    def usage(self):

        print 'USAGE: reiltrans insn|block|func <address> [options]'

    def invoke(self, arg, from_tty):

        args = arg.split(' ')
        if len(arg.strip()) == 0 or len(args) < 2:

            return self.usage()

        mode = args[0]
        addr = int(args[1], 16)        

        if not mode in self.translate.keys():

            return self.usage()

        arch = self.DEF_ARCH
        path = None

        for i in range(2, len(args)):

            val = args[i]
            if (val == '-a' or val == '--arch') and len(args) >= i + 1:

                arch = args[i + 1]

            elif (val == '-f' or val == '--to-file') and len(args) >= i + 1:    

                path = args[i + 1]

        # initialize OpenREIL stuff        
        storage = CodeStorageMem(arch)
        reader = GDB.Reader(gdb.selected_inferior())
        translator = CodeStorageTranslator(arch, reader, storage)        

        # translate function and enumerate it's basic blocks
        insn_list = self.translate[mode](translator, addr)
        
        if path is not None: 

            # save serialized function IR
            storage.to_file(path)

            print '%d instructions saved to %s' % (len(insn_list), path)

        else:

            # print translated instructions
            for insn in insn_list: print str(insn)

        

CommandREILTranslate()

