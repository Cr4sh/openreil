#include <stdio.h>
#include <string>
#include <vector>

extern "C"
{
// XED header files
#include "xed-interface.h"
}

#include "libvex.h"

#include "irtoir.h"
#include "irtoir-internal.h"

#define DISASM_MAX_INST_LEN 30

using namespace std;

#include "disasm.h"

typedef enum _dsiasm_arg_t
{
    disasm_arg_t_src,
    disasm_arg_t_dst

} dsiasm_arg_t;

static bool xed_tables_ready = false;

Temp *i386_disasm_arg_to_temp(xed_reg_enum_t arg)
{
    switch (arg)
    {
    case XED_REG_CR0:
        {
            break;
        }

    case XED_REG_CR1:
        {
            break;
        }

    case XED_REG_CR2:
        {
            break;
        }

    case XED_REG_CR3:
        {
            break;
        }

    case XED_REG_CR4:
        {
            break;
        }

    case XED_REG_CR5:
        {
            break;
        }

    case XED_REG_CR6:
        {
            break;
        }

    case XED_REG_CR7:
        {
            break;
        }

    case XED_REG_CR8:
        {
            break;
        }

    case XED_REG_CR9:
        {
            break;
        }

    case XED_REG_CR10:
        {
            break;
        }

    case XED_REG_CR11:
        {
            break;
        }

    case XED_REG_CR12:
        {
            break;
        }

    case XED_REG_CR13:
        {
            break;
        }

    case XED_REG_CR14:
        {
            break;
        }

    case XED_REG_CR15:
        {
            break;
        }

    case XED_REG_DR0:
        {
            break;
        }

    case XED_REG_DR1:
        {
            break;
        }

    case XED_REG_DR2:
        {
            break;
        }

    case XED_REG_DR3:
        {
            break;
        }

    case XED_REG_DR4:
        {
            break;
        }

    case XED_REG_DR5:
        {
            break;
        }

    case XED_REG_DR6:
        {
            break;
        }

    case XED_REG_DR7:
        {
            break;
        }

    case XED_REG_DR8:
        {
            break;
        }

    case XED_REG_DR9:
        {
            break;
        }

    case XED_REG_DR10:
        {
            break;
        }

    case XED_REG_DR11:
        {
            break;
        }

    case XED_REG_DR12:
        {
            break;
        }

    case XED_REG_DR13:
        {
            break;
        }

    case XED_REG_DR14:
        {
            break;
        }

    case XED_REG_DR15:
        {
            break;
        }

    case XED_REG_FLAGS:
        {
            break;
        }

    case XED_REG_EFLAGS:
        {
            break;
        }

    case XED_REG_RFLAGS:
        {
            break;
        }

    case XED_REG_AX:
        {
            return mk_reg("EAX", REG_16);
        }

    case XED_REG_CX:
        {
            return mk_reg("ECX", REG_16);
        }

    case XED_REG_DX:
        {
            return mk_reg("EDX", REG_16);
        }

    case XED_REG_BX:
        {
            return mk_reg("EBX", REG_16);
        }

    case XED_REG_SP:
        {
            return mk_reg("ESP", REG_16);
        }

    case XED_REG_BP:
        {
            return mk_reg("EBP", REG_16);
        }

    case XED_REG_SI:
        {
            return mk_reg("ESI", REG_16);
        }

    case XED_REG_DI:
        {
            return mk_reg("EDI", REG_16);
        }

    case XED_REG_R8W:
        {
            break;
        }

    case XED_REG_R9W:
        {
            break;
        }

    case XED_REG_R10W:
        {
            break;
        }

    case XED_REG_R11W:
        {
            break;
        }

    case XED_REG_R12W:
        {
            break;
        }

    case XED_REG_R13W:
        {
            break;
        }

    case XED_REG_R14W:
        {
            break;
        }

    case XED_REG_R15W:
        {
            break;
        }

    case XED_REG_EAX:
        {
            return mk_reg("EAX", REG_32);
        }

    case XED_REG_ECX:
        {
            return mk_reg("ECX", REG_32);
        }

    case XED_REG_EDX:
        {
            return mk_reg("EDX", REG_32);
        }

    case XED_REG_EBX:
        {
            return mk_reg("EBX", REG_32);
        }

    case XED_REG_ESP:
        {
            return mk_reg("ESP", REG_32);
        }

    case XED_REG_EBP:
        {
            return mk_reg("EBP", REG_32);
        }

    case XED_REG_ESI:
        {
            return mk_reg("ESI", REG_32);
        }

    case XED_REG_EDI:
        {
            return mk_reg("EDI", REG_32);
        }

    case XED_REG_R8D:
        {
            break;
        }

    case XED_REG_R9D:
        {
            break;
        }

    case XED_REG_R10D:
        {
            break;
        }

    case XED_REG_R11D:
        {
            break;
        }

    case XED_REG_R12D:
        {
            break;
        }

    case XED_REG_R13D:
        {
            break;
        }

    case XED_REG_R14D:
        {
            break;
        }

    case XED_REG_R15D:
        {
            break;
        }

    case XED_REG_RAX:
        {
            break;
        }

    case XED_REG_RCX:
        {
            break;
        }

    case XED_REG_RDX:
        {
            break;
        }

    case XED_REG_RBX:
        {
            break;
        }

    case XED_REG_RSP:
        {
            break;
        }

    case XED_REG_RBP:
        {
            break;
        }

    case XED_REG_RSI:
        {
            break;
        }

    case XED_REG_RDI:
        {
            break;
        }

    case XED_REG_R8:
        {
            break;
        }

    case XED_REG_R9:
        {
            break;
        }

    case XED_REG_R10:
        {
            break;
        }

    case XED_REG_R11:
        {
            break;
        }

    case XED_REG_R12:
        {
            break;
        }

    case XED_REG_R13:
        {
            break;
        }

    case XED_REG_R14:
        {
            break;
        }

    case XED_REG_R15:
        {
            break;
        }

    case XED_REG_AL:
        {
            return mk_reg("EAX", REG_8);
        }

    case XED_REG_CL:
        {
            return mk_reg("ECX", REG_8);
        }

    case XED_REG_DL:
        {
            return mk_reg("EDX", REG_8);
        }

    case XED_REG_BL:
        {
            return mk_reg("EBX", REG_8);
        }

    case XED_REG_SPL:
        {
            return mk_reg("ESP", REG_8);
        }

    case XED_REG_BPL:
        {
            return mk_reg("EBP", REG_8);
        }

    case XED_REG_SIL:
        {
            return mk_reg("ESI", REG_8);
        }

    case XED_REG_DIL:
        {
            return mk_reg("EDI", REG_8);
        }

    case XED_REG_R8B:
        {
            break;
        }

    case XED_REG_R9B:
        {
            break;
        }

    case XED_REG_R10B:
        {
            break;
        }

    case XED_REG_R11B:
        {
            break;
        }

    case XED_REG_R12B:
        {
            break;
        }

    case XED_REG_R13B:
        {
            break;
        }

    case XED_REG_R14B:
        {
            break;
        }

    case XED_REG_R15B:
        {
            break;
        }

    case XED_REG_AH:
        {
            fprintf(stderr, "i386_disasm_arg_to_temp() WARNING: using AX instead AH\n");

            return mk_reg("EAX", REG_16);
        }

    case XED_REG_CH:
        {
            fprintf(stderr, "i386_disasm_arg_to_temp() WARNING: using CX instead CH\n");

            return mk_reg("ECX", REG_16);
        }

    case XED_REG_DH:
        {
            fprintf(stderr, "i386_disasm_arg_to_temp() WARNING: using DX instead DH\n");

            return mk_reg("EDX", REG_16);
        }

    case XED_REG_BH:
        {
            fprintf(stderr, "i386_disasm_arg_to_temp() WARNING: using BX instead BH\n");

            return mk_reg("EBX", REG_16);
        }

    case XED_REG_ERROR:
        {
            break;
        }

    case XED_REG_RIP:
        {
            break;
        }

    case XED_REG_EIP:
        {
            break;
        }

    case XED_REG_IP:
        {
            break;
        }

    case XED_REG_MMX0:
        {
            break;
        }

    case XED_REG_MMX1:
        {
            break;
        }

    case XED_REG_MMX2:
        {
            break;
        }

    case XED_REG_MMX3:
        {
            break;
        }

    case XED_REG_MMX4:
        {
            break;
        }

    case XED_REG_MMX5:
        {
            break;
        }

    case XED_REG_MMX6:
        {
            break;
        }

    case XED_REG_MMX7:
        {
            break;
        }

    case XED_REG_MXCSR:
        {
            break;
        }

    case XED_REG_STACKPUSH:
        {
            break;
        }

    case XED_REG_STACKPOP:
        {
            break;
        }

    case XED_REG_GDTR:
        {
            break;
        }

    case XED_REG_LDTR:
        {
            break;
        }

    case XED_REG_IDTR:
        {
            break;
        }

    case XED_REG_TR:
        {
            break;
        }

    case XED_REG_TSC:
        {
            break;
        }

    case XED_REG_TSCAUX:
        {
            break;
        }

    case XED_REG_MSRS:
        {
            break;
        }

    case XED_REG_X87CONTROL:
        {
            break;
        }

    case XED_REG_X87STATUS:
        {
            break;
        }

    case XED_REG_X87TAG:
        {
            break;
        }

    case XED_REG_X87PUSH:
        {
            break;
        }

    case XED_REG_X87POP:
        {
            break;
        }

    case XED_REG_X87POP2:
        {
            break;
        }

    case XED_REG_X87OPCODE:
        {
            break;
        }

    case XED_REG_X87LASTCS:
        {
            break;
        }

    case XED_REG_X87LASTIP:
        {
            break;
        }

    case XED_REG_X87LASTDS:
        {
            break;
        }

    case XED_REG_X87LASTDP:
        {
            break;
        }

    case XED_REG_CS:
        {
            return mk_reg("CS", REG_16);
        }

    case XED_REG_DS:
        {
            return mk_reg("DS", REG_16);
        }

    case XED_REG_ES:
        {
            return mk_reg("ES", REG_16);
        }

    case XED_REG_SS:
        {
            return mk_reg("SS", REG_16);
        }

    case XED_REG_FS:
        {
            return mk_reg("FS", REG_16);
        }

    case XED_REG_GS:
        {
            return mk_reg("GS", REG_16);
        }

    case XED_REG_TMP0:
        {
            break;
        }

    case XED_REG_TMP1:
        {
            break;
        }

    case XED_REG_TMP2:
        {
            break;
        }

    case XED_REG_TMP3:
        {
            break;
        }

    case XED_REG_TMP4:
        {
            break;
        }

    case XED_REG_TMP5:
        {
            break;
        }

    case XED_REG_TMP6:
        {
            break;
        }

    case XED_REG_TMP7:
        {
            break;
        }

    case XED_REG_TMP8:
        {
            break;
        }

    case XED_REG_TMP9:
        {
            break;
        }

    case XED_REG_TMP10:
        {
            break;
        }

    case XED_REG_TMP11:
        {
            break;
        }

    case XED_REG_TMP12:
        {
            break;
        }

    case XED_REG_TMP13:
        {
            break;
        }

    case XED_REG_TMP14:
        {
            break;
        }

    case XED_REG_TMP15:
        {
            break;
        }

    case XED_REG_ST0:
        {
            break;
        }

    case XED_REG_ST1:
        {
            break;
        }

    case XED_REG_ST2:
        {
            break;
        }

    case XED_REG_ST3:
        {
            break;
        }

    case XED_REG_ST4:
        {
            break;
        }

    case XED_REG_ST5:
        {
            break;
        }

    case XED_REG_ST6:
        {
            break;
        }

    case XED_REG_ST7:
        {
            break;
        }

    case XED_REG_XMM0:
        {
            break;
        }

    case XED_REG_XMM1:
        {
            break;
        }

    case XED_REG_XMM2:
        {
            break;
        }

    case XED_REG_XMM3:
        {
            break;
        }

    case XED_REG_XMM4:
        {
            break;
        }

    case XED_REG_XMM5:
        {
            break;
        }

    case XED_REG_XMM6:
        {
            break;
        }

    case XED_REG_XMM7:
        {
            break;
        }

    case XED_REG_XMM8:
        {
            break;
        }

    case XED_REG_XMM9:
        {
            break;
        }

    case XED_REG_XMM10:
        {
            break;
        }

    case XED_REG_XMM11:
        {
            break;
        }

    case XED_REG_XMM12:
        {
            break;
        }

    case XED_REG_XMM13:
        {
            break;
        }

    case XED_REG_XMM14:
        {
            break;
        }

    case XED_REG_XMM15:
        {
            break;
        }

    case XED_REG_YMM0:
        {
            break;
        }

    case XED_REG_YMM1:
        {
            break;
        }

    case XED_REG_YMM2:
        {
            break;
        }

    case XED_REG_YMM3:
        {
            break;
        }

    case XED_REG_YMM4:
        {
            break;
        }

    case XED_REG_YMM5:
        {
            break;
        }

    case XED_REG_YMM6:
        {
            break;
        }

    case XED_REG_YMM7:
        {
            break;
        }

    case XED_REG_YMM8:
        {
            break;
        }

    case XED_REG_YMM9:
        {
            break;
        }

    case XED_REG_YMM10:
        {
            break;
        }

    case XED_REG_YMM11:
        {
            break;
        }

    case XED_REG_YMM12:
        {
            break;
        }

    case XED_REG_YMM13:
        {
            break;
        }

    case XED_REG_YMM14:
        {
            break;
        }

    case XED_REG_YMM15:
        {
            break;
        }
    }

    return NULL;
}

Temp *disasm_arg_to_temp(VexArch guest, xed_reg_enum_t arg)
{
    switch (guest)
    {
    case VexArchX86:

        return i386_disasm_arg_to_temp(arg);       
    
    default:
    
        throw "disasm_arg_to_temp(): unsupported arch";
    }    

    return NULL;
}

void disasm_open(VexArch guest, xed_decoded_inst_t *xed_insn)
{
    if (!xed_tables_ready)
    {
        // one-time XED tables initialization
        xed_tables_init();
        xed_tables_ready = true;
    }

    xed_decoded_inst_zero(xed_insn);

    switch (guest)
    {
    case VexArchX86:

        xed_decoded_inst_set_mode(xed_insn, XED_MACHINE_MODE_LEGACY_32, XED_ADDRESS_WIDTH_32b);
        break;
    
    default:
    
        throw "disasm_open(): unsupported arch";
    }
}

int disasm_insn(VexArch guest, uint8_t *data, string &op_str)
{
    int ret = -1;

    xed_decoded_inst_t xed_insn;
    disasm_open(guest, &xed_insn);

    // decode instruction
    xed_error_enum_t xed_error = xed_decode(
        &xed_insn, 
        XED_STATIC_CAST(const xed_uint8_t *, data),
        DISASM_MAX_INST_LEN
    );
    if (xed_error == XED_ERROR_NONE)
    {
        // get text representation of mnemonic code
        const char *mnemonic = xed_iclass_enum_t2str(xed_decoded_inst_get_iclass(&xed_insn));
        if (mnemonic)
        {
            op_str = string(mnemonic);
        }
        else
        {
            op_str = string("UNKNOWN");
        }

        ret = (int)xed_decoded_inst_get_length(&xed_insn);
    }
    else
    {
        fprintf(stderr, "ERROR: Failed to disassemble\n");
    }

    return ret;
}

int disasm_arg(VexArch guest, uint8_t *data, vector<Temp *> &args, dsiasm_arg_t type)
{
    int ret = -1;
    
    xed_decoded_inst_t xed_insn;
    disasm_open(guest, &xed_insn);

    // decode instruction
    xed_error_enum_t xed_error = xed_decode(
        &xed_insn, 
        XED_STATIC_CAST(const xed_uint8_t *, data),
        DISASM_MAX_INST_LEN
    );
    if (xed_error == XED_ERROR_NONE)
    {        
        const xed_inst_t* xed_insn_d = xed_decoded_inst_inst(&xed_insn);
        unsigned int xed_ops_num = xed_inst_noperands(xed_insn_d);

        // get instruction operands
        for (unsigned int i = 0; i < xed_ops_num; i++) 
        {
            const xed_operand_t *xed_op = xed_inst_operand(xed_insn_d, i);
            xed_operand_enum_t xed_op_name = xed_operand_name(xed_op);

            // check for the register operand
            if (xed_operand_is_register(xed_op_name))
            {
                if ((type == disasm_arg_t_src && xed_operand_read(xed_op)) ||
                    (type == disasm_arg_t_dst && xed_operand_written(xed_op)))
                {
                    // convert XED operand to libasmir object
                    Temp *temp = disasm_arg_to_temp(
                        guest, 
                        xed_decoded_inst_get_reg(&xed_insn, xed_op_name)
                    );
                    if (temp)
                    {
                        args.push_back(temp);
                    }
                }
            }            
        }
    }
    else
    {
        fprintf(stderr, "ERROR: Failed to disassemble\n");
    }
    
    return ret;
}

int disasm_arg_src(VexArch guest, uint8_t *data, vector<Temp *> &args)
{
    return disasm_arg(guest, data, args, disasm_arg_t_src);
}

int disasm_arg_dst(VexArch guest, uint8_t *data, vector<Temp *> &args)
{
    return disasm_arg(guest, data, args, disasm_arg_t_dst);
}
