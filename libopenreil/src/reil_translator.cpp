#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <iostream>
#include <string>

extern "C" 
{ 
#include "libvex.h" 
}

// libasmir includes
#include "asm_program.h"
#include "irtoir.h"

// OpenREIL includes
#include "libopenreil.h"
#include "reil_translator.h"

using namespace std;

const char *reil_inst_name[] = 
{
    "NONE", "JCC", 
    "STR", "STM", "LDM", 
    "ADD", "SUB", "NEG", "MUL", "DIV", "MOD", "SMUL", "SDIV", "SMOD", 
    "SHL", "SHR", "ROL", "ROR", "AND", "OR", "XOR", "NOT",
    "EQ", "NEQ", "L", "LE", "SL", "SLE", 
    "CAST_L", "CAST_H", "CAST_U", "CAST_S"
};

reil_op_t reil_inst_map_binop[] = 
{
    /* PLUS     */ I_ADD, 
    /* MINUS    */ I_SUB,   
    /* TIMES    */ I_MUL,  
    /* DIVIDE   */ I_DIV,
    /* MOD      */ I_MOD,      
    /* LSHIFT   */ I_SHL,   
    /* RSHIFT   */ I_SHR,  
    /* ARSHIFT  */ I_NONE,
    /* LROTATE  */ I_ROL,  
    /* RROTATE  */ I_ROR,  
    /* LOGICAND */ I_AND, 
    /* LOGICOR  */ I_OR,
    /* BITAND   */ I_AND,  
    /* BITOR    */ I_OR,       
    /* XOR      */ I_XOR,      
    /* EQ       */ I_EQ,
    /* NEQ      */ I_NEQ,  
    /* GT       */ I_NONE,       
    /* LT       */ I_L,       
    /* GE       */ I_NONE,
    /* LE       */ I_LE, 
    /* SDIVIDE  */ I_SDIV, 
    /* SMOD     */ I_SMOD   
};

reil_op_t reil_inst_map_unop[] = 
{
    /* NEG      */ I_NEG,
    /* NOT      */ I_NOT 
};

template<class T>
string _to_string(T i)
{
    stringstream s;
    s << i;
    return s.str();
}

void reil_assert(bool condition, string reason)
{
    if (!condition) throw CReilTranslatorException(reason);
}

#define RELATIVE ((exp_type_t)((uint32_t)EXTENSION + 1))

class Relative : public Exp 
{
public:
    Relative(reg_t t, const_val_t val); 
    Relative(const Relative& other);
    virtual Relative *clone() const;
    virtual ~Relative() {}
    static void destroy(Constant *expr);

    virtual string tostring() const;
    virtual void accept(IRVisitor *v) { }
    reg_t typ;
    const_val_t val;  
};

Relative::Relative(reg_t t, const_val_t v)
  : Exp(RELATIVE), typ(t), val(v) { }

Relative::Relative(const Relative& other)
  : Exp(RELATIVE), typ(other.typ), val(other.val) { }

Relative *Relative::clone() const
{
    return new Relative(*this);
}

string Relative::tostring() const
{
    return string("$+") + _to_string(val);
}

void Relative::destroy(Constant *expr)
{
    assert(expr);
    delete expr;
}

CReilFromBilTranslator::CReilFromBilTranslator(reil_inst_handler_t handler, void *context)
{
    inst_handler = handler;
    inst_handler_context = context;
    reset_state();
}

CReilFromBilTranslator::~CReilFromBilTranslator()
{
    
}

void CReilFromBilTranslator::reset_state()
{
    tempreg_bap.clear();
    tempreg_count = inst_count = 0;
}

string CReilFromBilTranslator::tempreg_get_name(int32_t tempreg_num)
{
    char number[15];
    sprintf(number, "%.2d", tempreg_num);

    string tempreg_name = string("V_");
    tempreg_name += number;

    return tempreg_name;
}

int32_t CReilFromBilTranslator::tempreg_bap_find(string name)
{
    vector<TEMPREG_BAP>::iterator it;

    // find temporary registry number by BAP temporary registry name
    for (it = tempreg_bap.begin(); it != tempreg_bap.end(); ++it)
    {
        if (it->second == name)
        {
            return it->first;
        }
    }

    return -1;
}

int32_t CReilFromBilTranslator::tempreg_alloc(void)
{
    while (true)
    {
        vector<TEMPREG_BAP>::iterator it;
        bool found = false;
        int32_t ret = tempreg_count;

        // check if temporary registry number was reserved for BAP registers
        for (it = tempreg_bap.begin(); it != tempreg_bap.end(); ++it)
        {
            if (it->first == tempreg_count)
            {
                found = true;
                break;
            }
        }   

        tempreg_count += 1;     
        if (!found) return ret;
    }

    reil_assert(0, "error while allocating temp registry");
    return -1;
}

uint64_t CReilFromBilTranslator::convert_special(Special *special)
{
    if (special->special == "call")
    {
        return IOPT_CALL;
    }
    else if (special->special == "ret")
    {
        return IOPT_RET;
    }

    return 0;
}

reil_size_t CReilFromBilTranslator::convert_operand_size(reg_t typ)
{
    switch (typ)
    {
    case REG_1: return U1;
    case REG_8: return U8;
    case REG_16: return U16;
    case REG_32: return U32;
    case REG_64: return U64;
    default: reil_assert(0, "invalid operand size");
    }    
}

void CReilFromBilTranslator::convert_operand(Exp *exp, reil_arg_t *reil_arg)
{
    if (exp == NULL) return;

    reil_assert(
        exp->exp_type == TEMP || exp->exp_type == CONSTANT || exp->exp_type == RELATIVE,
        "invalid expression type");

    if (exp->exp_type == CONSTANT)
    {
        // special handling for canstants
        Constant *constant = (Constant *)exp;
        reil_arg->type = A_CONST;
        reil_arg->size = convert_operand_size(constant->typ);
        reil_arg->val = constant->val;
        return;
    }

    Temp *temp = (Temp *)exp;    
    string ret = temp->name;

    const char *c_name = ret.c_str();
    if (strncmp(c_name, "R_", 2) && strncmp(c_name, "V_", 2) && strncmp(c_name, "pc_0x", 5))
    {
        // this is a BAP temporary registry
        int32_t tempreg_num = tempreg_bap_find(temp->name);
        if (tempreg_num == -1)
        {
            // there is no alias for this registry, create it
            tempreg_num = tempreg_alloc();
            tempreg_bap.push_back(make_pair(tempreg_num, temp->name));

#ifdef DBG_TEMPREG

            printf("Temp reg %d reserved for %s\n", tempreg_num, name.c_str());
#endif
        }
        else
        {

#ifdef DBG_TEMPREG

            printf("Temp reg %d found for %s\n", tempreg_num, name.c_str());   
#endif
        }

        ret = tempreg_get_name(tempreg_num);
    }

    if (!strncmp(c_name, "R_", 2))
    {
        // architecture register
        reil_arg->type = A_REG;
        reil_arg->size = convert_operand_size(temp->typ);
        strncpy(reil_arg->name, ret.c_str(), REIL_MAX_NAME_LEN - 1);
    }
    else if (!strncmp(c_name, "pc_0x", 5))
    {
        // code pointer
        reil_arg->type = A_CONST;
        reil_arg->size = convert_operand_size(temp->typ);
        reil_arg->val = strtoll(c_name + 5, NULL, 16);
        reil_assert(errno != EINVAL, "invalid pc value");
    }
    else
    {
        // temporary register
        reil_arg->type = A_TEMP;
        reil_arg->size = convert_operand_size(temp->typ);
        strncpy(reil_arg->name, ret.c_str(), REIL_MAX_NAME_LEN - 1);
    }
}

void CReilFromBilTranslator::process_reil_inst(reil_inst_t *reil_inst)
{
    if (inst_handler)
    {
        // call user-specified REIL instruction handler
        inst_handler(reil_inst, inst_handler_context);
    }
}

void CReilFromBilTranslator::free_bil_exp(Exp *exp)
{
    if (exp) 
    {
        // free temp expression that was returned by process_bil_exp()
        Temp::destroy(reinterpret_cast<Temp *>(exp));
    }
}

Exp *CReilFromBilTranslator::process_bil_exp(Exp *exp)
{
    Exp *ret = exp;

    if (exp->exp_type != TEMP && exp->exp_type != CONSTANT)
    {
        reil_assert(
            exp->exp_type == BINOP || exp->exp_type == UNOP || exp->exp_type == CAST,
            "invaid expression type");

        // expand complex expression and store result to the new temporary value
        return process_bil_inst(I_STR, 0, NULL, exp);
    }    

    return NULL;
}

Exp *CReilFromBilTranslator::process_bil_inst(reil_op_t inst, uint64_t inst_flags, Exp *c, Exp *exp)
{
    reil_inst_t reil_inst;
    Exp *a = NULL, *b = NULL;
    Exp *a_temp = NULL, *b_temp = NULL, *exp_temp = NULL;

    reil_assert(exp, "invalid expression");
    reil_assert(inst == I_STR || inst == I_JCC, "invalid instruction");
    
    memset(&reil_inst, 0, sizeof(reil_inst));
    reil_inst.op = inst;
    reil_inst.raw_info.addr = current_raw_info->addr;
    reil_inst.raw_info.size = current_raw_info->size;
    reil_inst.flags = inst_flags;

    if (c && c->exp_type == MEM)
    {
        // check for the store to memory
        reil_assert(reil_inst.op == I_STR, "invalid instruction used with memory operand");

        Mem *mem = (Mem *)c;    
        reil_inst.op = I_STM;

        // parse address expression
        Exp *addr = process_bil_exp(mem->addr);
        if (addr)
        {
            c = addr;
        }
        else
        {
            c = mem->addr;
        }

        // parse value expression
        if (exp_temp = process_bil_exp(exp))
        {
            exp = exp_temp;
        }
    }
    else if (c && c->exp_type == NAME)
    {
        // check for the jump
        reil_assert(reil_inst.op == I_JCC, "invalid instruction used with name operand");

        Name *name = (Name *)c;
        c = new Temp(REG_32, name->name);
    }

    if (reil_inst.op == I_STR) 
    {
        reil_assert(c == NULL || c->exp_type == TEMP, 
            "invalid I_STR argument");
    }

    if (reil_inst.op == I_STM) 
    {
        reil_assert(c == NULL || (c->exp_type == TEMP || c->exp_type == CONSTANT),
            "invalid I_STM argument");
    }

    bool binary_logic = false;
    
    // get a and b operands values from expression
    if (exp->exp_type == BINOP)
    {
        reil_assert(reil_inst.op == I_STR, "invalid instruction");

        // store result of binary operation
        BinOp *binop = (BinOp *)exp;  
        reil_inst.op = reil_inst_map_binop[binop->binop_type];

        if (binop->binop_type == LOGICAND || binop->binop_type == LOGICOR)
        {
            binary_logic = true;
        }

        reil_assert(reil_inst.op != I_NONE, "invalid binop expression");

        a = binop->lhs;
        b = binop->rhs;
    }
    else if (exp->exp_type == UNOP)
    {
        reil_assert(reil_inst.op == I_STR, "invalid instruction");

        // store result of unary operation
        UnOp *unop = (UnOp *)exp;   
        reil_inst.op = reil_inst_map_unop[unop->unop_type];

        reil_assert(reil_inst.op != I_NONE, "invaid unop expression");

        a = unop->exp;
    }    
    else if (exp->exp_type == CAST)
    {
        reil_assert(reil_inst.op == I_STR, "invaid instruction");

        // store with type cast
        Cast *cast = (Cast *)exp;
        if (cast->cast_type == CAST_HIGH)
        {
            // use high half
            reil_inst.op = I_HCAST;
        }
        else if (cast->cast_type == CAST_LOW)
        {
            // use low half
            reil_inst.op = I_LCAST;
        }
        else if (cast->cast_type == CAST_UNSIGNED)
        {
            // cast to unsigned value of bigger size
            reil_inst.op = I_UCAST;
        }
        else if (cast->cast_type == CAST_SIGNED)
        {
            // cast to signed value of bigger size
            reil_inst.op = I_SCAST;
        }
        else
        {
            reil_assert(0, "invalid cast");
        }

        a = cast->exp;
    }
    else if (exp->exp_type == MEM)
    {
        reil_assert(reil_inst.op == I_STR, "invalid instruction used with memory operand");

        // read from memory and store
        Mem *mem = (Mem *)exp;
        reil_inst.op = I_LDM;

        if (a_temp = process_bil_exp(mem->addr))
        {
            a = a_temp;
        }
        else
        {
            a = mem->addr;
        }
    }     
    else if (exp->exp_type == TEMP || exp->exp_type == CONSTANT)
    {
        // store constant or register
        a = exp;
    }        
    else
    {
        reil_assert(0, "invalid expression");
    }

    // parse operand a expression
    if (a && (a_temp = process_bil_exp(a)))
    {
        a = a_temp;
    }   

    // parse operand b expression
    if (b && (b_temp = process_bil_exp(b)))
    {
        b = b_temp;
    }    

    reil_assert(a, "invalid instruction argument");
    
    reil_assert(a == NULL || a->exp_type == TEMP || a->exp_type == CONSTANT, 
        "invalid instruction argument");

    reil_assert(b == NULL || b->exp_type == TEMP || b->exp_type == CONSTANT, 
        "invalid instruction argument");    

    if (binary_logic)
    {
        // check for LOGICAND/LOGICOR size
        reil_assert(a == NULL || ((Temp *)a)->typ == REG_1, "invalid logic operand");
        reil_assert(b == NULL || ((Temp *)b)->typ == REG_1, "invalid logic operand");
    }

    if (c == NULL)
    {
        // allocate temporary value to store result
        reg_t tempreg_type;
        string tempreg_name;

        // determinate type for new value by type of result
        if (exp->exp_type == CAST)
        {
            Cast *cast = (Cast *)exp;
            tempreg_type = cast->typ;
        }
        else if (a->exp_type == TEMP)
        {
            Temp *temp = (Temp *)a;
            tempreg_type = temp->typ;
        }
        else if (a->exp_type == CONSTANT)
        {
            Constant *constant = (Constant *)a;
            tempreg_type = constant->typ;
        }
        else
        {
            reil_assert(0, "invaid expression");
        }        
        
        tempreg_name = tempreg_get_name(tempreg_alloc());
        c = new Temp(tempreg_type, tempreg_name);
    }        

    reil_inst.inum = inst_count;
    inst_count += 1;

    // make REIL operands from BIL expressions
    convert_operand(a, &reil_inst.a);
    convert_operand(b, &reil_inst.b);
    convert_operand(c, &reil_inst.c);

    // handle assembled REIL instruction
    process_reil_inst(&reil_inst);

    free_bil_exp(a_temp);
    free_bil_exp(b_temp);
    free_bil_exp(exp_temp);

    return c;
}

void CReilFromBilTranslator::process_bil(reil_raw_t *raw_info, uint64_t inst_flags, Stmt *s)
{
    current_raw_info = raw_info;

#ifdef DBG_BAP
        
    printf("%s\n", s->tostring().c_str());

#endif

    switch (s->stmt_type)
    {
    case MOVE:    
        {
            // move statement
            Move *move = (Move *)s;
            process_bil_inst(I_STR, inst_flags, move->lhs, move->rhs);
            break;    
        }       
    
    case JMP:
        {
            if (!(inst_flags & IOPT_CALL))
            {
                inst_flags |= IOPT_BB_END;
            }

            // jump statement
            Jmp *jmp = (Jmp *)s;
            Constant c(REG_1, 1);
            process_bil_inst(I_JCC, inst_flags, jmp->target, &c);
            break;
        }

    case CJMP:
        {
            // conditional jump statement
            CJmp *cjmp = (CJmp *)s;
            process_bil_inst(I_JCC, inst_flags | IOPT_BB_END, cjmp->t_target, cjmp->cond);
            break;
        }

    case CALL:
    case RETURN:
        {            
            printf("Statement %d is not implemented\n", s->stmt_type);
            reil_assert(0, "unimplemented statement");
        }

    case EXPSTMT:
    case COMMENT:
    case SPECIAL:
    case LABEL:
    case VARDECL:

        break;
    }  

#if defined(DBG_BAP) && defined(DBG_REIL)

    printf("\n");

#endif  

}

void CReilFromBilTranslator::process_bil(reil_raw_t *raw_info, bap_block_t *block)
{
    int size = block->bap_ir->size();

    reset_state();
    
    for (int i = 0; i < size; i++)
    {
        // enumerate BIL statements        
        Stmt *s = block->bap_ir->at(i);
        uint64_t inst_flags = IOPT_ASM_END;    

        for (int n = i + 1; n < size; n++)
        {
            // check for last IR instruction
            Stmt *s_next = block->bap_ir->at(n);

            if (s_next->stmt_type == MOVE || 
                s_next->stmt_type == CJMP ||
                s_next->stmt_type == JMP)
            {
                inst_flags = 0;
                break;
            }            
        }

        if (i < size - 1)
        {
            // check for the special statement that following current
            Stmt *s_next = block->bap_ir->at(i + 1);
            if (s_next->stmt_type == SPECIAL)
            {
                Special *special = (Special *)s_next;
                
                // translate special statement to the REIL instruction options
                inst_flags |= convert_special(special);
            }
        }        

        // convert statement to REIL code
        process_bil(raw_info, inst_flags, s);
    }
}

CReilTranslator::CReilTranslator(bfd_architecture arch, reil_inst_handler_t handler, void *context)
{
    // initialize libasmir
    translate_init();

    // allocate a fake bfd instance
    prog = asmir_new_asmp_for_arch(arch);
    assert(prog);

    // create code segment for instruction buffer 
    prog->segs = (section_t *)bfd_alloc(prog->abfd, sizeof(section_t));
    assert(prog->segs);
            
    prog->segs->data = inst_buffer;
    prog->segs->datasize = MAX_INST_LEN;                        
    prog->segs->section = NULL;
    prog->segs->is_code = true;
    set_inst_addr(0);

    translator = new CReilFromBilTranslator(handler, context);
    assert(translator);
}

CReilTranslator::~CReilTranslator()
{
    delete translator;
    asmir_close(prog);
}

void CReilTranslator::set_inst_addr(address_t addr)
{
    prog->segs->start_addr = addr;
    prog->segs->end_addr = addr + MAX_INST_LEN;
}

int CReilTranslator::process_inst(address_t addr, uint8_t *data, int size)
{
    int ret = 0;

    set_inst_addr(addr);
    memcpy(inst_buffer, data, min(size, MAX_INST_LEN));

    // translate to VEX
    bap_block_t *block = generate_vex_ir(prog, addr);
    assert(block);

#ifdef DBG_ASM

    string asm_code = asmir_string_of_insn(prog, addr);
    printf("# %s\n\n", asm_code.c_str());

#endif

    ret = asmir_get_instr_length(prog, addr);
    assert(ret != 0 && ret != -1);

    // tarnslate to BAP
    generate_bap_ir_block(prog, block);                

    // generate REIL
    reil_raw_t raw_info;
    raw_info.addr = addr;
    raw_info.size = ret;
    translator->process_bil(&raw_info, block);

    for (int i = 0; i < block->bap_ir->size(); i++)
    {
        // free BIL code
        Stmt *s = block->bap_ir->at(i);
        Stmt::destroy(s);
    }

    delete block->bap_ir;
    delete block;        

    // free VEX memory
    // asmir_close() is also doing that
    vx_FreeAll();

    return ret;
}
