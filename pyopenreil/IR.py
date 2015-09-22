
# IR instruction attributes
IATTR_ASM   = 0
IATTR_BIN   = 1
IATTR_FLAGS = 2
IATTR_NEXT  = 3
IATTR_SRC   = 4
IATTR_DST   = 5

# IR instruction flags
IOPT_CALL       = 0x00000001
IOPT_RET        = 0x00000002
IOPT_BB_END     = 0x00000004
IOPT_ASM_END    = 0x00000008
IOPT_ELIMINATED = 0x00000010

MAX_INST_LEN = 24

REIL_NAMES_INSN = [ 'NONE', 'UNK',  'JCC', 
                    'STR',  'STM',  'LDM', 
                    'ADD',  'SUB',  'NEG', 'MUL', 'DIV', 'MOD', 'SMUL', 'SDIV', 'SMOD', 
                    'SHL',  'SHR',  'AND', 'OR',  'XOR', 'NOT',
                    'EQ',   'LT' ]

REIL_NAMES_SIZE = [ '1', '8', '16', '32', '64' ]

REIL_NAMES_ARG = [ 'NONE', 'REG', 'TEMP', 'CONST', 'LOC' ]


def create_globals(items, list_name, prefix):

    items_list = globals()[list_name] = []
    num = 0

    for it in items:

        globals()[prefix + str(it)] = num
        items_list.append(num)
        num += 1

# create global constants for REIL opcodes, sizes and argument types
create_globals(REIL_NAMES_INSN, 'REIL_INSN', 'I_')
create_globals(REIL_NAMES_SIZE, 'REIL_SIZE', 'U')
create_globals(REIL_NAMES_ARG, 'REIL_ARG', 'A_')


ARG_TYPE = 0
ARG_SIZE = 1
ARG_LOC  = 1
ARG_NAME = 2
ARG_VAL  = 2

# raw IR arguments helpers
Arg_type = lambda arg: arg[ARG_TYPE] # argument type (see REIL_ARG)
Arg_size = lambda arg: arg[ARG_SIZE] # argument size (see REIL_SIZE)
Arg_name = lambda arg: arg[ARG_NAME] # argument name (for A_REG and A_TEMP)
Arg_val  = lambda arg: arg[ARG_VAL]  # argument value for A_CONST
Arg_loc  = lambda arg: arg[ARG_LOC]  # argument value for A_LOC


INSN_ADDR = 0
INSN_INUM = 1
INSN_OP   = 2
INSN_ARGS = 3
INSN_ATTR = 4

INSN_ADDR_ADDR = 0
INSN_ADDR_SIZE = 1

# raw IR instruction helpers
Insn_addr  = lambda insn: insn[INSN_ADDR][INSN_ADDR_ADDR]   # instruction virtual address
Insn_size  = lambda insn: insn[INSN_ADDR][INSN_ADDR_SIZE]   # assembly code size
Insn_inum  = lambda insn: insn[INSN_INUM]   # IR subinstruction number
Insn_op    = lambda insn: insn[INSN_OP]     # operation code
Insn_args  = lambda insn: insn[INSN_ARGS]   # tuple with 3 arguments
Insn_attr  = lambda insn: insn[INSN_ATTR]   # instruction attributes

# get IR instruction address
Insn_ir_addr = lambda insn: ( Insn_addr(insn), Insn_inum(insn) )

#
# EoF
#

