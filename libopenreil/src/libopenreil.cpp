#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <iostream>
#include <string>

using namespace std;

// libasmir includes
#include "irtoir-internal.h"
#include "disasm.h"

// OpenREIL includes
#include "libopenreil.h"
#include "reil_translator.h"

#define STR_ARG_EMPTY " "
#define STR_VAR(_name_, _t_) "(" + (_name_) + ", " + to_string_size((_t_)) + ")"
#define STR_CONST(_val_, _t_) "(" + to_string_constant((_val_), (_t_)) + ", " + to_string_size((_t_)) + ")"
#define STR_LOC(_addr_, _inum_) to_string_constant((_addr_), U32) + "." + to_string_constant((_inum_), U8)

// number of zero bytes reserved for VEX at beginning of the code buffer 
#define VEX_BYTES 18

typedef struct _reil_context
{
    reil_arch_t arch;
    CReilTranslator *translator;

} reil_context;

string to_string_constant(reil_const_t val, reil_size_t size)
{
    stringstream s;
    uint8_t u8 = 0;
    uint16_t u16 = 0;
    uint32_t u32 = 0;
    uint64_t u64 = 0;

    switch (size)
    {
    case U1: if (val == 0) s << "0"; else s << "1"; break;
    case U8: u8 = (uint8_t)val; s << dec << (int)u8; break;
    case U16: u16 = (uint16_t)val; s << u16; break;
    case U32: u32 = (uint32_t)val; s << u32; break;
    case U64: u64 = (uint64_t)val; s << u64; break;
    default: assert(0);
    }
    
    return s.str();
}

string to_string_size(reil_size_t size)
{
    switch (size)
    {
    case U1: return string("1");
    case U8: return string("8");
    case U16: return string("16");
    case U32: return string("32");
    case U64: return string("64");
    }

    assert(0);
}

string to_string_operand(reil_arg_t *a)
{
    switch (a->type)
    {
    case A_NONE: return string(STR_ARG_EMPTY);
    case A_REG: return STR_VAR(string(a->name), a->size);
    case A_TEMP: return STR_VAR(string(a->name), a->size);
    case A_CONST: return STR_CONST(a->val, a->size);
    case A_LOC: return STR_LOC(a->val, a->inum);
    }    

    assert(0);
}

// defined in reil_translator.cpp
extern const char *reil_inst_name[];

string to_string_inst_code(reil_op_t inst_code)
{
    return string(reil_inst_name[inst_code]);
}

extern "C" void reil_inst_print(reil_inst_t *inst)
{
    printf("%.8llx.%.2x ", inst->raw_info.addr, inst->inum);  
    printf("%7s ", to_string_inst_code(inst->op).c_str());
    printf("%16s, ", to_string_operand(&inst->a).c_str());
    printf("%16s, ", to_string_operand(&inst->b).c_str());
    printf("%16s  ", to_string_operand(&inst->c).c_str());
    printf("\n");
}

extern "C" reil_t reil_init(reil_arch_t arch, reil_inst_handler_t handler, void *context)
{
    VexArch guest;

    switch (arch)
    {
    case ARCH_X86: 

        guest = VexArchX86; 
        break;

#ifdef TESTING

    case ARCH_ARM: 

        guest = VexArchARM; 
        break;

#endif // TESTING

    default: 

        log_write(LOG_ERR, "Unknown architecture");
        return NULL;
    }

    // allocate translator context
    reil_context *c = (reil_context *)malloc(sizeof(reil_context));
    assert(c);

    c->arch = arch;    

    // create new translator instance
    c->translator = new CReilTranslator(guest, handler, context);
    assert(c->translator);

    return c;
}

extern "C" void reil_close(reil_t reil)
{
    reil_context *c = (reil_context *)reil;
    assert(c);

    assert(c->translator);
    delete c->translator;

    free(c);
}

int reil_log_init(uint32_t mask, const char *path)
{
    // setup stderr logging options
    log_stderr(mask);

    if (path)
    {
        // open log file
        return log_init(mask, path);
    }   

    return 0; 
}

void reil_log_close(void)
{
    log_close();
}

int reil_translate_report_error(reil_addr_t addr, const char *reason)
{
    log_write(LOG_ERR, "Error while processing instruction at address 0x%x", addr);

    if (reason)
    {
        log_write(LOG_ERR, "Exception occurs: %s", reason);
    }    
    
    return REIL_ERROR;
}

extern "C" int reil_translate_insn(reil_t reil, reil_addr_t addr, unsigned char *buff, int len)
{
    int inst_len = 0;
    reil_context *c = (reil_context *)reil;
    assert(c);

    try
    {
        uint8_t inst_buff[VEX_BYTES + MAX_INST_LEN];
        unsigned char *inst_ptr = inst_buff + VEX_BYTES;

        /* 
            VEX thumb ITstate analysis needs to examine the 18 bytes
            preceding the first instruction. So let's leave the first 18
            zeroed out. 
        */        
        memset(inst_buff, 0, sizeof(inst_buff));
        memcpy(inst_buff + VEX_BYTES, buff, len);                

        inst_len = c->translator->process_inst(addr, inst_ptr, len);    
        assert(inst_len != 0 && inst_len != -1);
    }
    catch (BapException e)
    {        
        // libasmir exception
        return reil_translate_report_error(addr, e.reason.c_str());
    }
    catch (CReilTranslatorException e)
    {
        // libopenreil exception
        return reil_translate_report_error(addr, e.reason.c_str());
    }
    catch (...)
    {
        return reil_translate_report_error(addr, NULL);
    }

    return inst_len;
}

extern "C" int reil_translate(reil_t reil, reil_addr_t addr, unsigned char *buff, int len)
{
    int p = 0, translated = 0;    

    while (p < len)
    {
        uint8_t inst_buff[MAX_INST_LEN];
        int copy_len = min(MAX_INST_LEN, len - p), inst_len = 0;

        // copy one instruction into the buffer
        memset(inst_buff, 0, sizeof(inst_buff));
        memcpy(inst_buff, buff + p, copy_len);

        inst_len = reil_translate_insn(reil, addr + p, inst_buff, MAX_INST_LEN);
        if (inst_len == REIL_ERROR) return REIL_ERROR;

        p += inst_len;
        translated += 1;
    }    

    return translated;
}
