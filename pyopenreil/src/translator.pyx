cimport libopenreil

IATTR_ASM = 'A'
IATTR_BIN = 'B'
IATTR_FLAGS = 'F'

cdef arg_handler(libopenreil._reil_arg_t arg):

    # convert reil_arg_t to the python tuple
    if arg.type == libopenreil.A_NONE: return ()
    elif arg.type == libopenreil.A_REG: return ( arg.type, arg.size, arg.name )
    elif arg.type == libopenreil.A_TEMP: return ( arg.type, arg.size, arg.name )
    elif arg.type == libopenreil.A_CONST: return ( arg.type, arg.size, arg.val )

cdef int inst_handler(libopenreil.reil_inst_t* inst, object context):

    attr = {}    

    cdef unsigned char* data
    cdef char* str_mnem
    cdef char* str_op
    
    if inst.flags != 0: 

        attr[IATTR_FLAGS] = inst.flags

    if inst.inum == 0:

        data = inst.raw_info.data
        str_mnem = inst.raw_info.str_mnem
        str_op = inst.raw_info.str_op

        if data != NULL:

            # instruction bytes
            attr[IATTR_BIN] = (<char *>data)[:inst.raw_info.size]

        if str_mnem != NULL and str_op != NULL:

            # assembly instruction
            attr[IATTR_ASM] = ( str(str_mnem), str(str_op) )
    
    # convert reil_inst_t to the python tuple
    raw_info = ( inst.raw_info.addr, inst.raw_info.size )    
    args = ( arg_handler(inst.a), arg_handler(inst.b), arg_handler(inst.c) )
    
    # put instruction into the list
    context.insert(0, ( raw_info, inst.inum, inst.op, args, attr ))

    return 1
    

class Error(Exception):

    def __init__(self, msg):

        self.msg = msg        

    def __str__(self):

        return self.msg


class TranslationError(Error):

    def __init__(self, addr):

        self.addr = addr

    def __str__(self):

        return 'Error while translating instruction %s to REIL' % hex(self.addr)


cdef class Translator:

    cdef libopenreil.reil_t reil
    cdef libopenreil.reil_arch_t reil_arch
    translated = []

    def __init__(self, arch):
    
        if arch == 'x86': self.reil_arch = libopenreil.REIL_X86
        else: raise(Exception('Unknown architecture'))

        # initialize translator
        self.reil = libopenreil.reil_init(self.reil_arch, 
            <libopenreil.reil_inst_handler_t>inst_handler, <void*>self.translated)

    def __del__(self):

        libopenreil.reil_close(self.reil)

    def to_reil(self, data, addr = 0):

        ret = []
        cdef unsigned char* c_data = data
        cdef int c_size = len(data)

        # translate specified binary code
        num = libopenreil.reil_translate_insn(self.reil, addr, c_data, c_size)
        if num == -1: 

            raise(TranslationError(addr))

        # collect translated instructions
        while len(self.translated) > 0: ret.append(self.translated.pop())
        return ret

