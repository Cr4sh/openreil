from pyopenreil.IR import *

# architecture name
name = 'arm'

# size of register
size = U32

# pointer length
ptr_len = 4

class Registers:

    # flag registers
    flags = ( 'R_NF', 'R_ZF', 'R_CF', 'R_VF' )

    # general purpose registers
    general = ( 'R_R0', 'R_R1', 'R_R2',  'R_R3',  'R_R4',  'R_R5',  'R_R6',  'R_R7', 
                'R_R8', 'R_R9', 'R_R10', 'R_R11', 'R_R12', 'R_R13', 'R_R14', 'R_R15T' )

    # instruction pointer
    ip = 'R_R15T'

    # stack pointer
    sp = 'R_R13'

    # link register
    lr = 'R_R14'

    # accumulator
    accum = 'R_R0'

    # all of the registers
    all = (( 'R_R0', U32 ),
           ( 'R_R1', U32 ),
           ( 'R_R2', U32 ),
           ( 'R_R3', U32 ),
           ( 'R_R4', U32 ),
           ( 'R_R5', U32 ),
           ( 'R_R6', U32 ),
           ( 'R_R7', U32 ),
           ( 'R_R8', U32 ),
           ( 'R_R9', U32 ),
           ( 'R_R10', U32 ),
           ( 'R_R11', U32 ),
           ( 'R_R12', U32 ),
           ( 'R_R13', U32 ),
           ( 'R_R14', U32 ),
           ( 'R_R15T', U32 ),

           ( 'R_CC_OP', U32 ), 
           ( 'R_CC_DEP1', U32 ),  
           ( 'R_CC_DEP2', U32 ),  
           ( 'R_CC_NDEP', U32 ),

           ( 'R_ITCOND', U1 )
        )

#
# EoF
#
