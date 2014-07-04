#include <string>
#include <vector>

#include "capstone.h"

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

Temp *i386_disasm_arg_to_temp(uint8_t arg)
{
    switch (arg)
    {
    case X86_REG_AH:

        fprintf(stderr, "i386_disasm_arg_to_temp() WARNING: using AX instead AH\n");
        return mk_reg("EAX", REG_16);

    case X86_REG_AL:

        return mk_reg("EAX", REG_8);
    
    case X86_REG_AX:

        return mk_reg("EAX", REG_16);
    
    case X86_REG_BH:

        fprintf(stderr, "i386_disasm_arg_to_temp() WARNING: using BX instead BH\n");
        return mk_reg("EBX", REG_16);
    
    case X86_REG_BL:

        return mk_reg("EBX", REG_8);
    
    case X86_REG_BP:

        return mk_reg("EBP", REG_16);
    
    case X86_REG_BX:

        return mk_reg("EBX", REG_16);
    
    case X86_REG_CH:

        fprintf(stderr, "i386_disasm_arg_to_temp() WARNING: using CX instead CH\n");
        return mk_reg("ECX", REG_16);
    
    case X86_REG_CL:

        return mk_reg("ECX", REG_8);
    
    case X86_REG_CS:

        return mk_reg("CS", REG_16);
    
    case X86_REG_CX:

        return mk_reg("ECX", REG_16);
    
    case X86_REG_DH:

        fprintf(stderr, "i386_disasm_arg_to_temp() WARNING: using DX instead DH\n");
        return mk_reg("EDX", REG_16);
    
    case X86_REG_DI:

        return mk_reg("EDI", REG_16);
    
    case X86_REG_DL:

        return mk_reg("EDX", REG_8);
    
    case X86_REG_DS:

        return mk_reg("DS", REG_16);
    
    case X86_REG_DX:

        return mk_reg("EDX", REG_16);
    
    case X86_REG_EAX:

        return mk_reg("EAX", REG_32);
    
    case X86_REG_EBP:

        return mk_reg("EBP", REG_32);
    
    case X86_REG_EBX:

        return mk_reg("EBX", REG_32);
    
    case X86_REG_ECX:

        return mk_reg("ECX", REG_32);
    
    case X86_REG_EDI:

        return mk_reg("EDI", REG_32);
    
    case X86_REG_EDX:

        return mk_reg("EDX", REG_32);
    
    case X86_REG_EFLAGS:

        return mk_reg("EFLAGS", REG_16);
    
    case X86_REG_EIP:

        return mk_reg("EIP", REG_32);
    
    case X86_REG_ES:

        return mk_reg("ES", REG_16);
    
    case X86_REG_ESI:

        return mk_reg("ESI", REG_32);
    
    case X86_REG_ESP:

        return mk_reg("ESP", REG_32);
    
    case X86_REG_FS:

        return mk_reg("FS", REG_16);
    
    case X86_REG_GS:

        return mk_reg("GS", REG_16);
    
    case X86_REG_IP:

        return mk_reg("EIP", REG_16);
    
    case X86_REG_SI:

        return mk_reg("ESI", REG_16);
    
    case X86_REG_SP:

        return mk_reg("ESP", REG_16);
    
    case X86_REG_SS:

        return mk_reg("SS", REG_16);
    
    case X86_REG_CR0:

        return mk_reg("CR0", REG_32);
    
    case X86_REG_CR1:

        return mk_reg("CR1", REG_32);
    
    case X86_REG_CR2:

        return mk_reg("CR2", REG_32);
    
    case X86_REG_CR3:

        return mk_reg("CR3", REG_32);
    
    case X86_REG_CR4:

        return mk_reg("CR4", REG_32);
    
    case X86_REG_CR5:

        return mk_reg("CR5", REG_32);
    
    case X86_REG_CR6:

        return mk_reg("CR6", REG_32);
    
    case X86_REG_CR7:

        return mk_reg("CR7", REG_32);
    
    case X86_REG_CR8:

        return mk_reg("CR8", REG_32);
    
    case X86_REG_CR9:

        return mk_reg("CR9", REG_32);
    
    case X86_REG_CR10:

        return mk_reg("CR10", REG_32);
    
    case X86_REG_CR11:

        return mk_reg("CR11", REG_32);
    
    case X86_REG_CR12:

        return mk_reg("CR12", REG_32);
    
    case X86_REG_CR13:

        return mk_reg("CR13", REG_32);
    
    case X86_REG_CR14:

        return mk_reg("CR14", REG_32);
    
    case X86_REG_CR15:

        return mk_reg("CR15", REG_32);
    
    case X86_REG_DR0:

        return mk_reg("DR0", REG_32);
    
    case X86_REG_DR1:

        return mk_reg("DR1", REG_32);
    
    case X86_REG_DR2:

        return mk_reg("DR2", REG_32);
    
    case X86_REG_DR3:

        return mk_reg("DR3", REG_32);
    
    case X86_REG_DR4:

        return mk_reg("DR4", REG_32);
    
    case X86_REG_DR5:

        return mk_reg("DR5", REG_32);
    
    case X86_REG_DR6:

        return mk_reg("DR6", REG_32);
    
    case X86_REG_DR7:

        return mk_reg("DR7", REG_32);

    default:

        return NULL;
    }

    return NULL;
}

Temp *disasm_arg_to_temp(VexArch guest, uint8_t arg)
{
    switch (guest)
    {
    case VexArchX86:

        return i386_disasm_arg_to_temp(arg);       

    case VexArchARM:
    
        return NULL;
    
    default:
    
        throw "disasm_arg_to_temp(): unsupported arch";
    }    

    return NULL;
}

void disasm_open(VexArch guest, csh *handle)
{
    cs_arch arch;
    cs_mode mode;

    switch (guest)
    {
    case VexArchX86:

        arch = CS_ARCH_X86;
        mode = CS_MODE_32;
        break;

    case VexArchARM:
    
        arch = CS_ARCH_ARM;
        mode = CS_MODE_ARM;
        break;
    
    default:
    
        throw "disasm_open(): unsupported arch";
    }

    if (cs_open(arch, mode, handle) != CS_ERR_OK)
    {
        throw "cs_open() fails";
    }    
}

int disasm_insn(VexArch guest, uint8_t *data, string &op_str)
{
    int ret = -1;
    
    csh handle;
    cs_insn *insn;

    disasm_open(guest, &handle);

    size_t count = cs_disasm_ex(handle, data, DISASM_MAX_INST_LEN, 0, 1, &insn);    
    if (count > 0) 
    {
        ret = (int)insn[0].size;
        op_str = string(cs_insn_name(handle, insn[0].id));
        cs_free(insn, count);
    } 
    else
    {
        fprintf(stderr, "ERROR: Failed to disassemble\n");
    }

    cs_close(&handle);
    return ret;
}

int i386_disasm_arg_special(unsigned int id, vector<Temp *> &args, dsiasm_arg_t type)
{
    bool src = (type == disasm_arg_t_src);
    bool dst = (type == disasm_arg_t_dst);

    switch (id)
    {
    case X86_INS_RDTSC:

        if (dst)
        {            
            args.push_back(mk_reg("EAX", REG_32));
            args.push_back(mk_reg("EDX", REG_32));
            return 2;   
        }
        else
        {
            return 0;
        }

    case X86_INS_RDTSCP:

        if (dst)
        {
            args.push_back(mk_reg("EAX", REG_32));
            args.push_back(mk_reg("EDX", REG_32));
            args.push_back(mk_reg("ECX", REG_32));
            return 3;   
        }
        else
        {
            return 0;
        }

    case X86_INS_RDMSR:

        if (src)
        {            
            args.push_back(mk_reg("ECX", REG_32));
            return 1;   
        }
        else
        {
            args.push_back(mk_reg("EAX", REG_32));
            args.push_back(mk_reg("EDX", REG_32));
            return 2;
        }

    case X86_INS_WRMSR:

        if (src)
        {            
            args.push_back(mk_reg("EAX", REG_32));
            args.push_back(mk_reg("EDX", REG_32));
            args.push_back(mk_reg("ECX", REG_32));
            return 3;   
        }
        else
        {
            return 0;
        }

        break;
    }

    return -1;
}

int disasm_arg_special(VexArch guest, unsigned int id, vector<Temp *> &args, dsiasm_arg_t type)
{
    switch (guest)
    {
    case VexArchX86:

        return i386_disasm_arg_special(id, args, type);       

    case VexArchARM:
    
        break;
    
    default:
    
        throw "disasm_arg_special(): unsupported arch";
    }    

    return -1;
}

int disasm_arg(VexArch guest, uint8_t *data, vector<Temp *> &args, dsiasm_arg_t type)
{
    int ret = -1;
    
    csh handle;
    cs_insn *insn;

    disasm_open(guest, &handle);
    cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);

    size_t count = cs_disasm_ex(handle, data, DISASM_MAX_INST_LEN, 0, 1, &insn);    
    if (count > 0) 
    {
        cs_detail *detail = insn[0].detail;
        uint8_t *data = NULL;

        // get arguments that capstone fails to recognise properly
        ret = disasm_arg_special(guest, insn[0].id, args, type);
        if (ret >= 0)
        {
            return ret;
        }

        if (detail && type == disasm_arg_t_src)
        {
            ret = detail->regs_read_count;
            data = detail->regs_read;
        }
        else if (detail && type == disasm_arg_t_dst)
        {
            ret = detail->regs_write_count;
            data = detail->regs_write;
        }

        if (ret > 0) 
        {
            for (int i = 0; i < ret; i++) 
            {
                Temp *temp = disasm_arg_to_temp(guest, data[i]);
                if (temp)
                {
                    args.push_back(temp);
                }                
            }
        }    

        cs_free(insn, count);
    } 
    else
    {
        fprintf(stderr, "ERROR: Failed to disassemble\n");
    }

    cs_close(&handle);
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
