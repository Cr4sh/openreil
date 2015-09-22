
DEF REIL_MAX_NAME_LEN = 15
DEF REIL_ERROR = -1

cdef extern from "libopenreil.h":    

    cdef enum _reil_op_t:

        I_NONE,     # no operation
        I_UNK,      # unknown instruction
        I_JCC,      # conditional jump 
        I_STR,      # store value to register
        I_STM,      # store value to memory
        I_LDM,      # load value from memory
        I_ADD,      # addition
        I_SUB,      # substraction
        I_NEG,      # negation
        I_MUL,      # multiplication
        I_DIV,      # division
        I_MOD,      # modulus    
        I_SMUL,     # signed multiplication
        I_SDIV,     # signed division
        I_SMOD,     # signed modulus
        I_SHL,      # shift left
        I_SHR,      # shift right
        I_AND,      # binary and
        I_OR,       # binary or
        I_XOR,      # binary xor
        I_NOT,      # binary not 
        I_EQ,       # equation
        I_LT        # less than

    cdef enum _reil_type_t:

        A_NONE,
        A_REG,      # target architecture registry operand
        A_TEMP,     # temporary registry operand
        A_CONST,    # immediate value    
        A_LOC       # jump location

    ctypedef unsigned long long reil_const_t
    ctypedef unsigned long long reil_addr_t
    ctypedef unsigned short reil_inum_t

    cdef enum _reil_size_t: U1, U8, U16, U32, U64

    cdef struct _reil_arg_t:
    
        _reil_type_t type
        _reil_size_t size
        reil_const_t val
        reil_inum_t inum
        char name[REIL_MAX_NAME_LEN]

    cdef struct _reil_raw_t:

        reil_addr_t addr      # address of the original assembly instruction
        int size              # ... and it's size
        unsigned char *data   # pointer to the instruction bytes
        char *str_mnem        # instruction mnemonic
        char *str_op          # instruction operands

    cdef struct _reil_inst_t:

        _reil_raw_t raw_info
        reil_inum_t inum      # number of the IR subinstruction
        _reil_op_t op         # operation code
        _reil_arg_t a, b, c   # arguments    
        unsigned long long flags
    
    ctypedef void* reil_t
    ctypedef void* reil_inst_handler_t    
    
    cdef enum _reil_arch_t: 

        ARCH_X86, 
        ARCH_ARM

    ctypedef _reil_arg_t reil_arg_t
    ctypedef _reil_inst_t reil_inst_t
    ctypedef _reil_arch_t reil_arch_t

    int reil_log_init(int mask, char *path)
    void reil_log_close()

    int reil_translate_insn(reil_t reil, reil_addr_t addr, unsigned char *buff, int len)
    reil_t reil_init(reil_arch_t arch, reil_inst_handler_t handler, void *context)
    void reil_close(reil_t reil) 
