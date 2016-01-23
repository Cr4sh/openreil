#include <string>
#include <algorithm>
#include <vector>

#include "capstone/capstone.h"

#include "libvex.h"

using namespace std;

#include "irtoir-internal.h"
#include "disasm.h"

typedef enum _dsiasm_arg_t
{
    disasm_arg_t_src,
    disasm_arg_t_dst

} dsiasm_arg_t;

Temp *i386_disasm_arg_to_temp(uint16_t arg)
{
    switch (arg)
    {
    case X86_REG_AH:

        log_write(LOG_WARN, "i386_disasm_arg_to_temp(): using AX instead AH\n");
        return mk_reg("EAX", REG_16);

    case X86_REG_AL:

        return mk_reg("EAX", REG_8);
    
    case X86_REG_AX:

        return mk_reg("EAX", REG_16);
    
    case X86_REG_BH:

        log_write(LOG_WARN, "i386_disasm_arg_to_temp(): using BX instead BH\n");
        return mk_reg("EBX", REG_16);
    
    case X86_REG_BL:

        return mk_reg("EBX", REG_8);
    
    case X86_REG_BP:

        return mk_reg("EBP", REG_16);
    
    case X86_REG_BX:

        return mk_reg("EBX", REG_16);
    
    case X86_REG_CH:

        log_write(LOG_WARN, "i386_disasm_arg_to_temp(): using CX instead CH\n");
        return mk_reg("ECX", REG_16);
    
    case X86_REG_CL:

        return mk_reg("ECX", REG_8);
    
    case X86_REG_CS:

        return mk_reg("CS", REG_16);
    
    case X86_REG_CX:

        return mk_reg("ECX", REG_16);
    
    case X86_REG_DH:

        log_write(LOG_WARN, "i386_disasm_arg_to_temp(): using DX instead DH\n");
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

Temp *arm_disasm_arg_to_temp(uint16_t arg)
{
    switch (arg)
    {        
    case ARM_REG_R0:

        return mk_reg("R0", REG_32);

    case ARM_REG_R1:

        return mk_reg("R1", REG_32);

    case ARM_REG_R2:

        return mk_reg("R2", REG_32);

    case ARM_REG_R3:

        return mk_reg("R3", REG_32);

    case ARM_REG_R4:

        return mk_reg("R4", REG_32);

    case ARM_REG_R5:

        return mk_reg("R5", REG_32);

    case ARM_REG_R6:

        return mk_reg("R6", REG_32);

    case ARM_REG_R7:

        return mk_reg("R7", REG_32);

    case ARM_REG_R8:

        return mk_reg("R8", REG_32);

    case ARM_REG_R9:

        return mk_reg("R9", REG_32);

    case ARM_REG_R10:

        return mk_reg("R10", REG_32);

    case ARM_REG_R11:

        return mk_reg("R11", REG_32);

    case ARM_REG_R12:

        return mk_reg("R12", REG_32);
    
    case ARM_REG_R13:

        return mk_reg("R13", REG_32);
        
    case ARM_REG_R14:

        return mk_reg("R14", REG_32);

    case ARM_REG_R15:

        return mk_reg("R15T", REG_32);    

    default:

        return NULL;
    }

    return NULL;
}

Temp *disasm_arg_to_temp(VexArch guest, uint16_t arg)
{
    switch (guest)
    {
    case VexArchX86:

        return i386_disasm_arg_to_temp(arg);       

    case VexArchARM:
    
        return arm_disasm_arg_to_temp(arg);       
    
    default:
    
        panic("disasm_arg_to_temp(): unsupported arch");
    }    

    return NULL;
}

void disasm_open(VexArch guest, address_t addr, csh *handle)
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
        mode = IS_ARM_THUMB(addr) ? CS_MODE_THUMB : CS_MODE_ARM;
        break;
    
    default:
    
        panic("disasm_open(): unsupported arch");
    }

    if (cs_open(arch, mode, handle) != CS_ERR_OK)
    {
        panic("cs_open() fails");
    }    
}

int disasm_insn(VexArch guest, uint8_t *data, address_t addr, string &mnemonic, string &op)
{
    int ret = -1;
    
    csh handle;
    cs_insn *insn;

    disasm_open(guest, addr, &handle);

    size_t count = cs_disasm(handle, data, DISASM_MAX_INST_LEN, 0, 1, &insn);    
    if (count > 0) 
    {   
        // get instruction length     
        ret = (int)insn[0].size;

        // get assembly code
        mnemonic = string(insn[0].mnemonic);
        op = string(insn[0].op_str);        

        cs_free(insn, count);
    } 
    else
    {
        log_write(LOG_ERR, "disasm_insn(): failed to disassemble\n");
    }

    cs_close(&handle);
    return ret;
}

#define I386_MODRM_RM(_modrm_) ((_modrm_) & 7)

const char *i386_reg_name(int val)
{
    switch (val)
    {
    case 0: return "EAX";
    case 1: return "ECX";
    case 2: return "EDX";
    case 3: return "EBX";
    case 4: return "ESP";
    case 5: return "EBP";
    case 6: return "ESI";
    case 7: return "EDI";
    }

    return NULL;
}

int i386_disasm_arg_special(cs_insn *insn, vector<Temp *> &args, dsiasm_arg_t type)
{
    bool src = (type == disasm_arg_t_src);
    bool dst = (type == disasm_arg_t_dst);
    const char *reg = NULL;

    switch (insn->id)
    {
    case X86_INS_SIDT:        

        if ((reg = i386_reg_name(I386_MODRM_RM(insn->detail->x86.modrm))) != NULL)
        {
            if (src)
            {
                args.push_back(mk_reg(reg, REG_32));
                return 1;
            }
            else
            {
                args.push_back(mk_reg("IDT", REG_32));
                return 1;
            }
        }

        break;

    case X86_INS_SGDT:

        if ((reg = i386_reg_name(I386_MODRM_RM(insn->detail->x86.modrm))) != NULL)
        {
            if (src)
            {
                args.push_back(mk_reg(reg, REG_32));
                return 1;
            }
            else
            {
                args.push_back(mk_reg("GDT", REG_32));
                return 1;
            }
        }

        break;

    case X86_INS_SLDT:

        if ((reg = i386_reg_name(I386_MODRM_RM(insn->detail->x86.modrm))) != NULL)
        {
            if (src)
            {
                args.push_back(mk_reg(reg, REG_32));
                return 1;
            }
            else
            {
                args.push_back(mk_reg("LDT", REG_32));
                return 1;
            }
        }

        break;

    case X86_INS_LIDT:

        if ((reg = i386_reg_name(I386_MODRM_RM(insn->detail->x86.modrm))) != NULL)
        {
            if (src)
            {
                args.push_back(mk_reg("IDT", REG_32));
                args.push_back(mk_reg(reg, REG_32));
                return 2;
            }
            else
            {
                return 0;
            }
        }

        break;

    case X86_INS_LGDT:

        if ((reg = i386_reg_name(I386_MODRM_RM(insn->detail->x86.modrm))) != NULL)
        {
            if (src)
            {
                args.push_back(mk_reg("GDT", REG_32));
                args.push_back(mk_reg(reg, REG_32));
                return 2;
            }
            else
            {
                return 0;
            }
        }

        break;

    case X86_INS_LLDT:

        if ((reg = i386_reg_name(I386_MODRM_RM(insn->detail->x86.modrm))) != NULL)
        {
            if (src)
            {
                args.push_back(mk_reg("LDT", REG_32));
                args.push_back(mk_reg(reg, REG_32));
                return 2;
            }
            else
            {
                return 0;
            }
        }

        break;
    }

    return -1;
}

int disasm_arg_special(VexArch guest, cs_insn *insn, vector<Temp *> &args, dsiasm_arg_t type)
{
    switch (guest)
    {
    case VexArchX86:

        return i386_disasm_arg_special(insn, args, type);       

    case VexArchARM:
    
        break;
    
    default:
    
        panic("disasm_arg_special(): unsupported arch");
    }    

    return -1;
}

int disasm_arg(VexArch guest, uint8_t *data, address_t addr, vector<Temp *> &args, dsiasm_arg_t type)
{
    int ret = -1;
    
    csh handle;
    cs_insn *insn;

    disasm_open(guest, addr, &handle);
    cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);

    size_t count = cs_disasm(handle, data, DISASM_MAX_INST_LEN, 0, 1, &insn);    
    if (count > 0) 
    {        
        cs_regs regs_read, regs_write;
        uint8_t read_count, write_count;

        // get arguments for instructions that capstone fails to recognise properly
        ret = disasm_arg_special(guest, &insn[0], args, type);
        if (ret >= 0)
        {
            return ret;
        }

        // get all registers accessed by this instruction
        if (cs_regs_access(handle, insn, regs_read, &read_count, regs_write, &write_count) == 0) 
        {
            uint16_t *regs = NULL;
            uint8_t count = 0;

            if (read_count > 0 && type == disasm_arg_t_src) 
            {                
                regs = regs_read;
                count = read_count;                
            }
            else if (write_count > 0 && type == disasm_arg_t_dst) 
            {                
                regs = regs_write;
                count = write_count;                
            }

            for (int i = 0; i < count; i++) 
            {
                Temp *temp = disasm_arg_to_temp(guest, regs[i]);
                if (temp)
                {
                    bool exists = false;
                    vector<Temp *>::iterator it;

                    for (it = args.begin(); it != args.end(); ++it)
                    {
                        if ((*it)->name == temp->name)
                        {
                            exists = true;
                            break;
                        }
                    }

                    if (!exists)
                    {
                        args.push_back(temp);
                        ret += 1;
                    }                    
                }
            }
        }

        cs_free(insn, count);
    } 
    else
    {
        log_write(LOG_ERR, "disasm_arg(): failed to disassemble\n");
    }

    cs_close(&handle);
    return ret;
}

int disasm_arg_src(VexArch guest, uint8_t *data, address_t addr, vector<Temp *> &args)
{
    return disasm_arg(guest, data, addr, args, disasm_arg_t_src);
}

int disasm_arg_dst(VexArch guest, uint8_t *data, address_t addr, vector<Temp *> &args)
{
    return disasm_arg(guest, data, addr, args, disasm_arg_t_dst);
}
