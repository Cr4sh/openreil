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
    ret.push_back(new VarDecl("CC_OP",      r32));
    ret.push_back(new VarDecl("CC_DEP1",    r32));
    ret.push_back(new VarDecl("CC_DEP2",    r32));

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

        fprintf(stderr, "%d\n", offset);
    
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

Exp  *arm_translate_get(IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout)
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

Stmt *arm_translate_put(IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout)
{
    int offset = stmt->Ist.Put.offset;

    Exp *data = translate_expr(stmt->Ist.Put.data, irbb, irout);

    assert(typeOfIRExpr(irbb->tyenv, stmt->Ist.Put.data) == Ity_I32);

    return translate_put_reg_32(offset, data, irbb);
}

Exp *arm_translate_ccall(IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout)
{
    assert(expr);
    assert(irbb);
    assert(irout);

    Exp *result = NULL;

    string func = string(expr->Iex.CCall.cee->name);

    if (func == "armg_calculate_condition")
    {
        panic("armg_calculate_condition");
    }

    return new Unknown("CCall: " + func, regt_of_irexpr(irbb, expr));
}

static vector<Stmt *> mod_eflags_copy(reg_t type, Exp *arg1, Exp *arg2)
{
    vector<Stmt *> irout;

    // v = (dep1 >> ARMG_CC_SHIFT_V) & 1
    // c = (dep1 >> ARMG_CC_SHIFT_C) & 1
    // z = (dep1 >> ARMG_CC_SHIFT_Z) & 1
    // n = (dep1 >> ARMG_CC_SHIFT_N) & 1
    panic("mod_eflags_copy");

    return irout;
}

static vector<Stmt *> mod_eflags_add(reg_t type, Exp *arg1, Exp *arg2)
{
    vector<Stmt *> irout;
    Temp *res = mk_temp(REG_32, &irout);

    // The operation itself
    irout.push_back(new Move(res, ex_add(arg1, arg2), type));

    // Calculate flags
    Temp *NF = mk_reg("NF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *CF = mk_reg("CF", REG_1);
    Temp *VF = mk_reg("VF", REG_1);

    // res = dep1 + dep2
    // v = ((res ^ dep1) & (res ^ dep2)) >> 31
    // c = res < dep1
    // z = res == 0
    // n = res >> 31
    panic("mod_eflags_add");

    return irout;
}

static vector<Stmt *> mod_eflags_sub(reg_t type, Exp *arg1, Exp *arg2)
{
    vector<Stmt *> irout;
    Temp *res = mk_temp(REG_32, &irout);

    // The operation itself
    irout.push_back(new Move(res, ex_sub(arg1, arg2)));

    // Calculate flags
    Temp *NF = mk_reg("NF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *CF = mk_reg("CF", REG_1);
    Temp *VF = mk_reg("VF", REG_1);

    // res = dep1 - dep2
    // v = ((dep1 ^ dep2) & (dep1 ^ res)) >> 31
    // c = dep1 >= dep2
    // z = res == 0
    // n = res >> 31
    panic("mod_eflags_sub");

    return irout;
}

static vector<Stmt *> mod_eflags_adc(reg_t type, Exp *arg1, Exp *arg2, Exp *arg3)
{
    vector<Stmt *> irout;
    Temp *res = mk_temp(REG_32, &irout);

    // The operation itself
    irout.push_back(new Move(res, _ex_add(ex_add(arg1, arg2), ecl(arg3))));

    // Calculate flags
    Temp *NF = mk_reg("NF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *CF = mk_reg("CF", REG_1);
    Temp *VF = mk_reg("VF", REG_1);

    // res = dep1 + dep2 + dep3  
    // v = ((res ^ dep1) & (res ^ dep2)) >> 31
    // c = dep3 ? (res <= dep1) : (res < dep1)
    // z = res == 0
    // n = res >> 31
    panic("mod_eflags_adc");

    return irout;
}

static vector<Stmt *> mod_eflags_sbb(reg_t type, Exp *arg1, Exp *arg2, Exp *arg3)
{
    vector<Stmt *> irout;
    Temp *res = mk_temp(REG_32, &irout);

    // All the static constants we'll ever need
    Constant c_1(REG_32, 1);

    // The operation itself
    irout.push_back(new Move(res, _ex_sub(ex_sub(arg1, arg2), ex_xor(arg3, &c_1))));

    // res = dep1 - dep2 - (dep3 ^ 1)
    // v = ((dep1 ^ dep2) & (dep1 ^ res)) >> 31
    // c = dep3 ? (dep1 >= dep2) : (dep1 > dep2);
    // z = res == 0
    // n = res >> 31
    panic("mod_eflags_sbb");

    return irout;
}

static vector<Stmt *> mod_eflags_logic(reg_t type, Exp *arg1, Exp *arg2)
{
    vector<Stmt *> irout;

    // Calculate flags
    Temp *NF = mk_reg("NF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *CF = mk_reg("CF", REG_1);
    Temp *VF = mk_reg("VF", REG_1);

    // v = dep3
    // c = dep2
    // z = dep1 == 0
    // n = dep1 >> 31
    panic("mod_eflags_logic");

    return irout;
}

static vector<Stmt *> mod_eflags_umul(reg_t type, Exp *arg1, Exp *arg2)
{
    vector<Stmt *> irout;

    // Calculate flags
    Temp *NF = mk_reg("NF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *CF = mk_reg("CF", REG_1);
    Temp *VF = mk_reg("VF", REG_1);

    // v = (dep3 >> 0) & 1
    // c = (dep3 >> 1) & 1
    // z = dep1 == 0
    // n = dep1 >> 31
    panic("mod_eflags_umul");

    return irout;
}

static vector<Stmt *> mod_eflags_umull(reg_t type, Exp *arg1, Exp *arg2, Exp *arg3)
{
    vector<Stmt *> irout;

    // Calculate flags
    Temp *NF = mk_reg("NF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *CF = mk_reg("CF", REG_1);
    Temp *VF = mk_reg("VF", REG_1);

    // v = (dep3 >> 0) & 1
    // c = (dep3 >> 1) & 1
    // z = (dep1 | dep2) == 0
    // n = dep2 >> 31
    panic("mod_eflags_umull");

    return irout;
}

void arm_modify_flags(bap_block_t *block)
{
    assert(block);

    vector<Stmt *> *ir = block->bap_ir;

    // Look for occurrence of CC_OP assignment
    // These will have the indices of the CC_OP stmts
    int opi, dep1, dep2, ndep, mux0x;
    opi = dep1 = dep2 = ndep = mux0x = -1;
    get_thunk_index(ir, &opi, &dep1, &dep2, &ndep, &mux0x);

    if (opi == -1)        
    {
        // doesn't set flags
        return;
    }

    Stmt *op_stmt = ir->at(opi);
    bool got_op = false;
    int op = 0;

    if (op_stmt->stmt_type == MOVE)
    {
        Move *op_mov = (Move *)op_stmt;

        if (!(op_mov->rhs->exp_type == CONSTANT))
        {
            got_op = false;
        }
        else
        {
            Constant *op_const = (Constant *)op_mov->rhs;
            op = op_const->val;
            got_op = true;
        }
    }

    if (got_op)
    {
        Mod_Func_0 *cb = NULL;
        int num_params = 0;

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
            
            num_params = 2;
            cb = (Mod_Func_0 *)mod_eflags_logic;
            break;

        case ARMG_CC_OP_MUL: 
            
            num_params = 2;
            cb = (Mod_Func_0 *)mod_eflags_umul;
            break;

        case ARMG_CC_OP_MULL: 
            
            num_params = 3;
            cb = (Mod_Func_0 *)mod_eflags_umull;
            break;

        default:
         
            panic("unhandled cc_op!");
        }

        if (cb)
        {
            string op = block->str_mnem;

            modify_eflags_helper(op, REG_32, ir, num_params, cb);
        }
        else
        {
            panic("eflags handler is not set!");   
        }
    }
}

