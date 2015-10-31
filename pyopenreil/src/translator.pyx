cimport libopenreil

ARCH_X86 = 0
ARCH_ARM = 1

# IR instruction attributes
IATTR_ASM = 0
IATTR_BIN = 1
IATTR_FLAGS = 2

# log_init() mask constants
LOG_INFO = 0x00000001 # regular message
LOG_WARN = 0x00000002 # error
LOG_ERR = 0x00000004 # warning
LOG_BIN = 0x00000008 # instruction bytes
LOG_ASM = 0x00000010 # instruction assembly code
LOG_VEX = 0x00000020 # instruction VEX code
LOG_BIL = 0x00000040 # instruction BAP IL code

# enable all debug messages
LOG_ALL = 0x7FFFFFFF

# disable all debug messages
LOG_NONE = 0

# default log messages mask
LOG_MASK_DEFAULT = LOG_INFO | LOG_WARN | LOG_ERR

cdef process_arg(libopenreil._reil_arg_t arg):

    # convert reil_arg_t to the python tuple
    if arg.type == libopenreil.A_NONE: 

        return ()
    
    elif arg.type == libopenreil.A_REG: 

        return ( arg.type, arg.size, arg.name )
    
    elif arg.type == libopenreil.A_TEMP: 

        return ( arg.type, arg.size, arg.name )
    
    elif arg.type == libopenreil.A_CONST: 

        return ( arg.type, arg.size, arg.val )

    elif arg.type == libopenreil.A_LOC: 

        return ( arg.type, ( arg.val, arg.inum ))

cdef int process_insn(libopenreil.reil_inst_t* inst, object context):

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
    args = ( process_arg(inst.a), process_arg(inst.b), process_arg(inst.c) )
    
    # put instruction into the list
    context.insert(0, ( raw_info, inst.inum, inst.op, args, attr ))

    return 1
    

class Error(Exception):

    def __init__(self, msg):

        self.msg = msg        

    def __str__(self):

        return self.msg


class InitError(Error):

    pass


class TranslationError(Error):

    def __init__(self, addr):

        self.addr = addr

    def __str__(self):

        return 'Error while translating instruction %s to REIL' % hex(self.addr)


cdef class Translator:

    cdef libopenreil.reil_t reil
    cdef libopenreil.reil_arch_t reil_arch 
    cdef char* log_path   
    translated = []

    def __init__(self, arch, log_path = None, log_mask = LOG_MASK_DEFAULT):
    
        self.reil_arch = self.get_reil_arch(arch)
        
        if log_path is None:
        
            self.log_path = NULL

        else:
        
            self.log_path = log_path

        # initialize logging
        libopenreil.reil_log_init(log_mask, self.log_path)

        # initialize translator
        self.reil = libopenreil.reil_init(self.reil_arch, 
            <libopenreil.reil_inst_handler_t>process_insn, <void*>self.translated)

        if self.reil == NULL:

            raise InitError('Error while initializing REIL translator')

    def __del__(self):

        libopenreil.reil_close(self.reil)
        libopenreil.reil_log_close()

    def get_reil_arch(self, arch):

        try: 

            return { ARCH_X86: libopenreil.ARCH_X86, 
                     ARCH_ARM: libopenreil.ARCH_ARM }[ arch ]

        except KeyError: 

            raise InitError('Unknown architecture')

    def to_reil(self, data, addr = 0):

        ret = []
        cdef unsigned char* c_data = data
        cdef int c_size = len(data)

        # translate specified binary code
        num = libopenreil.reil_translate_insn(self.reil, addr, c_data, c_size)
        if num == -1: 

            raise TranslationError(addr)

        # collect translated instructions
        while len(self.translated) > 0: ret.append(self.translated.pop())
        return ret

