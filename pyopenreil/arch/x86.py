from pyopenreil.IR import *

# architecture name
name = 'x86'

# size of register
size = U32

# pointer length
ptr_len = 4

class Registers:

    # flag registers
    flags = ( 'R_ZF', 'R_PF', 'R_CF', 'R_AF', 'R_SF', 'R_OF', 'R_DFLAG' )

    # general purpose registers
    general = ( 'R_EAX', 'R_EBX', 'R_ECX', 'R_EDX', 'R_ESI', 'R_EDI', 'R_EBP', 'R_ESP' )

    # instruction pointer
    ip = 'R_EIP'

    # stack pointer
    sp = 'R_ESP'

    # accumulator
    accum = 'R_EAX'

    # all of the registers
    all = (( 'EFLAGS', U32 ),
           ( 'R_LDT', U32 ), 
           ( 'R_GDT', U32 ),
           ( 'R_DFLAG', U32 ),

           ( 'R_CS', U16 ), 
           ( 'R_DS', U16 ), 
           ( 'R_ES', U16 ), 
           ( 'R_FS', U16 ), 
           ( 'R_GS', U16 ), 
           ( 'R_SS', U16 ),

           # Status bit flags
           ( 'R_CF', U1 ),
           ( 'R_CF', U1 ),  
           ( 'R_PF', U1 ),  
           ( 'R_AF', U1 ),  
           ( 'R_ZF', U1 ),  
           ( 'R_SF', U1 ),  
           ( 'R_OF', U1 ),
           ( 'R_CC_OP', U32 ), 
           ( 'R_CC_DEP1', U32 ),  
           ( 'R_CC_DEP2', U32 ),  
           ( 'R_CC_NDEP', U32 ),  

           # other flags
           ( 'R_DFLAG', U32 ),   # Direction Flag
           ( 'R_IDFLAG', U1 ),   # Id flag (support for cpu id instruction)
           ( 'R_ACFLAG', U1 ),   # Alignment check
           ( 'R_EMWARN', U32 ),

           # General purpose 32-bit registers
           ( 'R_EAX', U32 ),  
           ( 'R_EBX', U32 ),  
           ( 'R_ECX', U32 ),  
           ( 'R_EDX', U32 ),  
           ( 'R_ESI', U32 ),  
           ( 'R_EDI', U32 ),  
           ( 'R_EBP', U32 ),  
           ( 'R_ESP', U32 ),  

           # 16-bit registers (bits 0-15)
           ( 'R_AX', U16 ),  
           ( 'R_BX', U16 ),  
           ( 'R_CX', U16 ),  
           ( 'R_DX', U16 ),  
           ( 'R_BP', U16 ),  
           ( 'R_SI', U16 ),  
           ( 'R_DI', U16 ),  
           ( 'R_SP', U16 ),  

           # 8-bit registers (bits 0-7)
           ( 'R_AL', U8 ),  
           ( 'R_BL', U8 ),  
           ( 'R_CL', U8 ),  
           ( 'R_DL', U8 ),  

           # 8-bit registers (bits 8-15)
           ( 'R_AH', U8 ),  
           ( 'R_BH', U8 ),  
           ( 'R_CH', U8 ),  
           ( 'R_DH', U8 ),

           # 32-bit segment base registers
           ( 'R_CS_BASE', U32 ),
           ( 'R_DS_BASE', U32 ),
           ( 'R_ES_BASE', U32 ),
           ( 'R_FS_BASE', U32 ),
           ( 'R_GS_BASE', U32 ),
           ( 'R_SS_BASE', U32 )
        )

#
# EoF
#
