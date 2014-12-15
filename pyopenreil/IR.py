
IATTR_FLAGS  = 'F'
IATTR_NEXT   = 'N'
IATTR_SRC    = 'S'
IATTR_DST    = 'D'

IOPT_CALL       = 0x00000001
IOPT_RET        = 0x00000002
IOPT_BB_END     = 0x00000004
IOPT_ASM_END    = 0x00000008
IOPT_ELIMINATED = 0x00000010

MAX_INST_LEN = 30

REIL_NAMES_INSN = [ 'NONE', 'UNK', 'JCC', 
                    'STR', 'STM', 'LDM', 
                    'ADD', 'SUB', 'NEG', 'MUL', 'DIV', 'MOD', 'SMUL', 'SDIV', 'SMOD', 
                    'SHL', 'SHR', 'AND', 'OR', 'XOR', 'NOT',
                    'EQ', 'NEQ', 'L', 'LE', 'SL', 'SLE' ]

REIL_NAMES_SIZE = [ '1', '8', '16', '32', '64' ]

REIL_NAMES_ARG = [ 'NONE', 'REG', 'TEMP', 'CONST' ]


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

#
# EoF
#

