#include <stdio.h>
#include <string>
#include <iostream>
#include <assert.h>
#include <stddef.h>

using namespace std;

#include "irtoir-internal.h"
#include "libvex_guest_arm.h"

//
// Register offsets, copied from VEX/priv/guest_arm/toIR.c
//

/*------------------------------------------------------------*/
/*--- Offsets of various parts of the arm guest state.     ---*/
/*------------------------------------------------------------*/
#define OFFB_R0       offsetof(VexGuestARMState, guest_R0)
#define OFFB_R1       offsetof(VexGuestARMState, guest_R1)
#define OFFB_R2       offsetof(VexGuestARMState, guest_R2)
#define OFFB_R3       offsetof(VexGuestARMState, guest_R3)
#define OFFB_R4       offsetof(VexGuestARMState, guest_R4)
#define OFFB_R5       offsetof(VexGuestARMState, guest_R5)
#define OFFB_R6       offsetof(VexGuestARMState, guest_R6)
#define OFFB_R7       offsetof(VexGuestARMState, guest_R7)
#define OFFB_R8       offsetof(VexGuestARMState, guest_R8)
#define OFFB_R9       offsetof(VexGuestARMState, guest_R9)
#define OFFB_R10      offsetof(VexGuestARMState, guest_R10)
#define OFFB_R11      offsetof(VexGuestARMState, guest_R11)
#define OFFB_R12      offsetof(VexGuestARMState, guest_R12)
#define OFFB_R13      offsetof(VexGuestARMState, guest_R13)
#define OFFB_R14      offsetof(VexGuestARMState, guest_R14)
#define OFFB_R15T     offsetof(VexGuestARMState, guest_R15T)

// We assume you are compiling with the included patched version of
// VEX. If not, you may need to define ARM_THUNKS to revert to
// (at least at the time of this writing) default VEX ARM behavior.
#define OFFB_CC_OP    offsetof(VexGuestARMState, guest_CC_OP)
#define OFFB_CC_DEP1  offsetof(VexGuestARMState, guest_CC_DEP1)
#define OFFB_CC_DEP2  offsetof(VexGuestARMState, guest_CC_DEP2)
#define OFFB_CC_NDEP  offsetof(VexGuestARMState, guest_CC_NDEP)

#define OFFB_ITSTATE  offsetof(VexGuestARMState, guest_ITSTATE)

//
// Condition code enum copied from VEX/priv/guest_arm_defs.h
// Note: If these constants are ever changed, then they would
//       need to be re-copied from the newer version of VEX.
//
typedef enum
{
    ARMCondEQ     = 0,      /* equal                    */
    ARMCondNE     = 1,      /* not equal                */

    ARMCondHS     = 2,      /* higher or same           */
    ARMCondLO     = 3,      /* lower                    */

    ARMCondMI     = 4,      /* minus (negative)         */
    ARMCondPL     = 5,      /* plus (zero or +ve)       */

    ARMCondVS     = 6,      /* overflow                 */
    ARMCondVC     = 7,      /* no overflow              */

    ARMCondHI     = 8,      /* higher                   */
    ARMCondLS     = 9,      /* lower or same            */

    ARMCondGE     = 10,     /* signed greater or equal  */
    ARMCondLT     = 11,     /* signed less than         */

    ARMCondGT     = 12,     /* signed greater           */
    ARMCondLE     = 13,     /* signed less or equal     */

    ARMCondAL     = 14,     /* always (unconditional)   */
    ARMCondNV     = 15      /* never (unconditional)    */

} ARMCondcode;

//
// Copied from VEX/priv/guest_arm_defs.h
//
enum 
{
    ARMG_CC_OP_COPY = 0,    /* DEP1 = NZCV in 31:28, DEP2 = 0, DEP3 = 0 */
    ARMG_CC_OP_ADD,         /* DEP1 = argL (Rn), DEP2 = argR (shifter_op), DEP3 = 0 */
    ARMG_CC_OP_SUB,         /* DEP1 = argL (Rn), DEP2 = argR (shifter_op), DEP3 = 0 */
    ARMG_CC_OP_ADC,         /* DEP1 = argL (Rn), DEP2 = arg2 (shifter_op), DEP3 = oldC */
    ARMG_CC_OP_SBB,         /* DEP1 = argL (Rn), DEP2 = arg2 (shifter_op), DEP3 = oldC */
    ARMG_CC_OP_LOGIC,       /* DEP1 = result, DEP2 = shifter_carry_out (in LSB), DEP3 = old V flag */
    ARMG_CC_OP_MUL,         /* DEP1 = result, DEP2 = 0, DEP3 = oldC:old_V */
    ARMG_CC_OP_MULL,        /* DEP1 = resLO32, DEP2 = resHI32, DEP3 = oldC:old_V */

    ARMG_CC_OP_NUMBER
};

static vector<Stmt *> mod_eflags_copy(bap_context_t *context, reg_t type, Exp *arg1, Exp *arg2);
static vector<Stmt *> mod_eflags_add(bap_context_t *context, reg_t type, Exp *arg1, Exp *arg2);
static vector<Stmt *> mod_eflags_sub(bap_context_t *context, reg_t type, Exp *arg1, Exp *arg2);
static vector<Stmt *> mod_eflags_adc(bap_context_t *context, reg_t type, Exp *arg1, Exp *arg2, Exp *arg3);
static vector<Stmt *> mod_eflags_sbb(bap_context_t *context, reg_t type, Exp *arg1, Exp *arg2, Exp *arg3);
static vector<Stmt *> mod_eflags_logic(bap_context_t *context, reg_t type, Exp *arg1, Exp *arg2, Exp *arg3);
static vector<Stmt *> mod_eflags_umul(bap_context_t *context, reg_t type, Exp *arg1, Exp *arg2, Exp *arg3);
static vector<Stmt *> mod_eflags_umull(bap_context_t *context, reg_t type, Exp *arg1, Exp *arg2, Exp *arg3);

vector<VarDecl *> arm_get_reg_decls()
{
    vector<VarDecl *> ret;
    reg_t r32 = REG_32;
    reg_t r1 = REG_1;

    // General purpose 32-bit registers
    ret.push_back(new VarDecl("R_R0",       r32));
    ret.push_back(new VarDecl("R_R1",       r32));
    ret.push_back(new VarDecl("R_R2",       r32));
    ret.push_back(new VarDecl("R_R3",       r32));
    ret.push_back(new VarDecl("R_R4",       r32));
    ret.push_back(new VarDecl("R_R5",       r32));
    ret.push_back(new VarDecl("R_R6",       r32));
    ret.push_back(new VarDecl("R_R7",       r32));
    ret.push_back(new VarDecl("R_R8",       r32));
    ret.push_back(new VarDecl("R_R9",       r32));
    ret.push_back(new VarDecl("R_R10",      r32));
    ret.push_back(new VarDecl("R_R11",      r32));
    ret.push_back(new VarDecl("R_R12",      r32));
    ret.push_back(new VarDecl("R_R13",      r32));
    ret.push_back(new VarDecl("R_R14",      r32));
    ret.push_back(new VarDecl("R_R15",      r32));

    // Flags
    ret.push_back(new VarDecl("R_NF",       r1));
    ret.push_back(new VarDecl("R_ZF",       r1));
    ret.push_back(new VarDecl("R_CF",       r1));
    ret.push_back(new VarDecl("R_VF",       r1));
    ret.push_back(new VarDecl("R_CC_OP",    r32));
    ret.push_back(new VarDecl("R_CC_DEP1",  r32));
    ret.push_back(new VarDecl("R_CC_DEP2",  r32));

    // IT block condition
    ret.push_back(new VarDecl("R_ITCOND",   r1));

    return ret;
}

//----------------------------------------------------------------------
// Translate VEX IR offset into ARM register name
// This is only called for 32-bit registers.
//----------------------------------------------------------------------
static string reg_offset_to_name(int offset)
{
    assert(offset >= 0);

    switch (offset)
    {
    case OFFB_R0:
    
        return "R0";
    
    case OFFB_R1:
    
        return "R1";
    
    case OFFB_R2:
    
        return "R2";
    
    case OFFB_R3:
    
        return "R3";
    
    case OFFB_R4:
    
        return "R4";
    
    case OFFB_R5:
    
        return "R5";
    
    case OFFB_R6:
    
        return "R6";
    
    case OFFB_R7:
    
        return "R7";
    
    case OFFB_R8:
    
        return "R8";
    
    case OFFB_R9:
    
        return "R9";
    
    case OFFB_R10:
    
        return "R10";
    
    case OFFB_R11:
    
        return "R11";
    
    case OFFB_R12:
    
        return "R12";
    
    case OFFB_R13:
    
        return "R13";
    
    case OFFB_R14:
    
        return "R14";
    
    case OFFB_R15T:
    
        return "R15T";

    case OFFB_CC_OP:
    
        return "CC_OP";
    
    case OFFB_CC_DEP1:
    
        return "CC_DEP1";
    
    case OFFB_CC_DEP2:
    
        return "CC_DEP2";
    
    case OFFB_CC_NDEP:
    
        return "CC_NDEP";

    case OFFB_ITSTATE:

        return "ITSTATE";
    
    default:
    
        panic("reg_offset_to_name(arm): Unrecognized register offset");
    }

    return NULL;
}

static Exp *translate_get_reg_32(int offset)
{
    assert(offset >= 0);

    Temp *reg = mk_reg(reg_offset_to_name(offset), REG_32);

    return reg;
}

Exp  *arm_translate_get(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout)
{
    int offset = expr->Iex.Get.offset;

    assert(typeOfIRExpr(irbb->tyenv, expr) == Ity_I32);

    return translate_get_reg_32(offset);
}

static Stmt *translate_put_reg_32(int offset, Exp *data, IRSB *irbb)
{
    assert(data);

    Temp *reg = mk_reg(reg_offset_to_name(offset), REG_32);

    return new Move(reg, data);
}

Stmt *arm_translate_put(bap_context_t *context, IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout)
{
    int offset = stmt->Ist.Put.offset;

    Exp *data = translate_expr(context, stmt->Ist.Put.data, irbb, irout);

    assert(typeOfIRExpr(irbb->tyenv, stmt->Ist.Put.data) == Ity_I32);

    return translate_put_reg_32(offset, data, irbb);
}

void arm_translate_ccall_args(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout)
{
    // we need it for cleanup of CCall arguments
    Internal *intr = new Internal(INTERNAL_VEX_FN_ARG_LIST, sizeof(internal_vex_fn_arg_list));
    assert(intr);

    // get pointer to arguments list structure
    internal_vex_fn_arg_list *arg_list = (internal_vex_fn_arg_list *)intr->data;
    assert(arg_list);

    // enumerate VEX function arguments
    for (int i = 0; i < 4; i++)
    {
        IRExpr *arg = expr->Iex.CCall.args[i];

        // check for Temp argument
        if (arg->tag == Iex_RdTmp) 
        {
            IRTemp temp_arg = arg->Iex.RdTmp.tmp;

            arg_list->args[arg_list->count].temp = temp_arg;
            arg_list->args[arg_list->count].type = translate_tmp_type(context, irbb, arg);

            arg_list->count += 1;
        }
    }

    irout->push_back(intr);
}

Exp *arm_translate_ccall(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout)
{
    assert(expr);
    assert(irbb);
    assert(irout);

    Exp *result = NULL;

    string func = string(expr->Iex.CCall.cee->name);

    Temp *NF = mk_reg("NF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *CF = mk_reg("CF", REG_1);
    Temp *VF = mk_reg("VF", REG_1);

    if (func == "armg_calculate_condition")
    {
        int arg = -1;
        IRExpr *cond_op = expr->Iex.CCall.args[0];     

        /*
            Get value of the condition type for armg_calculate_condition.
            Example of VEX code:

            t5 = GET:I32(72)
            t4 = Or32(t5,0xC0:I32)
            t6 = GET:I32(76)
            t7 = GET:I32(80)
            t8 = GET:I32(84)
            t9 = armg_calculate_condition[mcx=0x9]{0xa6974280}(t4,t6,t7,t8):I32

            ... where desired value is 0xC0:I32
        */
        if (cond_op->tag == Iex_RdTmp) 
        {
            IRTemp look_for = cond_op->Iex.RdTmp.tmp;

            // enumerate VEX IR statements
            for (int i = 0; i < irbb->stmts_used; i++)
            {
                IRStmt *st = irbb->stmts[i];

                // check for desired temp register assignment
                if (st->tag == Ist_WrTmp &&
                    st->Ist.WrTmp.tmp == look_for &&
                    st->Ist.WrTmp.data->tag == Iex_Binop &&
                    st->Ist.WrTmp.data->Iex.Binop.op == Iop_Or32)
                {
                    IRExpr *e = st->Ist.WrTmp.data->Iex.Binop.arg2;

                    // check for constant 2-nd argument of Iop_Or32
                    if (e->tag == Iex_Const &&
                        e->Iex.Const.con->tag == Ico_U32)
                    {
                        arg = e->Iex.Const.con->Ico.U32;
                    }

                    break;                    
                }
            }
        }

        if (arg == -1)
        {
            /* 
                Unable to get CC_OP value from current IR block,
                use value that was set in previous instructions instead.
            */
            arg = context->flag_thunks.op;
        }

        /*
            Get cond_n_op argument value in according to the VEX
            implementation of the armg_calculate_condition():

                UInt armg_calculate_condition ( UInt cond_n_op,
                                    UInt cc_dep1,
                                    UInt cc_dep2, UInt cc_dep3 )
                {
                   UInt cond  = cond_n_op >> 4;
                   UInt cc_op = cond_n_op & 0xF;

                   ... 

                   switch (cond) {
                      case ARMCondEQ:    // Z=1         => z
                      case ARMCondNE:    // Z=0

                   ...
        */
        arg >>= 4;

        switch (arg)
        {
        case ARMCondEQ:

            result = _ex_u_cast(ecl(ZF), REG_32);
            break;

        case ARMCondNE:

            result = _ex_u_cast(ex_not(ZF), REG_32);
            break;

        case ARMCondHS:

            result = _ex_u_cast(ecl(CF), REG_32);
            break;

        case ARMCondLO:

            result = _ex_u_cast(ex_not(CF), REG_32);
            break;

        case ARMCondMI:

            result = _ex_u_cast(ecl(NF), REG_32);
            break;

        case ARMCondPL:

            result = _ex_u_cast(ex_not(NF), REG_32);
            break;

        case ARMCondVS:

            result = _ex_u_cast(ecl(VF), REG_32);
            break;

        case ARMCondVC:

            result = _ex_u_cast(ex_not(VF), REG_32);
            break;

        case ARMCondHI:

            result = _ex_u_cast(_ex_and(ecl(CF), ex_not(ZF)), REG_32);
            break;

        case ARMCondLS:

            result = _ex_u_cast(_ex_or(ex_not(CF), ecl(ZF)), REG_32);
            break;

        case ARMCondGE:

            result = _ex_u_cast(ex_eq(NF, VF), REG_32);
            break;

        case ARMCondLT:

            result = _ex_u_cast(ex_neq(NF, VF), REG_32);
            break;

        case ARMCondGT:

            result = _ex_u_cast(_ex_and(ex_not(ZF), ex_eq(NF, VF)), REG_32);
            break;

        case ARMCondLE:

            result = _ex_u_cast(_ex_or(ecl(ZF), ex_neq(NF, VF)), REG_32);
            break;

        case ARMCondAL:

            result = new Constant(REG_32, 1);
            break;

        case ARMCondNV:

            result = new Constant(REG_32, 0);
            break;

        default:
        
            panic("Unrecognized condition for armg_calculate_condition");
        }

        // create Internal with CCall arguments information
        arm_translate_ccall_args(context, expr, irbb, irout);
    }
    else if (func == "armg_calculate_flag_n")
    {
        // create Internal with CCall arguments information
        arm_translate_ccall_args(context, expr, irbb, irout);

        result = _ex_u_cast(ecl(NF), REG_32);
    }
    else if (func == "armg_calculate_flag_z")
    {
        // create Internal with CCall arguments information
        arm_translate_ccall_args(context, expr, irbb, irout);

        result = _ex_u_cast(ecl(ZF), REG_32);
    }
    else if (func == "armg_calculate_flag_c")
    {
        // create Internal with CCall arguments information
        arm_translate_ccall_args(context, expr, irbb, irout);

        result = _ex_u_cast(ecl(CF), REG_32);
    }
    else if (func == "armg_calculate_flag_v")
    {
        // create Internal with CCall arguments information
        arm_translate_ccall_args(context, expr, irbb, irout);

        result = _ex_u_cast(ecl(VF), REG_32);
    }
    else
    {
        result = new Unknown("CCall: " + func, regt_of_irexpr(irbb, expr));
    }

    delete NF;
    delete ZF;
    delete CF;
    delete VF;

    return result;
}

static vector<Stmt *> mod_eflags_copy(bap_context_t *context, reg_t type, Exp *arg1, Exp *arg2)
{
    vector<Stmt *> irout;

    // All the static constants we'll ever need
    Constant c_0(REG_32, 0);
    Constant c_1(REG_32, 1);
    Constant c_ARMG_CC_SHIFT_N(REG_32, 31);
    Constant c_ARMG_CC_SHIFT_Z(REG_32, 30);
    Constant c_ARMG_CC_SHIFT_C(REG_32, 29);
    Constant c_ARMG_CC_SHIFT_V(REG_32, 28);

    // Calculate flags
    Temp *NF = mk_reg("NF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *CF = mk_reg("CF", REG_1);
    Temp *VF = mk_reg("VF", REG_1);

    // v = (dep1 >> ARMG_CC_SHIFT_V) & 1
    Exp *condVF = _ex_neq(
        _ex_and(
            _ex_shr(ecl(arg1), ecl(&c_ARMG_CC_SHIFT_V)), 
            ecl(&c_1)
        ), 
        ecl(&c_0)
    );

    set_flag(&irout, type, VF, condVF);

    // c = (dep1 >> ARMG_CC_SHIFT_C) & 1
    Exp *condCF = _ex_neq(
        _ex_and(
            _ex_shr(ecl(arg1), ecl(&c_ARMG_CC_SHIFT_C)), 
            ecl(&c_1)
        ), 
        ecl(&c_0)
    );

    set_flag(&irout, type, CF, condCF);

    // z = (dep1 >> ARMG_CC_SHIFT_Z) & 1
    Exp *condZF = _ex_neq(
        _ex_and(
            _ex_shr(ecl(arg1), ecl(&c_ARMG_CC_SHIFT_Z)), 
            ecl(&c_1)
        ), 
        ecl(&c_0)
    );

    set_flag(&irout, type, ZF, condZF);

    // n = (dep1 >> ARMG_CC_SHIFT_N) & 1
    Exp *condNF = _ex_neq(
        _ex_and(
            _ex_shr(ecl(arg1), ecl(&c_ARMG_CC_SHIFT_N)), 
            ecl(&c_1)
        ), 
        ecl(&c_0)
    );

    set_flag(&irout, type, NF, condNF);

    return irout;
}

static vector<Stmt *> mod_eflags_add(bap_context_t *context, reg_t type, Exp *arg1, Exp *arg2)
{
    vector<Stmt *> irout;
    Temp *res = mk_temp(REG_32, &irout);

    // All the static constants we'll ever need
    Constant c_0(REG_32, 0);
    Constant c_31(REG_32, 31);

    // The operation itself: res = dep1 + dep2
    irout.push_back(new Move(res, ex_add(arg1, arg2), type));

    // Calculate flags
    Temp *NF = mk_reg("NF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *CF = mk_reg("CF", REG_1);
    Temp *VF = mk_reg("VF", REG_1);

    // n = res >> 31
    Exp *condNF = _ex_neq(_ex_shr(ecl(res), ecl(&c_31)), ecl(&c_0));
    set_flag(&irout, type, NF, condNF);

    // z = res == 0
    Exp *condZF = ex_eq(res, &c_0);
    set_flag(&irout, type, ZF, condZF);

    // c = res < dep1    
    Exp *condCF = ex_lt(res, arg1);
    set_flag(&irout, type, CF, condCF);

    // v = ((res ^ dep1) & (res ^ dep2)) >> 31
    Exp *condVF = _ex_neq(
        _ex_shr(
            _ex_and(
                ex_xor(res, arg1), 
                ex_xor(res, arg2)
            ), 
            ecl(&c_31)
        ), 
        ecl(&c_0)
    );

    set_flag(&irout, type, VF, condVF);

    return irout;
}

static vector<Stmt *> mod_eflags_sub(bap_context_t *context, reg_t type, Exp *arg1, Exp *arg2)
{
    vector<Stmt *> irout;
    Temp *res = mk_temp(REG_32, &irout);

    // All the static constants we'll ever need
    Constant c_0(REG_32, 0);
    Constant c_31(REG_32, 31);

    // The operation itself: res = dep1 - dep2
    irout.push_back(new Move(res, ex_sub(arg1, arg2)));

    // Calculate flags
    Temp *NF = mk_reg("NF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *CF = mk_reg("CF", REG_1);
    Temp *VF = mk_reg("VF", REG_1);

    // n = res >> 31
    Exp *condNF = _ex_neq(_ex_shr(ecl(res), ecl(&c_31)), ecl(&c_0));
    set_flag(&irout, type, NF, condNF);

    // z = res == 0    
    Exp *condZF = ex_eq(res, &c_0);
    set_flag(&irout, type, ZF, condZF);

    // c = dep1 >= dep2
    Exp *condCF = ex_ge(arg1, arg2);
    set_flag(&irout, type, CF, condCF);
    
    // v = ((dep1 ^ dep2) & (dep1 ^ res)) >> 31    
    Exp *condVF = _ex_neq(
        _ex_shr(
            _ex_and(
                ex_xor(arg1, arg2), 
                ex_xor(arg1, res)
            ), 
            ecl(&c_31)
        ), 
        ecl(&c_0)
    );

    set_flag(&irout, type, VF, condVF);

    return irout;
}

static vector<Stmt *> mod_eflags_adc(bap_context_t *context, reg_t type, Exp *arg1, Exp *arg2, Exp *arg3)
{
    vector<Stmt *> irout;
    Temp *res = mk_temp(REG_32, &irout);

    // All the static constants we'll ever need
    Constant c_0(REG_32, 0);
    Constant c_31(REG_32, 31);

    // The operation itself: res = dep1 + dep2 + dep3  
    irout.push_back(new Move(res, _ex_add(ex_add(arg1, arg2), ecl(arg3))));

    // Calculate flags
    Temp *NF = mk_reg("NF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *CF = mk_reg("CF", REG_1);
    Temp *VF = mk_reg("VF", REG_1);

    // v = ((res ^ dep1) & (res ^ dep2)) >> 31
    Exp *condVF = _ex_neq(
        _ex_shr(
            _ex_and(
                _ex_xor(ecl(res), ecl(arg1)), 
                _ex_xor(ecl(res), ecl(arg2))
            ),
            ecl(&c_31)
        ), 
        ecl(&c_0)
    );

    set_flag(&irout, type, VF, condVF);

    // c = dep3 ? (res <= dep1) : (res < dep1)
    Exp *condCF = _ex_neq(
        emit_mux0x(
            &irout, REG_32, arg3, 
            ex_le(res, arg1), 
            ex_lt(res, arg1)
        ),
        ecl(&c_0)
    );  

    set_flag(&irout, type, CF, condCF);

    // z = res == 0
    Exp *condZF = _ex_eq(ecl(res), ecl(&c_0));

    set_flag(&irout, type, ZF, condZF);

    // n = res >> 31
    Exp *condNF = _ex_neq(_ex_shr(ecl(res), ecl(&c_31)), ecl(&c_0));

    return irout;
}

static vector<Stmt *> mod_eflags_sbb(bap_context_t *context, reg_t type, Exp *arg1, Exp *arg2, Exp *arg3)
{
    vector<Stmt *> irout;
    Temp *res = mk_temp(REG_32, &irout);

    // All the static constants we'll ever need
    Constant c_0(REG_32, 0);
    Constant c_1(REG_32, 1);
    Constant c_31(REG_32, 31);    

    // The operation itself: res = dep1 - dep2 - (dep3 ^ 1)
    irout.push_back(new Move(res, _ex_sub(ex_sub(arg1, arg2), ex_xor(arg3, &c_1))));

    // Calculate flags
    Temp *NF = mk_reg("NF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *CF = mk_reg("CF", REG_1);
    Temp *VF = mk_reg("VF", REG_1);

    // v = ((dep1 ^ dep2) & (dep1 ^ res)) >> 31
    Exp *condVF = _ex_neq(
        _ex_shr(
            _ex_and(
                _ex_xor(ecl(arg1), ecl(arg2)), 
                _ex_xor(ecl(arg1), ecl(res))
            ),
            ecl(&c_31)
        ), 
        ecl(&c_0)
    );

    set_flag(&irout, type, VF, condVF);

    // c = dep3 ? (dep1 >= dep2) : (dep1 > dep2);
    Exp *condCF = _ex_neq(
        emit_mux0x(
            &irout, REG_32, arg3, 
            ex_ge(arg1, arg2), 
            ex_gt(arg1, arg2)
        ),
        ecl(&c_0)
    );  

    set_flag(&irout, type, CF, condCF);

    // z = res == 0
    Exp *condZF = _ex_eq(ecl(res), ecl(&c_0));

    set_flag(&irout, type, ZF, condZF);

    // n = res >> 31
    Exp *condNF = _ex_neq(_ex_shr(ecl(res), ecl(&c_31)), ecl(&c_0));

    set_flag(&irout, type, NF, condNF);

    return irout;
}

static vector<Stmt *> mod_eflags_logic(bap_context_t *context, reg_t type, Exp *arg1, Exp *arg2, Exp *arg3)
{
    vector<Stmt *> irout;

    // All the static constants we'll ever need
    Constant c_0(REG_32, 0);
    Constant c_31(REG_32, 31);

    // Calculate flags
    Temp *NF = mk_reg("NF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *CF = mk_reg("CF", REG_1);
    Temp *VF = mk_reg("VF", REG_1);

    // n = dep1 >> 31
    Exp *condNF = _ex_neq(_ex_shr(ecl(arg1), ecl(&c_31)), ecl(&c_0));
    set_flag(&irout, type, NF, condNF);

    // z = dep1 == 0
    Exp *condZF = ex_eq(arg1, &c_0);
    set_flag(&irout, type, ZF, condZF);

    // c = dep2
    set_flag(&irout, type, CF, _ex_neq(ecl(arg2), ecl(&c_0)));

    // v = dep3
    set_flag(&irout, type, VF, _ex_neq(ecl(arg3), ecl(&c_0)));

    return irout;
}

static vector<Stmt *> mod_eflags_umul(bap_context_t *context, reg_t type, Exp *arg1, Exp *arg2, Exp *arg3)
{
    vector<Stmt *> irout;

    // All the static constants we'll ever need
    Constant c_0(REG_32, 0);
    Constant c_1(REG_32, 1);
    Constant c_31(REG_32, 31);

    // Calculate flags
    Temp *NF = mk_reg("NF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *CF = mk_reg("CF", REG_1);
    Temp *VF = mk_reg("VF", REG_1);

    // v = (dep3 >> 0) & 1
    Exp *condVF = _ex_neq(
        _ex_and(
            _ex_shr(ecl(arg3), ecl(&c_0)), 
            ecl(&c_1)
        ), 
        ecl(&c_0)
    );

    set_flag(&irout, type, VF, condVF);

    // c = (dep3 >> 1) & 1
    Exp *condCF = _ex_neq(
        _ex_and(
            _ex_shr(ecl(arg3), ecl(&c_1)), 
            ecl(&c_1)
        ), 
        ecl(&c_0)
    );

    set_flag(&irout, type, CF, condCF);
    
    // z = dep1 == 0
    Exp *condZF = _ex_eq(ecl(arg1), ecl(&c_0));

    set_flag(&irout, type, ZF, condZF);

    // n = dep1 >> 31
    Exp *condNF = _ex_neq(_ex_shr(ecl(arg1), ecl(&c_31)), ecl(&c_0));

    set_flag(&irout, type, NF, condNF);

    return irout;
}

static vector<Stmt *> mod_eflags_umull(bap_context_t *context, reg_t type, Exp *arg1, Exp *arg2, Exp *arg3)
{
    vector<Stmt *> irout;

    // All the static constants we'll ever need
    Constant c_0(REG_32, 0);
    Constant c_1(REG_32, 1);
    Constant c_31(REG_32, 31);

    // Calculate flags
    Temp *NF = mk_reg("NF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *CF = mk_reg("CF", REG_1);
    Temp *VF = mk_reg("VF", REG_1);

    // v = (dep3 >> 0) & 1
    Exp *condVF = _ex_neq(
        _ex_and(
            _ex_shr(ecl(arg3), ecl(&c_0)), 
            ecl(&c_1)
        ), 
        ecl(&c_0)
    );

    set_flag(&irout, type, VF, condVF);

    // c = (dep3 >> 1) & 1
    Exp *condCF = _ex_neq(
        _ex_and(
            _ex_shr(ecl(arg3), ecl(&c_1)), 
            ecl(&c_1)
        ), 
        ecl(&c_0)
    );

    set_flag(&irout, type, CF, condCF);
    
    // z = (dep1 | dep2) == 0
    Exp *condZF = _ex_eq(_ex_or(ecl(arg1), ecl(arg2)), ecl(&c_0));

    set_flag(&irout, type, ZF, condZF);

    // n = dep2 >> 31
    Exp *condNF = _ex_neq(_ex_shr(ecl(arg2), ecl(&c_31)), ecl(&c_0));

    set_flag(&irout, type, NF, condNF);

    return irout;
}

void arm_modify_flags(bap_context_t *context, bap_block_t *block)
{
    assert(block);

    vector<Stmt *> *ir = block->bap_ir;
    int opi = -1, dep1 = -1, dep2 = -1, ndep = -1, mux0x = -1;

    // Look for occurrence of CC_OP assignment
    // These will have the indices of the CC_OP stmts
    get_put_thunk(block, &opi, &dep1, &dep2, &ndep, &mux0x);

    if (opi == -1)        
    {
        // doesn't set flags
        return;
    }

    Stmt *stmt = ir->at(opi);
    Mod_Func_0 *cb = NULL;
    int num_params = 0, op = 0;
    bool got_op = false;

    if (stmt->stmt_type == MOVE)
    {
        Move *move = (Move *)stmt;

        if (move->rhs->exp_type == CONSTANT)
        {
            Constant *constant = (Constant *)move->rhs;

            op = (int)constant->val;
            got_op = true;
        }
        else if (mux0x != -1)
        {
            Exp *cond = NULL, *exp0 = NULL, *expx = NULL, *res = NULL;

            // check if mux0x was used for CC_OP
            if (match_mux0x(ir, opi - MUX_SUB, &cond, &exp0, &expx, &res) >= 0)
            {
                Name *name = mk_dest_name(context->inst + context->inst_size);
                Label *label = mk_label();

                // insert conditional statement
                ir->insert(ir->begin() + opi - MUX_SUB, label); 
                ir->insert(ir->begin() + opi - MUX_SUB, new CJmp(ecl(cond), new Name(label->label), name));

                if (exp0->exp_type == CONSTANT)
                {
                    Constant *constant = (Constant *)exp0;

                    op = (int)constant->val;
                    got_op = true;
                }
            }
        }
    }    

    if (got_op)
    {        
        switch (op) 
        {
        case ARMG_CC_OP_COPY: 

            num_params = 2;
            cb = (Mod_Func_0 *)mod_eflags_copy;
            break;
        
        case ARMG_CC_OP_ADD: 
            
            num_params = 2;
            cb = (Mod_Func_0 *)mod_eflags_add;
            break;
          
        case ARMG_CC_OP_SUB: 
            
            num_params = 2;
            cb = (Mod_Func_0 *)mod_eflags_sub;
            break;

        case ARMG_CC_OP_ADC: 
            
            num_params = 3;
            cb = (Mod_Func_0 *)mod_eflags_adc;
            break;

        case ARMG_CC_OP_SBB: 
            
            num_params = 3;
            cb = (Mod_Func_0 *)mod_eflags_sbb;
            break;
          
        case ARMG_CC_OP_LOGIC: 
            
            num_params = 3;
            cb = (Mod_Func_0 *)mod_eflags_logic;
            break;

        case ARMG_CC_OP_MUL: 
            
            num_params = 3;
            cb = (Mod_Func_0 *)mod_eflags_umul;
            break;

        case ARMG_CC_OP_MULL: 
            
            num_params = 3;
            cb = (Mod_Func_0 *)mod_eflags_umull;
            break;

        default:
         
            panic("unhandled CC_OP");
        }

        modify_eflags_helper(context, block, block->str_mnem, REG_1, num_params, cb);     
    }
    else
    {
        panic("unexpected CC_OP");   
    }
}

void arm_modify_itstate(bap_context_t *context, bap_block_t *block)
{
    assert(block);

    vector<Stmt *> *ir = block->bap_ir;
    int n = -1;

    // Look for occurrence of ITSTATE assignment
    get_put_itstate(block, &n);

    if (n == -1)        
    {
        // doesn't set ITSTATE
        return;
    }

    Stmt *stmt = ir->at(n);
    uint32_t itstate = 0;

    if (stmt->stmt_type == MOVE)
    {
        Move *move = (Move *)stmt;

        if (move->rhs->exp_type == CONSTANT)
        {
            Constant *constant = (Constant *)move->rhs;

            itstate = (uint32_t)constant->val;
        }
    }

    // delete statements that sets ITSTATE
    del_put_itstate(block, n);

    if (itstate == 0)
    {
        // current instruction is outside of the IT block
        return;
    }

    // get condition type from current ITSTATE lane
    uint8_t lane = (uint8_t)itstate;    

    ARMCondcode cond = (ARMCondcode)(((lane >> 4) & 0xF) ^ 0xE);

    Exp *result = NULL;

    Temp *NF = mk_reg("NF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *CF = mk_reg("CF", REG_1);
    Temp *VF = mk_reg("VF", REG_1);

    Temp *ITCOND_1 = mk_reg("ITCOND_1", REG_1);
    Temp *ITCOND_2 = mk_reg("ITCOND_2", REG_1);
    Temp *ITCOND_3 = mk_reg("ITCOND_3", REG_1);
    Temp *ITCOND_4 = mk_reg("ITCOND_4", REG_1);

    switch (cond)
    {
    case ARMCondEQ:

        result = ecl(ITCOND_2);
        break;

    case ARMCondNE:

        result = ex_not(ITCOND_2);
        break;

    case ARMCondHS:

        result = ecl(CF);
        break;

    case ARMCondLO:

        result = ex_not(CF);
        break;

    case ARMCondMI:

        result = ecl(NF);
        break;

    case ARMCondPL:

        result = ex_not(NF);
        break;

    case ARMCondVS:

        result = ecl(VF);
        break;

    case ARMCondVC:

        result = ex_not(VF);
        break;

    case ARMCondHI:

        result = _ex_and(ecl(CF), ex_not(ZF));
        break;

    case ARMCondLS:

        result = _ex_or(ex_not(CF), ecl(ZF));
        break;

    case ARMCondGE:

        result = ex_eq(NF, VF);
        break;

    case ARMCondLT:

        result = ex_neq(ITCOND_1, ITCOND_4);
        break;
    
    case ARMCondGT:

        result = _ex_and(ex_not(ITCOND_2), ex_eq(ITCOND_1, ITCOND_4));
        break;

    case ARMCondLE:

        result = _ex_or(ecl(ZF), ex_neq(NF, VF));
        break;

    case ARMCondAL:

        result = new Constant(REG_1, 1);
        break;

    case ARMCondNV:

        result = new Constant(REG_1, 0);
        break;

    default:

        panic("arm_modify_itstate(): Unexpected condition");
    }

    Name *name = mk_dest_name(context->inst + context->inst_size);
    Label *label = mk_label();

    // insert conditional jump for IT block
    ir->insert(ir->begin() + 1, label);
    ir->insert(ir->begin() + 1, new CJmp(new UnOp(NOT, result), name, new Name(label->label)));

    delete ITCOND_4;
    delete ITCOND_3;
    delete ITCOND_2;
    delete ITCOND_1;

    delete NF;
    delete ZF;
    delete CF;
    delete VF;
}

void arm_modify_itstate_cond(bap_context_t *context, bap_block_t *block)
{
    assert(block);

    vector<Stmt *> *ir = block->bap_ir;

    if (block->str_mnem.find("it") == 0)
    {
        ir->insert(ir->begin() + 1, new Move(mk_reg("ITCOND_1", REG_1),
                                             mk_reg("NF", REG_1)));

        ir->insert(ir->begin() + 1, new Move(mk_reg("ITCOND_2", REG_1),
                                             mk_reg("ZF", REG_1)));

        ir->insert(ir->begin() + 1, new Move(mk_reg("ITCOND_3", REG_1),
                                             mk_reg("CF", REG_1)));

        ir->insert(ir->begin() + 1, new Move(mk_reg("ITCOND_4", REG_1),
                                             mk_reg("VF", REG_1)));
    }
}
