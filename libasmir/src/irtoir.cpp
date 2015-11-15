#include <string>
#include <vector>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <assert.h>

using namespace std;

#include "irtoir-internal.h"
#include "disasm.h"

#if VEX_VERSION >= 1793
#define Ist_MFence Ist_MBE
#endif

// enable/disable lazy eflags computation.
// this is for transitional purposes, and should be removed soon.
bool use_eflags_thunks = 0;

// Use R_XS_BASE registers instead of gdt/ldt
bool use_simple_segments = 1;
bool translate_calls_and_returns = 0;

//
// For labeling VEX IR instructions that was not translated
//
string uTag = "Unknown: ";
string sTag = "Skipped: ";


//======================================================================
// Forward declarations
//======================================================================
Exp *emit_mux0x(vector<Stmt *> *irout, reg_t type, Exp *cond, Exp *exp0, Exp *expX);
void insert_specials(bap_context_t *context, bap_block_t *block);

void track_itstate(bap_context_t *context, bap_block_t *block);
void track_flags(bap_context_t *context, bap_block_t *block);
void modify_flags(bap_context_t *context, bap_block_t *block);

void do_cleanups_before_processing(bap_context_t *context);


//======================================================================
//
// Helper functions for the translation
//
//======================================================================

// Set whether to use the thunk code with function calls, or not.
void set_use_eflags_thunks(bool value)
{
    use_eflags_thunks = value;
}

// Set whether to use code with simple segments or not.
void asmir_set_use_simple_segments(bool value)
{
    use_simple_segments = value;
}

// Terrible name, but to be consistent, named similar to above.
// Return what the current eflags thunks values is
bool get_use_eflags_thunks()
{
    return use_eflags_thunks;
}

void set_call_return_translation(int value)
{
    translate_calls_and_returns = (bool)value;
}

//
// This function CONSUMES the cond and flag expressions passed in, i.e.
// they are not cloned before use.
//
void set_flag(vector<Stmt *> *irout, reg_t type, Temp *flag, Exp *cond)
{
    // set flag directly to condition
    irout->push_back(new Move(flag, cond));
}

Exp *modify_eflags_helper_mux0x(bap_block_t *block, int n)
{
    assert(block);

    Exp *ret = NULL;
    vector<Stmt *> *ir = block->bap_ir;    

    // check for mux0x
    if (match_mux0x(ir, n - MUX_SUB, NULL, &ret, NULL, NULL) >= 0)
    {
        // return true branch
        return ret;
    }

    return ((Move *)(ir->at(n)))->rhs;
}

void modify_eflags_helper(bap_context_t *context, bap_block_t *block, string op, reg_t type, int argnum, Mod_Func_0 *mod_eflags_func)
{
    assert(block);
    assert(argnum == 2 || argnum == 3);
    assert(mod_eflags_func);    
    
    vector<Stmt *> *ir = block->bap_ir;
    int opi = -1, dep1 = -1, dep2 = -1, ndep = -1, mux0x = -1;

    // Look for occurrence of CC_OP assignment
    // These will have the indices of the CC_OP stmts
    get_put_thunk(block, &opi, &dep1, &dep2, &ndep, &mux0x);

    if (opi >= 0)
    {
        vector<Stmt *> mods;

        if (argnum == 2)
        {
            // Get the arguments we need from these Stmt's
            Exp *arg1 = modify_eflags_helper_mux0x(block, dep1);
            Exp *arg2 = modify_eflags_helper_mux0x(block, dep2);

            // Do the translation
            // To figure out the type, we assume the rhs of the
            // assignment to CC_DEP is always either a Constant or a Temp
            // Otherwise, we have no way of figuring out the expression type
            Mod_Func_2 *mod_func = (Mod_Func_2 *)mod_eflags_func;
            mods = mod_func(context, type, arg1, arg2);
        }
        else // argnum == 3
        {
            Exp *arg1 = modify_eflags_helper_mux0x(block, dep1);
            Exp *arg2 = modify_eflags_helper_mux0x(block, dep2);
            Exp *arg3 = modify_eflags_helper_mux0x(block, ndep);

            Mod_Func_3 *mod_func = (Mod_Func_3 *)mod_eflags_func;
            mods = mod_func(context, type, arg1, arg2, arg3);
        }

        if (!use_eflags_thunks && !i386_op_is_very_broken(context->guest, op))
        {
            // Delete the thunk
            int pos = del_put_thunk(block, opi, dep1, dep2, ndep, mux0x);
            if (pos != -1)
            {
                // Insert the eflags mods in this position
                ir->insert(ir->begin() + pos, mods.begin(), mods.end());
                ir->insert(ir->begin() + pos, new Comment("eflags thunk: " + op));                
            }
            else
            {
                log_write(LOG_WARN, "del_put_thunk() fails for \"%s\"!", op.c_str());
            }
        }
    }
    else
    {
        log_write(LOG_WARN, "No EFLAGS thunk was found for \"%s\"!", op.c_str());
    }
}

#define DEL_STMT_RHS 0 // match by rhs of MOVE statement
#define DEL_STMT_LHS 1 // match by lhs of MOVE statement

#define DEL_STMT_GET(_arg_, _move_) ((_arg_) ? (_move_)->lhs : (_move_)->rhs)

int del_stmt(vector<Stmt *> *ir, int arg, bool del, vector<string> names)
{
    assert(ir);

    vector<Stmt *> rv;
    int ret = -1, len = 0;

    for (vector<Stmt *>::iterator i = ir->begin(); i != ir->end(); i++)
    {
        Stmt *stmt = (*i);
        rv.push_back(stmt);

        len++;

        if (stmt->stmt_type == MOVE)
        {
            Move *move = (Move *)stmt;

            if (DEL_STMT_GET(arg, move)->exp_type == TEMP)
            {
                Temp *temp = (Temp *)DEL_STMT_GET(arg, move);
                bool found = false;

                for (vector<string>::iterator n = names.begin(); n != names.end(); n++)
                {
                    string name = (*n);

                    if (temp->name == name)
                    {
                        found = true;
                        break;
                    }
                }

                if (found)
                {                    
                    if (del)
                    {
                        // remove and free the statement
                        Stmt::destroy(rv.back());
                        rv.pop_back();

                        len--;
                    }                    

                    // remember statement position
                    ret = len;
                }
            }
        }
    }

    ir->clear();
    ir->insert(ir->begin(), rv.begin(), rv.end());

    return ret;
}

void del_seg_selector_trash(bap_block_t *block)
{
    assert(block);

    vector<string> names;

    names.push_back("R_LDT");
    names.push_back("R_GDT");

    vector<Stmt *> rv;
    vector<Stmt *> *ir = block->bap_ir;

    bool use_seg_selector_found = false;
    
    // Look for junk code after the eliminated x86g_use_seg_selector VEX call
    for (int i = 0; i < ir->size(); i++)
    {
        Stmt *stmt = ir->at(i);
        rv.push_back(stmt);

        if (i == 0 || i >= ir->size() - 3)
        {
            continue;
        }

        Stmt *prev = ir->at(i - 1);

        // Check for x86g_use_seg_selector comment statement
        if ((stmt->stmt_type == MOVE && ((Move *)stmt)->lhs->exp_type == TEMP) &&
            (prev->stmt_type == COMMENT && ((Comment *)prev)->comment == "x86g_use_seg_selector"))
        {         
            Temp *seg_addr = (Temp *)((Move *)stmt)->lhs;

            Stmt *next_1 = ir->at(i + 1);
            Stmt *next_2 = ir->at(i + 2);
            Stmt *next_3 = ir->at(i + 3);

            /*
                Find assert() statement that checks virtual address after the
                x86g_use_seg_selector VEX call, this code was generated by libVEX 
                and translated by libasmir.

                VEX IR example:

                    t6 = GET:I16(294)
                    t5 = 16Uto32(t6)
                    t1 = GET:I32(300)
                    t2 = GET:I32(304)
                    t7 = GET:I32(8)
                    t8 = x86g_use_seg_selector{0xa6ab8540}(t1,t2,t5,t7):I64
                    t10 = 64HIto32(t8)
                    t9 = CmpNE32(t10,0x0:I32)
                    if (t9) { PUT(68) = 0x1337:I32; exit-MapFail }
                    ...

                Generated BAP IR:

                    T_16t6:reg16_t = R_FS:reg16_t;
                    T_32t5:reg32_t = cast(T_16t6:reg16_t)U:reg32_t;
                    T_32t1:reg32_t = R_LDT:reg32_t;
                    T_32t2:reg32_t = R_GDT:reg32_t;
                    T_32t7:reg32_t = R_EAX:reg32_t;
                    // x86g_use_seg_selector
                    T_64t8:reg64_t = cast((R_FS_BASE:reg32_t+T_32t7:reg32_t))U:reg64_t;
                    T_32t10:reg32_t = cast(T_64t8:reg64_t)H:reg32_t;
                    T_1t9:reg1_t = (T_32t10:reg32_t<>0:reg32_t);
                    assert((!T_1t9:reg1_t));
                    ...
            */
            if ((next_3->stmt_type == ASSERT && ((Assert *)next_3)->cond->exp_type == UNOP) && 

                (next_2->stmt_type == MOVE && ((Move *)next_2)->rhs->exp_type == BINOP &&
                                              ((Move *)next_2)->lhs->exp_type == TEMP) &&

                (next_1->stmt_type == MOVE && ((Move *)next_1)->rhs->exp_type == CAST && 
                                              ((Move *)next_1)->lhs->exp_type == TEMP))
            {
                // Get 1-st statement arguments
                Cast *cast = (Cast *)((Move *)next_1)->rhs;                
                Temp *temp_1 = (Temp *)((Move *)next_1)->lhs;

                // Get 2-nd statement arguments
                BinOp *binop = (BinOp *)((Move *)next_2)->rhs;
                Temp *temp_2 = (Temp *)((Move *)next_2)->lhs;

                // Get assert argument
                UnOp *unop = (UnOp *)((Assert *)next_3)->cond;

                /*
                    Here goes some argument checks to be sure that we will 
                    not break a dataflow graph of translated instruction
                    during junk code deletion.
                */
                Exp *arg_1_1 = (Exp *)cast->exp;
                Exp *arg_2_1 = (Exp *)binop->lhs;
                Exp *arg_2_2 = (Exp *)binop->rhs;
                Exp *arg_3_1 = (Exp *)unop->exp;

                // Check for proper expressions
                if ((cast->cast_type == CAST_HIGH && arg_1_1->exp_type == TEMP) &&
                    (unop->unop_type == NOT && arg_3_1->exp_type == TEMP) && 
                    (binop->binop_type == NEQ && arg_2_1->exp_type == TEMP && 
                                                 arg_2_2->exp_type == CONSTANT))
                {
                    // Check for proper expression arguments
                    if (((Temp *)arg_1_1)->name == seg_addr->name &&
                        ((Temp *)arg_2_1)->name == temp_1->name && ((Constant *)arg_2_2)->val == 0 &&
                        ((Temp *)arg_3_1)->name == temp_2->name)                        
                    {
                        // Skip next 3 instructions, it's safe to delete them
                        i += 3;
                        use_seg_selector_found = true;
                    }
                }
            }                        
        }        
    }

    ir->clear();
    ir->insert(ir->begin(), rv.begin(), rv.end());

    if (!use_seg_selector_found)
    {
        // Segment selector access was not found
        return;
    }    

    // eliminate LDT/GDT args of x86g_use_seg_selector
    del_stmt(block->bap_ir, DEL_STMT_RHS, true, names);    

    rv.clear();

    // Eliminate junk REG_32 <-> REG_64 casts
    for (int i = 0; i < ir->size(); i++)
    {
        Stmt *stmt = ir->at(i);
        rv.push_back(stmt);

        if (i == 0 || i >= ir->size() - 1)
        {
            continue;
        }

        Stmt *prev = ir->at(i - 1);
        Stmt *next = ir->at(i + 1);        

        // Check for x86g_use_seg_selector comment statement
        if ((prev->stmt_type == COMMENT && ((Comment *)prev)->comment == "x86g_use_seg_selector") &&
            
            (next->stmt_type == MOVE && ((Move *)next)->lhs->exp_type == TEMP
                                     && ((Move *)next)->rhs->exp_type == CAST) &&

            (stmt->stmt_type == MOVE && ((Move *)stmt)->lhs->exp_type == TEMP
                                     && ((Move *)stmt)->rhs->exp_type == CAST))
        {   
            Temp *temp_1 = (Temp *)((Move *)stmt)->lhs;      
            Cast *cast_1 = (Cast *)((Move *)stmt)->rhs;
            Cast *cast_2 = (Cast *)((Move *)next)->rhs;

            // Check for 32 -> 64 -> 32 cast
            if (cast_1->typ == REG_64 && cast_1->cast_type == CAST_UNSIGNED &&
                cast_2->typ == REG_32 && cast_2->cast_type == CAST_LOW && 
                cast_2->exp->exp_type == TEMP)
            {
                Temp *temp_2 = (Temp *)cast_2->exp;

                if (temp_1->name == temp_2->name)
                {
                    temp_1->typ = REG_32;
                    temp_2->typ = REG_32;

                    // Remove both of the cast operations
                    ((Move *)stmt)->rhs = ecl(cast_1->exp);
                    ((Move *)next)->rhs = ecl(cast_2->exp);                    
                }                      
            }
        }
    }

    ir->clear();
    ir->insert(ir->begin(), rv.begin(), rv.end());
}

void del_internal(bap_block_t *block)
{
    assert(block);

    vector<string> names;

    vector<Stmt *> rv;
    vector<Stmt *> *ir = block->bap_ir;
    
    // Look for 'Internal' statements
    for (int i = 0; i < ir->size(); i++)
    {
        Stmt *stmt = ir->at(i);       

        if (stmt->stmt_type == INTERNAL)
        {  
            Internal *intr = (Internal *)stmt;

            if (intr->type == INTERNAL_VEX_FN_ARG_LIST)
            {
                // get pointer to arguments list structure
                internal_vex_fn_arg_list *arg_list = (internal_vex_fn_arg_list *)intr->data;
                assert(arg_list);

                for (int n = 0; n < arg_list->count; n++)
                {
                    string name = translate_tmp_name(NULL, 
                        arg_list->args[n].type, 
                        arg_list->args[n].temp
                    );
                    
                    names.push_back(name);
                }
            }            

            continue;
        }

        rv.push_back(stmt);
    }

    ir->clear();
    ir->insert(ir->begin(), rv.begin(), rv.end());

    // delete VEX function argument statements
    del_stmt(block->bap_ir, DEL_STMT_LHS, true, names);    
}

void del_get_thunk(bap_block_t *block)
{
    assert(block);

    vector<string> names;

    names.push_back("R_CC_OP");
    names.push_back("R_CC_DEP1");
    names.push_back("R_CC_DEP2");
    names.push_back("R_CC_NDEP");

    // delete get thunks
    del_stmt(block->bap_ir, DEL_STMT_RHS, true, names);    
}

bool del_put_thunk_mux0x(bap_block_t *block, int n, vector<string> &names)
{
    assert(block);

    vector<Stmt *> *ir = block->bap_ir;

    if (match_mux0x(ir, n - MUX_SUB, NULL, NULL, NULL, NULL) >= 0)
    {

#ifndef MUX_AS_CJMP

        // check for temp + temp + move + move + move
        if (ir->at(n - MUX_SUB + 0)->stmt_type == VARDECL &&
            ir->at(n - MUX_SUB + 1)->stmt_type == VARDECL)
        {
            VarDecl *var1 = (VarDecl *)ir->at(n - MUX_SUB + 0);
            VarDecl *var2 = (VarDecl *)ir->at(n - MUX_SUB + 1);

            names.push_back(var1->name);
            names.push_back(var2->name);

            if (ir->at(n - 1)->stmt_type == MOVE)
            {
                Move *move = (Move *)ir->at(n - 1);

                if (move->rhs->exp_type == TEMP && move->lhs->exp_type == TEMP)
                {
                    Temp *rhs = (Temp *)move->rhs;
                    Temp *lhs = (Temp *)move->lhs;

                    if (rhs->name == var1->name)
                    {
                        names.push_back(lhs->name);
                    }
                }
            }
        }        

#else

#error MUX_AS_CJMP is not supported

#endif

        return true;
    }

    return false;
}

int del_put_thunk(bap_block_t *block, int op, int dep1, int dep2, int ndep, int mux0x)
{
    assert(block);

    vector<string> names;
    vector<Stmt *> *ir = block->bap_ir;

    // delete mux0x used for flag thunks
    del_put_thunk_mux0x(block, op, names);
    del_put_thunk_mux0x(block, dep1, names);
    del_put_thunk_mux0x(block, dep2, names);
    del_put_thunk_mux0x(block, ndep, names);    

    names.push_back("R_CC_OP");
    names.push_back("R_CC_DEP1");
    names.push_back("R_CC_DEP2");
    names.push_back("R_CC_NDEP");

    // delete get thunks
    return del_stmt(block->bap_ir, DEL_STMT_LHS, true, names);
}

void get_put_thunk(bap_block_t *block, int *op, int *dep1, int *dep2, int *ndep, int *mux0x)
{
    assert(block);

    unsigned int i;
    vector<Stmt *> *ir = block->bap_ir;

    *op = -1;

    for (i = 0; i < ir->size(); i++)
    {
        Stmt *stmt = ir->at(i);

        if (stmt->stmt_type != MOVE)
        {
            continue;
        }

        Move *move = (Move *)stmt;

        if (move->lhs->exp_type != TEMP)
        {
            continue;
        }

        Temp *temp = (Temp *)move->lhs;

        if (temp->name.find("R_CC_OP") != string::npos)
        {
            *op = i;            

            if (match_mux0x(ir, (i - MUX_SUB), NULL, NULL, NULL, NULL) >= 0)
            {
                *mux0x = (i - MUX_SUB);
            }
        }
        else if (temp->name.find("R_CC_DEP1") != string::npos)
        {
            *dep1 = i;
        }
        else if (temp->name.find("R_CC_DEP2") != string::npos)
        {
            *dep2 = i;
        }
        else if (temp->name.find("R_CC_NDEP") != string::npos)
        {
            *ndep = i;
        }
    }
}

void del_get_itstate(bap_block_t *block)
{
    assert(block);

    vector<string> names;

    names.push_back("R_ITSTATE");

    // delete ITSTATE operations
    del_stmt(block->bap_ir, DEL_STMT_RHS, true, names);    
}

void del_put_itstate(bap_block_t *block, int itstate)
{
    assert(block);
    
    vector<string> names;

    names.push_back("R_ITSTATE");

    // delete ITSTATE operations
    del_stmt(block->bap_ir, DEL_STMT_LHS, true, names);    
}

void get_put_itstate(bap_block_t *block, int *itstate)
{
    assert(block);

    unsigned int i;
    vector<Stmt *> *ir = block->bap_ir;

    *itstate = -1;

    // Look for occurrence of ITSTATE assignment
    for (i = 0; i < ir->size(); i++)
    {
        Stmt *stmt = ir->at(i);

        if (stmt->stmt_type != MOVE)
        {
            continue;
        }

        Move *move = (Move *)stmt;

        if (move->lhs->exp_type != TEMP)
        {
            continue;
        }

        Temp *temp = (Temp *)move->lhs;        

        if (temp->name.find("R_ITSTATE") != string::npos)
        {
            *itstate = i;
        }
    }
}

//---------------------------------------------------------------------
// Helper wrappers around arch specific functions
//---------------------------------------------------------------------

vector<VarDecl *> get_reg_decls(bap_context_t *context)
{
    vector<VarDecl *> ret;

    switch (context->guest)
    {
    case VexArchX86:

        return i386_get_reg_decls();

    case VexArchARM:
    
        return arm_get_reg_decls();
    
    default:
    
        panic("irtoir.cpp: translate_get: unsupported arch");
    }

    return ret;
}

Exp *translate_get(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout)
{
    assert(expr);
    assert(irbb);
    assert(irout);

    switch (context->guest)
    {
    case VexArchX86:
    
        return i386_translate_get(context, expr, irbb, irout);
    
    case VexArchARM:
    
        return arm_translate_get(context, expr, irbb, irout);
    
    default:
    
        panic("irtoir.cpp: translate_get: unsupported arch");
    }

    return NULL;
}

Stmt *translate_put(bap_context_t *context, IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout)
{
    switch (context->guest)
    {
    case VexArchX86:
    
        return i386_translate_put(context, stmt, irbb, irout);
    
    case VexArchARM:
    
        return arm_translate_put(context, stmt, irbb, irout);
    
    default:
        
        panic("translate_put");
    }

    return NULL;
}

Exp *translate_ccall(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout)
{
    assert(expr);
    assert(irbb);
    assert(irout);

    switch (context->guest)
    {
    case VexArchX86:
    
        return i386_translate_ccall(context, expr, irbb, irout);
    
    case VexArchARM:
    
        return arm_translate_ccall(context, expr, irbb, irout);
    
    default:

        panic("translate_ccall");
    }

    return NULL;
}

void modify_flags(bap_context_t *context, bap_block_t *block)
{
    assert(block);

    switch (context->guest)
    {
    case VexArchX86:
    
        i386_modify_flags(context, block);
        break;
    
    case VexArchARM:
    
        arm_modify_flags(context, block);
        break;
    
    default:

        panic("modify_flags");
    }
}

Temp *mk_reg(string name, reg_t width)
{
    return new Temp(width, "R_" + name);
}

// Note:
//  VEX uses IRType to specify both the width(in terms of bits) and
//  type(signed/unsigned/float) of an expression. To translate an
//  IRType, we use get_reg_width() and get_reg_type() to extract
//  these two aspects of the expression type from the IRType.
reg_t IRType_to_reg_type(IRType type)
{
    reg_t t;

    switch (type)
    {
    case Ity_I1:

        t = REG_1;
        break;
    
    case Ity_I8:

        t = REG_8;
        break;
    
    case Ity_I16:

        t = REG_16;
        break;
    
    case Ity_I32:

        t = REG_32;
        break;
    
    case Ity_I64:

        t = REG_64;
        break;
    
    case Ity_F32:

        log_write(LOG_WARN, "Float32 register encountered");
        t = REG_32;

        break;
    
    case Ity_F64:

        log_write(LOG_WARN, "Float64 register encountered");
        t = REG_64;

        break;
    
    default:

        panic("Unknown IRType");
    }

    return t;
}

reg_t regt_of_irexpr(IRSB *irbb, IRExpr *e)
{
    return IRType_to_reg_type(typeOfIRExpr(irbb->tyenv, e));
}

Temp *mk_temp(string name, IRType ty)
{
    reg_t typ = IRType_to_reg_type(ty);
    Temp *ret = new Temp(typ, name);
    return ret;
}

Temp *mk_temp(reg_t type, vector<Stmt *> *stmts)
{
    static int temp_counter = 0;
    Temp *ret =  new Temp(type, "T_" + int_to_str(temp_counter++));
    stmts->push_back(new VarDecl(ret));
    return ret;
}

Temp *mk_temp(IRType ty, vector<Stmt *> *stmts)
{
    reg_t typ = IRType_to_reg_type(ty);
    return mk_temp(typ, stmts);
}

//----------------------------------------------------------------------
// Takes a destination address and makes a Label out of it.
// Note that this function and mk_dest_name must produce the same
// string for the same given address!
//----------------------------------------------------------------------
Label *mk_dest_label(Addr64 dest)
{
    return new Label("pc_0x" + int_to_hex(dest));
}

//----------------------------------------------------------------------
// Takes a destination address and makes a Name out of it.
// Note that this function and mk_dest_label must produce the same
// string for the same given address!
//----------------------------------------------------------------------
Name *mk_dest_name(Addr64 dest)
{
    return new Name("pc_0x" + int_to_hex(dest));
}

//======================================================================
//
// Actual translation functions
//
//======================================================================

//----------------------------------------------------------------------
// arg1 and arg2 are both suppose to be 32 bit expressions.
// This function returns a 64 bit expression with arg1 occupying the
// high 32 bits and arg2 occupying the low 32 bits.
//----------------------------------------------------------------------
Exp *translate_32HLto64(bap_context_t *context, Exp *arg1, Exp *arg2)
{
    assert(arg1);
    assert(arg2);

    Exp *high = new Cast(arg1, REG_64, CAST_UNSIGNED);
    Exp *low = new Cast(arg2, REG_64, CAST_UNSIGNED);

    high = new BinOp(LSHIFT, high, ex_const(REG_64, 32));

    return new BinOp(BITOR, high, low);
}

Exp *translate_64HLto64(bap_context_t *context, Exp *high, Exp *low)
{
    assert(high);
    assert(low);

    high = new BinOp(LSHIFT, high, ex_const(REG_64, 32));
    high = new BinOp(BITAND, high, ex_const(REG_64, 0xffffffff00000000LL));
    low = new BinOp(BITAND, low, ex_const(REG_64, 0xffffffffL));

    return new BinOp(BITOR, high, low);
}

Exp *translate_DivModU64to32(bap_context_t *context, Exp *arg1, Exp *arg2)
{
    assert(arg1);
    assert(arg2);
    arg2 = new Cast(arg2, REG_64, CAST_UNSIGNED);
    Exp *div = new BinOp(DIVIDE, arg1, arg2);
    Exp *mod = new BinOp(MOD, ecl(arg1), ecl(arg2));

    return translate_64HLto64(context, mod, div);
}

Exp *translate_DivModS64to32(bap_context_t *context, Exp *arg1, Exp *arg2)
{
    assert(arg1);
    assert(arg2);
    arg2 = new Cast(arg2, REG_64, CAST_SIGNED);
    Exp *div = new BinOp(SDIVIDE, arg1, arg2);
    Exp *mod = new BinOp(SMOD, ecl(arg1), ecl(arg2));

    return translate_64HLto64(context, mod, div);
}

Exp *translate_MullS8(bap_context_t *context, Exp *arg1, Exp *arg2)
{
    assert(arg1);
    assert(arg2);

    Exp *wide1 = new Cast(arg1, REG_16, CAST_SIGNED);
    Exp *wide2 = new Cast(arg2, REG_16, CAST_SIGNED);

    return new BinOp(TIMES, wide1, wide2);
}

Exp *translate_MullU32(bap_context_t *context, Exp *arg1, Exp *arg2)
{
    assert(arg1);
    assert(arg2);

    Exp *wide1 = new Cast(arg1, REG_64, CAST_UNSIGNED);
    Exp *wide2 = new Cast(arg2, REG_64, CAST_UNSIGNED);

    return new BinOp(TIMES, wide1, wide2);
}

Exp *translate_MullS32(bap_context_t *context, Exp *arg1, Exp *arg2)
{
    assert(arg1);
    assert(arg2);

    Exp *wide1 = new Cast(arg1, REG_64, CAST_SIGNED);
    Exp *wide2 = new Cast(arg2, REG_64, CAST_SIGNED);

    return new BinOp(TIMES, wide1, wide2);
}

Exp *translate_Clz32(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout)
{
    assert(expr);
    assert(irbb);
    assert(irout);

    Exp *arg = translate_expr(context, expr->Iex.Unop.arg, irbb, irout);

    Temp *counter = mk_temp(Ity_I32, irout);
    Temp *temp = mk_temp(typeOfIRExpr(irbb->tyenv, expr->Iex.Unop.arg), irout);

    Label *loop = mk_label();
    Label *label0 = mk_label();
    Label *label1 = mk_label();

    Exp *cond = new BinOp(NEQ, temp, ex_const(0));

    irout->push_back(new Move(new Temp(*counter), ex_const(32)));
    irout->push_back(new Move(new Temp(*temp), arg));
    irout->push_back(loop);
    irout->push_back(new CJmp(cond, new Name(label0->label), new Name(label1->label)));
    irout->push_back(label0);
    irout->push_back(new Move(new Temp(*temp), new BinOp(RSHIFT, new Temp(*temp), ex_const(1))));
    irout->push_back(new Move(new Temp(*counter), new BinOp(MINUS, new Temp(*counter), ex_const(1))));
    irout->push_back(new Jmp(new Name(loop->label)));
    irout->push_back(label1);

    return counter;
}

Exp *translate_Ctz32(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout)
{
    assert(expr);
    assert(irbb);
    assert(irout);

    Exp *arg = translate_expr(context, expr->Iex.Unop.arg, irbb, irout);

    Temp *counter = mk_temp(Ity_I32, irout);
    Temp *temp = mk_temp(typeOfIRExpr(irbb->tyenv, expr->Iex.Unop.arg), irout);

    Label *loop = mk_label();
    Label *label0 = mk_label();
    Label *label1 = mk_label();

    Exp *cond = new BinOp(NEQ, temp, ex_const(0));

    irout->push_back(new Move(new Temp(*counter), ex_const(32)));
    irout->push_back(new Move(new Temp(*temp), arg));
    irout->push_back(loop);
    irout->push_back(new CJmp(cond, new Name(label0->label), new Name(label1->label)));
    irout->push_back(label0);
    irout->push_back(new Move(new Temp(*temp), new BinOp(LSHIFT, new Temp(*temp), ex_const(1))));
    irout->push_back(new Move(new Temp(*counter), new BinOp(MINUS, new Temp(*counter), ex_const(1))));
    irout->push_back(new Jmp(new Name(loop->label)));
    irout->push_back(label1);

    return counter;
}

Exp *translate_CmpF64(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout)
{
    assert(expr);
    assert(irbb);
    assert(irout);

    Exp *arg1 = translate_expr(context, expr->Iex.Binop.arg1, irbb, irout);
    Exp *arg2 = translate_expr(context, expr->Iex.Binop.arg2, irbb, irout);

    Exp *condEQ = new BinOp(EQ, arg1, arg2);
    Exp *condGT = new BinOp(GT, ecl(arg1), ecl(arg2));
    Exp *condLT = new BinOp(LT, ecl(arg1), ecl(arg2));

    Label *labelEQ = mk_label();
    Label *labelGT = mk_label();
    Label *labelLT = mk_label();
    Label *labelUN = mk_label();
    Label *labelNext0 = mk_label();
    Label *labelNext1 = mk_label();
    Label *done = mk_label();

    Temp *temp = mk_temp(Ity_I32, irout);

    irout->push_back(new CJmp(condEQ, new Name(labelEQ->label), new Name(labelNext0->label)));
    irout->push_back(labelNext0);
    irout->push_back(new CJmp(condGT, new Name(labelGT->label), new Name(labelNext1->label)));
    irout->push_back(labelNext1);
    irout->push_back(new CJmp(condLT, new Name(labelLT->label), new Name(labelUN->label)));
    irout->push_back(labelUN);
    irout->push_back(new Move(new Temp(*temp), ex_const(0x45)));
    irout->push_back(new Jmp(new Name(done->label)));
    irout->push_back(labelEQ);
    irout->push_back(new Move(new Temp(*temp), ex_const(0x40)));
    irout->push_back(new Jmp(new Name(done->label)));
    irout->push_back(labelGT);
    irout->push_back(new Move(new Temp(*temp), ex_const(0x00)));
    irout->push_back(new Jmp(new Name(done->label)));
    irout->push_back(labelLT);
    irout->push_back(new Move(new Temp(*temp), ex_const(0x01)));
    irout->push_back(done);

    return temp;
}

Exp *translate_const(bap_context_t *context, IRExpr *expr)
{
    assert(expr);

    IRConst *co = expr->Iex.Const.con;

    const_val_t value;
    reg_t width;

    switch (co->tag)
    {
    // Your typical unsigned ints
    case Ico_U1:
    
        width = REG_1;
        value = co->Ico.U1;
        break;
    
    case Ico_U8:
    
        width = REG_8;
        value = co->Ico.U8;
        break;
    
    case Ico_U16:
    
        width = REG_16;
        value = co->Ico.U16;
        break;
    
    case Ico_U32:
    
        width = REG_32;
        value = co->Ico.U32;
        break;
    
    case Ico_U64:
    
        width = REG_64;
        value = co->Ico.U64;
        break;

    //
    // Not sure what the diff here is, VEX comments say F64 is IEEE754 floating
    // and F64i is 64 bit unsigned int interpreted literally at IEEE754 double.
    //
    case Ico_F64: //  width = I64;   value.floatval = co->Ico.F64;   kind = REG_FLOAT; break;
    case Ico_F64i: // width = I64;   value.floatval = co->Ico.F64i;  kind = REG_FLOAT; break;
    
        width = REG_64;
        value = co->Ico.F64i;
        break;

    // Dunno what this is for, so leave it unrecognized
    case Ico_V128:
    
        panic("Unsupported constant type (Ico_V128)");
    
    default:
    
        panic("Unrecognized constant type");
    }

    Constant *result = new Constant(width, value);

    return result;
}

Exp *translate_simple_unop(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout)
{
    Exp *arg = translate_expr(context, expr->Iex.Unop.arg, irbb, irout);

    switch (expr->Iex.Unop.op)
    {
    case Iop_Not8:
    case Iop_Not16:
    case Iop_Not32:
    case Iop_Not64:

        return new UnOp(NOT, arg);

#if VEX_VERSION < 1770
    
    case Iop_Neg8:
    case Iop_Neg16:
    case Iop_Neg32:
    case Iop_Neg64:
    
        return new UnOp(NEG, arg);
#endif
        
    case Iop_NegF64:

        return new Unknown("NegF64", REG_64);

    case Iop_Not1:

        return new UnOp(NOT, arg);

    case Iop_8Uto16:

        return new Cast(arg, REG_16, CAST_UNSIGNED);
    
    case Iop_8Sto16:
    
        return new Cast(arg, REG_16, CAST_SIGNED);
    
    case Iop_8Uto32:
    
        return new Cast(arg, REG_32, CAST_UNSIGNED);
    
    case Iop_8Sto32:
    
        return new Cast(arg, REG_32, CAST_SIGNED);
    
    case Iop_8Uto64:
    
        return new Cast(arg, REG_64, CAST_UNSIGNED);
    
    case Iop_8Sto64:
    
        return new Cast(arg, REG_64, CAST_SIGNED);
    
    case Iop_16Uto32:
    
        return new Cast(arg, REG_32, CAST_UNSIGNED);
    
    case Iop_16Sto32:
    
        return new Cast(arg, REG_32, CAST_SIGNED);
    
    case Iop_16Uto64:
    
        return new Cast(arg, REG_64, CAST_UNSIGNED);
    
    case Iop_16Sto64:
    
        return new Cast(arg, REG_64, CAST_SIGNED);
    
    case Iop_16to8:
    
        return new Cast(arg, REG_8,  CAST_LOW);
    
    case Iop_16HIto8:
    
        return new Cast(arg, REG_8,  CAST_HIGH);
    
    case Iop_32Sto64:
    
        return new Cast(arg, REG_64, CAST_SIGNED);
    
    case Iop_32Uto64:
    
        return new Cast(arg, REG_64, CAST_UNSIGNED);
    
    case Iop_32to8:
    
        return new Cast(arg, REG_8,  CAST_LOW);
    
    case Iop_32to16:
    
        return new Cast(arg, REG_16, CAST_LOW);
    
    case Iop_64to8:
    
        return new Cast(arg, REG_8,  CAST_LOW);
    
    case Iop_64to16:
    
        return new Cast(arg, REG_16, CAST_LOW);
    
    case Iop_64to32:
    
        return new Cast(arg, REG_32, CAST_LOW);
    
    case Iop_64HIto32:
    
        return new Cast(arg, REG_32, CAST_HIGH);
    
    case Iop_32to1:
    
        return new Cast(arg, REG_1,  CAST_LOW);
    
    case Iop_64to1:
    
        return new Cast(arg, REG_1,  CAST_LOW);
    
    case Iop_1Uto8:
    
        return new Cast(arg, REG_8,  CAST_UNSIGNED);
    
    case Iop_1Uto32:
    
        return new Cast(arg, REG_32, CAST_UNSIGNED);
    
    case Iop_1Uto64:
    
        return new Cast(arg, REG_64, CAST_UNSIGNED);
    
    case Iop_1Sto8:
    
        return new Cast(arg, REG_8,  CAST_SIGNED);
    
    case Iop_1Sto16:
    
        return new Cast(arg, REG_16, CAST_SIGNED);
    
    case Iop_1Sto32:
    
        return new Cast(arg, REG_32, CAST_SIGNED);
    
    case Iop_1Sto64:
    
        return new Cast(arg, REG_64, CAST_SIGNED);
/*
    case Iop_F32toF64:  

        return new Cast(arg, REG_64, CAST_FLOAT);
    
    case Iop_I32toF64:  

        return new Cast(arg, REG_64, CAST_FLOAT);

    case Iop_ReinterpI64asF64:  

        return new Cast(arg, REG_64, CAST_RFLOAT);
    
    case Iop_ReinterpF64asI64:  

        return new Cast(arg, REG_64, CAST_RINTEGER);

    case Iop_F32toF64:
    case Iop_I32toF64:
    case Iop_ReinterpI64asF64:
    case Iop_ReinterpF64asI64:
    
        return new Unknown("floatcast", REG_64); 
*/
    default:

        break;
    }

    return NULL;
}

Exp *translate_unop(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout)
{
    assert(irbb);
    assert(expr);
    assert(irout);

    Exp *result;

    if ((result = translate_simple_unop(context, expr, irbb, irout)) != NULL)
    {
        return result;
    }

    switch (expr->Iex.Unop.op)
    {
    case Iop_Clz32:
        
        return translate_Clz32(context, expr, irbb, irout);

    case Iop_Ctz32:
        
        return translate_Ctz32(context, expr, irbb, irout);

    case Iop_AbsF64:
        
        return new Unknown("Floating point op", REG_64);

    default:
        
        panic("Unrecognized unary op");
    }

    return NULL;
}

Exp *translate_simple_binop(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout)
{
    Exp *arg1 = translate_expr(context, expr->Iex.Binop.arg1, irbb, irout);
    Exp *arg2 = translate_expr(context, expr->Iex.Binop.arg2, irbb, irout);

    switch (expr->Iex.Binop.op)
    {
    case Iop_Add8:
    case Iop_Add16:
    case Iop_Add32:
    case Iop_Add64:

        return new BinOp(PLUS, arg1, arg2);
    
    case Iop_Sub8:
    case Iop_Sub16:
    case Iop_Sub32:
    case Iop_Sub64:
    
        return new BinOp(MINUS, arg1, arg2);
    
    case Iop_Mul8:
    case Iop_Mul16:
    case Iop_Mul32:
    case Iop_Mul64:
    
        return new BinOp(TIMES, arg1, arg2);
    
    case Iop_Or8:
    case Iop_Or16:
    case Iop_Or32:
    case Iop_Or64:
    
        return new BinOp(BITOR, arg1, arg2);
    
    case Iop_And8:
    case Iop_And16:
    case Iop_And32:
    case Iop_And64:
    
        return new BinOp(BITAND, arg1, arg2);
    
    case Iop_Xor8:
    case Iop_Xor16:
    case Iop_Xor32:
    case Iop_Xor64:
    
        return new BinOp(XOR, arg1, arg2);
    
    case Iop_Shl8:

        if (!context->count_opnd && !use_eflags_thunks)
        {
            context->count_opnd = arg2;
        }

        return new BinOp(LSHIFT, arg1, new Cast(arg2, REG_8, CAST_UNSIGNED));
    
    case Iop_Shl16:

        if (!context->count_opnd && !use_eflags_thunks)
        {
            context->count_opnd = arg2;
        }

        return new BinOp(LSHIFT, arg1, new Cast(arg2, REG_16, CAST_UNSIGNED));
    
    case Iop_Shl32:

        if (!context->count_opnd && !use_eflags_thunks)
        {
            context->count_opnd = arg2;
        }

        return new BinOp(LSHIFT, arg1, new Cast(arg2, REG_32, CAST_UNSIGNED));
    
    case Iop_Shl64:

        if (!context->count_opnd && !use_eflags_thunks)
        {
            context->count_opnd = arg2;
        }

        return new BinOp(LSHIFT, arg1, new Cast(arg2, REG_64, CAST_UNSIGNED));
    
    case Iop_Shr8:

        if (!context->count_opnd && !use_eflags_thunks)
        {
            context->count_opnd = arg2;
        }

        return new BinOp(RSHIFT, arg1, new Cast(arg2, REG_8, CAST_UNSIGNED));
    
    case Iop_Shr16:

        if (!context->count_opnd && !use_eflags_thunks)
        {
            context->count_opnd = arg2;
        }

        return new BinOp(RSHIFT, arg1, new Cast(arg2, REG_16, CAST_UNSIGNED));
    
    case Iop_Shr32:

        if (!context->count_opnd && !use_eflags_thunks)
        {
            context->count_opnd = arg2;
        }

        return new BinOp(RSHIFT, arg1, new Cast(arg2, REG_32, CAST_UNSIGNED));
    
    case Iop_Shr64:

        if (!context->count_opnd && !use_eflags_thunks)
        {
            context->count_opnd = arg2;
        }

        return new BinOp(RSHIFT, arg1, new Cast(arg2, REG_64, CAST_UNSIGNED));
    
    case Iop_Sar8:

        if (!context->count_opnd && !use_eflags_thunks)
        {
            context->count_opnd = arg2;
        }

        return new BinOp(ARSHIFT, arg1, new Cast(arg2, REG_8, CAST_UNSIGNED));
    
    case Iop_Sar16:

        if (!context->count_opnd && !use_eflags_thunks)
        {
            context->count_opnd = arg2;
        }

        return new BinOp(ARSHIFT, arg1, new Cast(arg2, REG_16, CAST_UNSIGNED));
    
    case Iop_Sar32:

        if (!context->count_opnd && !use_eflags_thunks)
        {
            context->count_opnd = arg2;
        }

        return new BinOp(ARSHIFT, arg1, new Cast(arg2, REG_32, CAST_UNSIGNED));
    
    case Iop_Sar64:

        if (!context->count_opnd && !use_eflags_thunks)
        {
            context->count_opnd = arg2;
        }

        return new BinOp(ARSHIFT, arg1, new Cast(arg2, REG_64, CAST_UNSIGNED));
    
    case Iop_CmpEQ8:
    case Iop_CmpEQ16:
    case Iop_CmpEQ32:
    case Iop_CmpEQ64:

    case Iop_CasCmpEQ8:
    case Iop_CasCmpEQ16:
    case Iop_CasCmpEQ32:
    case Iop_CasCmpEQ64:
    
        return new BinOp(EQ, arg1, arg2);
    
    case Iop_CmpNE8:
    case Iop_CmpNE16:
    case Iop_CmpNE32:
    case Iop_CmpNE64:

    case Iop_CasCmpNE8:
    case Iop_CasCmpNE16:
    case Iop_CasCmpNE32:
    case Iop_CasCmpNE64:
    
        return new BinOp(NEQ, arg1, arg2);

    case Iop_CmpLT32U:
    
        return new BinOp(LT, arg1, arg2);

    case Iop_32HLto64:
    
        return translate_32HLto64(context, arg1, arg2);
    
    case Iop_MullS8:
    
        return translate_MullS8(context, arg1, arg2);
    
    case Iop_MullU32:
    
        return translate_MullU32(context, arg1, arg2);
    
    case Iop_MullS32:
    
        return translate_MullS32(context, arg1, arg2);
    
    case Iop_DivModU64to32:
    
        return translate_DivModU64to32(context, arg1, arg2);
    
    case Iop_DivModS64to32:
    
        return translate_DivModS64to32(context, arg1, arg2);

    case Iop_DivU32:
    
        return new BinOp(DIVIDE, arg1, arg2);
    
    case Iop_DivS32:
    
        return new BinOp(SDIVIDE, arg1, arg2);
    
    case Iop_DivU64:
    
        return new BinOp(DIVIDE, arg1, arg2);
    
    case Iop_DivS64:
    
        return new BinOp(SDIVIDE, arg1, arg2);
    
    default:
    
        break;
    }

    // TODO: free args

    return NULL;
}

Exp *translate_binop(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout)
{
    assert(irbb);
    assert(expr);
    assert(irout);

    Exp *result;

    if ((result = translate_simple_binop(context, expr, irbb, irout)) != NULL)
    {
        return result;
    }

    switch (expr->Iex.Binop.op)
    {
    case Iop_CmpF32:
    case Iop_CmpF64:
    case Iop_CmpF128:
/*
    // arg1 in this case, specifies rounding mode, and so is ignored
    case Iop_I64toF64:
    case Iop_F64toI64:
    case Iop_F64toF32:
    case Iop_F64toI32:
    case Iop_F64toI16:
    case Iop_RoundF64toInt:
    case Iop_2xm1F64:
*/
    case Iop_F64toI16S: /* IRRoundingMode(I32) x F64 -> signed I16 */
    case Iop_F64toI32S: /* IRRoundingMode(I32) x F64 -> signed I32 */
    case Iop_F64toI64S: /* IRRoundingMode(I32) x F64 -> signed I64 */
    case Iop_F64toI64U: /* IRRoundingMode(I32) x F64 -> unsigned I64 */

    case Iop_F64toI32U: /* IRRoundingMode(I32) x F64 -> unsigned I32 */

    case Iop_I32StoF64: /*                       signed I32 -> F64 */
    case Iop_I64StoF64: /* IRRoundingMode(I32) x signed I64 -> F64 */
    case Iop_I64UtoF64: /* IRRoundingMode(I32) x unsigned I64 -> F64 */
    case Iop_I64UtoF32: /* IRRoundingMode(I32) x unsigned I64 -> F32 */

    case Iop_I32UtoF64: /*                       unsigned I32 -> F64 */

    case Iop_F32toI32S: /* IRRoundingMode(I32) x F32 -> signed I32 */
    case Iop_F32toI64S: /* IRRoundingMode(I32) x F32 -> signed I64 */

    case Iop_I32StoF32: /* IRRoundingMode(I32) x signed I32 -> F32 */
    case Iop_I64StoF32: /* IRRoundingMode(I32) x signed I64 -> F32 */

    /* Conversion between floating point formats */
    case Iop_F32toF64:  /*                       F32 -> F64 */
    case Iop_F64toF32:  /* IRRoundingMode(I32) x F64 -> F32 */

    /* Reinterpretation.  Take an F64 and produce an I64 with
       the same bit pattern, or vice versa. */
    case Iop_ReinterpF64asI64:
    case Iop_ReinterpI64asF64:
    case Iop_ReinterpF32asI32:
    case Iop_ReinterpI32asF32:

    /* Support for 128-bit floating point */
    case Iop_F64HLtoF128:/* (high half of F128,low half of F128) -> F128 */
    case Iop_F128HItoF64:/* F128 -> high half of F128 into a F64 register */
    case Iop_F128LOtoF64:/* F128 -> low  half of F128 into a F64 register */

    /* :: IRRoundingMode(I32) x F128 x F128 -> F128 */
    case Iop_AddF128:
    case Iop_SubF128:
    case Iop_MulF128:
    case Iop_DivF128:

    /* :: F128 -> F128 */
    case Iop_NegF128:
    case Iop_AbsF128:

    /* :: IRRoundingMode(I32) x F128 -> F128 */
    case Iop_SqrtF128:

    case Iop_I32StoF128: /*                signed I32  -> F128 */
    case Iop_I64StoF128: /*                signed I64  -> F128 */
    case Iop_F32toF128:  /*                       F32  -> F128 */
    case Iop_F64toF128:  /*                       F64  -> F128 */

    case Iop_F128toI32S: /* IRRoundingMode(I32) x F128 -> signed I32  */
    case Iop_F128toI64S: /* IRRoundingMode(I32) x F128 -> signed I64  */
    case Iop_F128toF64:  /* IRRoundingMode(I32) x F128 -> F64         */
    case Iop_F128toF32:  /* IRRoundingMode(I32) x F128 -> F32         */

        return new Unknown("Floating point binop", regt_of_irexpr(irbb, expr));
/*
    case Iop_CmpF64:            

        return translate_CmpF64(context, expr, irbb, irout);

    // arg1 in this case, specifies rounding mode, and so is ignored
    case Iop_I64toF64:

        arg2 = translate_expr(context, expr->Iex.Binop.arg2, irbb, irout);
        return new Cast( arg2, REG_64, CAST_FLOAT );

    case Iop_F64toI64:
        
        arg2 = translate_expr(context, expr->Iex.Binop.arg2, irbb, irout);
        return new Cast( arg2, REG_64, CAST_INTEGER );

    case Iop_F64toF32:

        arg2 = translate_expr(context, expr->Iex.Binop.arg2, irbb, irout);
        return new Cast( arg2, REG_32, CAST_FLOAT );

    case Iop_F64toI32:

        arg2 = translate_expr(context, expr->Iex.Binop.arg2, irbb, irout);
        return new Cast( arg2, REG_32, CAST_INTEGER );

    case Iop_F64toI16:

        arg2 = translate_expr(context, expr->Iex.Binop.arg2, irbb, irout);
        return new Cast( arg2, REG_16, CAST_INTEGER );

    case Iop_RoundF64toInt:
    case Iop_2xm1F64:

        return new Unknown("Floating point op");
*/
    default:

        panic("Unrecognized binary op");
    }

    return NULL;
}

Exp *translate_triop(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout)
{
    assert(irbb);
    assert(expr);
    assert(irout);

    //
    // Tri-ops tend to be analogs of binary arithmetic ops but for
    // floating point operands instead of plain integers. As such,
    // the first argument specifies the rounding mode, which we have
    // chosen to ignore for now. See Notes for detailed explanation.
    //
    Exp *arg2 = translate_expr(context, expr->Iex.Triop.details->arg2, irbb, irout);
    Exp *arg3 = translate_expr(context, expr->Iex.Triop.details->arg3, irbb, irout);

    switch (expr->Iex.Triop.details->op)
    {
    case Iop_AddF64:
    case Iop_SubF64:
    case Iop_MulF64:
    case Iop_DivF64:
/*
    case Iop_AddF64:    return new BinOp(PLUS, arg2, arg3);
    case Iop_SubF64:    return new BinOp(MINUS, arg2, arg3);
    case Iop_MulF64:    return new BinOp(TIMES, arg2, arg3);
    case Iop_DivF64:    return new BinOp(DIVIDE, arg2, arg3);
*/
    case Iop_Yl2xF64:
    case Iop_Yl2xp1F64:
    case Iop_ScaleF64:

        return new Unknown("Floating point triop", REG_64);

    default:

        panic("Unrecognized ternary op");
    }

    return NULL;
}

Exp *emit_mux0x(vector<Stmt *> *irout, reg_t type, Exp *cond, Exp *exp0, Exp *expX)
{
    assert(cond);
    assert(exp0);
    assert(expX);

    // Every instance of temps and labels should have their own object,
    // i.e. always new Label(label) instead of just label. So the labels
    // and temps created above are used only once after which they need
    // to be cloned for each subsequent use. This keeps the expression tree
    // a tree instead of a graph.

    size_t initialSize = irout->size();

    Temp *temp = mk_temp(type, irout);

#ifndef MUX_AS_CJMP

    // FIXME: modify match_mux0x to recognize this
    Exp *widened_cond;
    reg_t result_type;

    /*
     *
     * README
     *
     * IF YOU CHANGE THIS, MAKE SURE YOU CHANGE MATCH_MUX0X TOO.
     *
     * IF YOU DONT, THINGS WILL BREAK.
     *
     * THANK YOU.
     *
     */

    widened_cond = mk_temp(type, irout);
    irout->push_back(new Move(ecl(widened_cond),
                              new Cast(cond, type,
                                       CAST_SIGNED)));

    // tmp = x&c | y&~c
    irout->push_back(new Move(ecl(temp),
                              new BinOp(BITOR,
                                        new BinOp(BITAND,
                                                ecl(exp0),
                                                ecl(widened_cond)),
                                        new BinOp(BITAND,
                                                ecl(expX),
                                                new UnOp(NOT,
                                                        widened_cond)))));

#else // MUX_AS_CJMP

    Label *labelX = mk_label();
    Label *done = mk_label();

    // match_mux0x depends on the order/types of these statements
    // if changing them here, make sure to make the corresponding changes there
    irout->push_back(new Move(new Temp(*temp), exp0));
    irout->push_back(new CJmp(cond, new Name(done->label), new Name(labelX->label)));
    irout->push_back(labelX);
    irout->push_back(new Move(new Temp(*temp), expX));
    irout->push_back(done);

#endif

    /* Make sure that MUX_LENGTH is correct */
    assert((initialSize + MUX_LENGTH) == irout->size());

    return temp;
}

Exp *translate_ITE(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout)
{
    assert(expr);
    assert(irbb);
    assert(irout);

    IRExpr *cond = expr->Iex.ITE.cond;
    IRExpr *iftrue = expr->Iex.ITE.iftrue;
    IRExpr *iffalse = expr->Iex.ITE.iffalse;

    assert(cond);
    assert(iftrue);
    assert(iffalse);

    // It's assumed that both true and false expressions have the
    // same type so that when we create a temp to assign it to, we
    // can just use one type
    reg_t type = IRType_to_reg_type(typeOfIRExpr(irbb->tyenv, iftrue));
    reg_t cond_type = IRType_to_reg_type(typeOfIRExpr(irbb->tyenv, cond));

    Exp *condE = translate_expr(context, cond, irbb, irout);
    Exp *exp0 = translate_expr(context, iftrue, irbb, irout);
    Exp *expX = translate_expr(context, iffalse, irbb, irout);

    condE = _ex_eq(condE, ex_const(cond_type, 0));

    return emit_mux0x(irout, type, condE, exp0, expX);
}

Exp *translate_load(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout)
{
    assert(expr);
    assert(irbb);
    assert(irout);

    Exp *addr;
    Mem *mem;
    reg_t rtype;

    rtype = IRType_to_reg_type(expr->Iex.Load.ty);

    addr = translate_expr(context, expr->Iex.Load.addr, irbb, irout);
    mem = new Mem(addr, rtype);

    return mem;
}

string translate_tmp_name(bap_context_t *context, IRType type, IRTemp tmp)
{
    return "T_" + int_to_str(get_type_size(IRType_to_reg_type(type))) + "t" + int_to_str(tmp);
}

string translate_tmp_name(bap_context_t *context, IRSB *irbb, IRExpr *expr, IRTemp tmp)
{
    IRType type = translate_tmp_type(context, irbb, expr);

    return translate_tmp_name(context, type, tmp);
}

IRType translate_tmp_type(bap_context_t *context, IRSB *irbb, IRExpr *expr)
{    
    assert(irbb);
    assert(expr);

    return typeOfIRExpr(irbb->tyenv, expr);   
}

Exp *translate_tmp_expr(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout)
{
    assert(expr);
    assert(irbb);
    assert(irout);

    IRType type;
    string name;

    type = translate_tmp_type(context, irbb, expr);
    name = translate_tmp_name(context, irbb, expr, expr->Iex.RdTmp.tmp);

    return mk_temp(name, type);
}

//----------------------------------------------------------------------
// Translate a single expression
//----------------------------------------------------------------------
Exp *translate_expr(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout)
{
    assert(irout);

    Exp *result = NULL;
    string func;

    switch (expr->tag)
    {
    case Iex_Binder:

        result = new Unknown(sTag + "Binder", regt_of_irexpr(irbb, expr));
        break;
    
    case Iex_Get:
    
        result = translate_get(context, expr, irbb, irout);
        break;
    
    case Iex_GetI:
    
        result = new Unknown(uTag + "GetI", regt_of_irexpr(irbb, expr));    
        break;
    
    case Iex_RdTmp:
        result = translate_tmp_expr(context, expr, irbb, irout);
        break;
    
    case Iex_Triop:
    
        result = translate_triop(context, expr, irbb, irout);
        break;
    
    case Iex_Binop:
    
        result = translate_binop(context, expr, irbb, irout);
        break;
    
    case Iex_Unop:
    
        result = translate_unop(context, expr, irbb, irout);
        break;
    
    case Iex_Load:
    
        result = translate_load(context, expr, irbb, irout);
        break;
    
    case Iex_Const:
    
        result = translate_const(context, expr);
        break;
    
    case Iex_ITE:
    
        result = translate_ITE(context, expr, irbb, irout);
        break;
    
    case Iex_CCall:
    
        result = translate_ccall(context, expr, irbb, irout);
        break;
    
    default:
    
        panic("Unrecognized expression type");
    }

    return result;
}

Stmt *translate_tmp_stmt(bap_context_t *context, IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout)
{
    assert(stmt);
    assert(irout);

    IRType type;
    string name;
    Exp *data;    

    type = translate_tmp_type(context, irbb, stmt->Ist.WrTmp.data);
    name = translate_tmp_name(context, irbb, stmt->Ist.WrTmp.data, stmt->Ist.WrTmp.tmp);
    
    data = translate_expr(context, stmt->Ist.WrTmp.data, irbb, irout);    

    return new Move(mk_temp(name, type), data);
}

Stmt *translate_store(bap_context_t *context, IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout)
{
    assert(stmt);
    assert(irbb);
    assert(irout);

    Exp *dest;
    Exp *data;
    Mem *mem;
    IRType itype;
    reg_t rtype;

    dest = translate_expr(context, stmt->Ist.Store.addr, irbb, irout);
    itype = typeOfIRExpr(irbb->tyenv, stmt->Ist.Store.data);
    rtype = IRType_to_reg_type(itype);
    mem = new Mem(dest, rtype);
    data = translate_expr(context, stmt->Ist.Store.data, irbb, irout);

    return new Move(mem, data);
}

Stmt *translate_cas(bap_context_t *context, IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout)
{
    assert(stmt);
    assert(irbb);
    assert(irout);    

    IRType type;
    string name;
    Exp *dest;
    Exp *expd;
    Exp *data;
    Label *t;
    Label *f;    

    if (stmt->Ist.CAS.details->dataHi || stmt->Ist.CAS.details->expdHi)
    {
        panic("Unexpected CAS statement");
    }

    type = translate_tmp_type(context, irbb, stmt->Ist.CAS.details->dataLo);
    name = translate_tmp_name(context, type, stmt->Ist.CAS.details->oldLo);    

    dest = translate_expr(context, stmt->Ist.CAS.details->addr, irbb, irout);    

    irout->push_back(new Move(mk_temp(name, type), 
                              new Mem(dest, IRType_to_reg_type(type))));

    expd = translate_expr(context, stmt->Ist.CAS.details->expdLo, irbb, irout);    

    t = mk_label();
    f = mk_label();

    irout->push_back(new CJmp(new BinOp(NEQ, mk_temp(name, type), expd), 
                              new Name(f->label), 
                              new Name(t->label)));    

    irout->push_back(t);

    dest = translate_expr(context, stmt->Ist.CAS.details->addr, irbb, irout);    
    data = translate_expr(context, stmt->Ist.CAS.details->dataLo, irbb, irout);

    irout->push_back(new Move(new Mem(dest, IRType_to_reg_type(type)), 
                              data));    

    return f;
}

Stmt *translate_imark(bap_context_t *context, IRStmt *stmt, IRSB *irbb)
{
    assert(stmt);

    return mk_dest_label(stmt->Ist.IMark.addr);
}

Stmt *translate_exit(bap_context_t *context, IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout)
{
    assert(stmt);
    assert(irbb);
    assert(irout);

    // We assume the jump destination is always ever a 32 bit uint
    assert(stmt->Ist.Exit.dst->tag == Ico_U32);

    Exp *cond = translate_expr(context, stmt->Ist.Exit.guard, irbb, irout);

    if (stmt->Ist.Exit.jk == Ijk_MapFail)
    {
        // MapFail is only used for bailing out, and the target is always
        // the beginning of the same instruction, right? At least that seems
        // to currently be true in VEX r1774.
        return new Assert(new UnOp(NOT, cond));
    }

    Name *dest = mk_dest_name(stmt->Ist.Exit.dst->Ico.U32);
    Label *next = mk_label();

    irout->push_back(new CJmp(cond, dest, new Name(next->label)));

    return next;
}

//----------------------------------------------------------------------
// Translate a single statement
//----------------------------------------------------------------------
Stmt *translate_stmt(bap_context_t *context, IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout)
{
    assert(stmt);
    assert(irout);

    Stmt *result = NULL;

    switch (stmt->tag)
    {
    case Ist_NoOp:
    
        result = new Comment("NoOp");
        break;
    
    case Ist_IMark:
    
        result = translate_imark(context, stmt, irbb);
        break;
    
    case Ist_AbiHint:
    
        result = new Comment(sTag + "AbiHint");
        break;
    
    case Ist_Put:
    
        result = translate_put(context, stmt, irbb, irout);
        break;
    
    case Ist_PutI:
    
        result = new Special(uTag + "PutI");
        break;
    
    case Ist_WrTmp:
    
        result = translate_tmp_stmt(context, stmt, irbb, irout);
        break;
    
    case Ist_Store:
    
        result = translate_store(context, stmt, irbb, irout);
        break;
    
    case Ist_CAS:

        result = translate_cas(context, stmt, irbb, irout);
        break;
    
    case Ist_LLSC:
    
        result = new Special(uTag + "LLSC");
        break;
    
    case Ist_Dirty:
    
        result = new Special(uTag + "Dirty");
        break;
    
    case Ist_MFence:
    
        result = new Comment(sTag + "MFence");
        break;
    
    case Ist_Exit:
    
        result = translate_exit(context, stmt, irbb, irout);
        break;
    
    default:
    
        panic("Unrecognized statement type");
    }

    assert(result);

    return result;
}

Stmt *translate_jumpkind(bap_context_t *context, IRSB *irbb, vector<Stmt *> *irout)
{
    assert(irbb);

    Stmt *result = NULL;
    Exp *dest = NULL;

    if (irbb->next->tag == Iex_Const)
    {
        dest = mk_dest_name(irbb->next->Iex.Const.con->Ico.U32);

        // removing the insignificant jump statment that actually goes to the next instruction
        // except for conditional jump cases
        if (irbb->jumpkind == Ijk_Boring && irbb->stmts[irbb->stmts_used - 1]->tag != Ist_Exit)
        {
            if (irbb->stmts[0]->Ist.IMark.addr + irbb->stmts[0]->Ist.IMark.len == 
                irbb->next->Iex.Const.con->Ico.U32)
            {
                Exp::destroy(dest);
                return NULL;
            }
        }
    }
    else
    {
        dest = translate_expr(context, irbb->next, irbb, irout);
    }

    switch (irbb->jumpkind)
    {
    case Ijk_Boring:
    case Ijk_Yield:
    
        result = new Jmp(dest);
        break;
    
    case Ijk_Call:

        if (!translate_calls_and_returns)
        {
            result = new Jmp(dest);
        }
        else
        {
            result = new Call(NULL, dest, vector<Exp *>());
        }

        break;
    
    case Ijk_Ret:

        if (!translate_calls_and_returns)
        {
            result = new Jmp(dest);
        }
        else
        {
            Exp::destroy(dest);
            result = new Return(NULL);
        }

        break;
    
    case Ijk_NoDecode:
    
        Exp::destroy(dest);
        result = new Special(uTag + "NoDecode");
        break;
    
    case Ijk_SigTRAP:

        result = new Special(uTag + "SIGTRAP");
        break;

    case Ijk_Sys_syscall:
    case Ijk_Sys_int32:
    case Ijk_Sys_int128:
    case Ijk_Sys_sysenter:
    
        // Since these will create a special (insert_specials), we
        // won't translate these as a jump here.
        Exp::destroy(dest);
        return NULL;
    
    default:
    
        Exp::destroy(dest);
        panic("Unrecognized jump kind");
    }

    assert(result);

    return result;
}

// FIXME: call arch specific functions
bool is_special(address_t inst)
{
    return false;
}

vector<Stmt *> *translate_special(bap_context_t *context, address_t inst)
{
    panic("Why did this get called? We are now saying that no instruction is a special.");

    return NULL;
}

//----------------------------------------------------------------------
// Generate Stmts for unknown or not translated machine instruction
//----------------------------------------------------------------------
vector<Stmt *> *translate_unknown(bap_context_t *context, string tag)
{
    vector<Stmt *> *irout = new vector<Stmt *>();

    irout->push_back(new Special(uTag + tag));

    return irout;
}

//----------------------------------------------------------------------
// Translate an IRSB into a vector of Stmts in our IR
//----------------------------------------------------------------------
vector<Stmt *> *translate_irbb(bap_context_t *context, IRSB *irbb)
{
    int i = 0;

    //
    // It's assumed that each irbb only contains the translation for
    // 1 instruction
    //

    // For some instructions, the eflag affecting IR needs out-of-band
    // arguments. This function cleans up those operands.
    do_cleanups_before_processing(context);

    assert(irbb);

    vector<Stmt *> *irout = new vector<Stmt *>();
    assert(irout);    

    //
    // Translate all the statements
    //
    IRStmt *stmt = irbb->stmts[0];
    assert(stmt->tag == Ist_IMark);

    Stmt *st = translate_stmt(context, stmt, irbb, irout);
    
    assert(st);
    assert(st->stmt_type == LABEL);
    
    irout->push_back(st);

    for (i = 0; i < irbb->tyenv->types_used; i++)
    {
        IRType ty = irbb->tyenv->types[i];
        reg_t typ = IRType_to_reg_type(ty);

        // FIXME: Can we safely remove the T_n prefix? No need to specify the type twice...
        string name = "T_" + int_to_str(get_type_size(typ)) + "t" + int_to_str(i);
        irout->push_back(new VarDecl(name, typ));
    }

    for (i = 1; i < irbb->stmts_used; i++)
    {
        stmt = irbb->stmts[i];

        try
        {
            st = translate_stmt(context, stmt, irbb, irout);
        }
        catch (const char *e)
        {
            st = new Special("IR statement not translated, cause: " + string(e));
        }

        irout->push_back(st);
    }

    //
    // Translate the jump at the end of the BB
    //
    try
    {
        st = translate_jumpkind(context, irbb, irout);
    }
    catch (const char *e)
    {
        st = new Special("Jump out of BB not translated, cause: " + string(e));
    }

    if (st != NULL)
    {
        irout->push_back(st);
    }

    return irout;
}

//======================================================================
//
// Utility functions that wrap the raw translation functions.
// These are what should be used to do the actual translation.
// See print-ir.cpp in ../../apps for examples of how to use these.
//
//======================================================================

bap_block_t *generate_vex_ir(bap_context_t *context, uint8_t *data, address_t inst)
{
    bap_block_t *vblock = new bap_block_t;

    vblock->vex_ir = NULL;
    vblock->bap_ir = NULL;

    vblock->inst = inst;
    vblock->inst_size = disasm_insn(context->guest, data, inst, vblock->str_mnem, vblock->str_op);
    
    if (vblock->inst_size > 0)
    {
        if (context->inst_size != 0 &&
            context->inst + context->inst_size != inst)
        {
            context->flag_thunks.op = CC_OP_UNDEF;
        }

        context->inst = inst;
        context->inst_size = vblock->inst_size;

        // Skip the VEX translation of special instructions because these
        // are also the ones that VEX does not handle
        if (!is_special(inst))
        {
            vblock->vex_ir = translate_insn(context->guest, data, inst, NULL);
        }
        else
        {
            vblock->vex_ir = NULL;
        }
    }    
    else
    {
        panic("generate_vex_ir(): Error while disassembling instruction");
    }    

    return vblock;
}

//----------------------------------------------------------------------
// Take a vector of instrs function and translate it into VEX IR blocks
// and store them in the vector of bap blocks
//----------------------------------------------------------------------
vector<bap_block_t *> generate_vex_ir(bap_context_t *context, uint8_t *data, address_t start, address_t end)
{
    vector<bap_block_t *> results;
    address_t inst;

    for (inst = start; inst < end; )
    {
        bap_block_t *vblock = generate_vex_ir(context, data, inst);        
        results.push_back(vblock);

        inst += vblock->inst_size;
    }

    return results;
}

//----------------------------------------------------------------------
// Insert both special("call") and special("ret") into the code.
// This function should be replacing add_special_returns()
//----------------------------------------------------------------------
void insert_specials(bap_context_t *context, bap_block_t *block)
{
    IRSB *bb = block->vex_ir;

    if (bb == NULL)
    {
        return;
    }

    IRJumpKind kind = bb->jumpkind;

    switch (kind)
    {
    case Ijk_Sys_syscall:
    case Ijk_Sys_int32:
    case Ijk_Sys_int128:
    case Ijk_Sys_sysenter:

        if (!translate_calls_and_returns)
        {
            block->bap_ir->push_back(new Special("syscall"));
        }

        break;

    case Ijk_Call:

        if (!translate_calls_and_returns)
        {
            block->bap_ir->push_back(new Special("call"));
        }

        break;

    case Ijk_Ret:

        if (!translate_calls_and_returns)
        {
            block->bap_ir->push_back(new Special("ret"));
            block->bap_ir->push_back(mk_label());
        }

        break;

    default:
        
        // do nothing
        break;
    }
}

void track_flags(bap_context_t *context, bap_block_t *block)
{
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

    if (stmt->stmt_type == MOVE)
    {
        Move *move = (Move *)stmt;

        if (move->rhs->exp_type == CONSTANT)
        {
            Constant *constant = (Constant *)move->rhs;
            
            // save CC_OP value
            context->flag_thunks.op = constant->val;
        }
    }
}

void track_itstate(bap_context_t *context, bap_block_t *block)
{
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

    if (stmt->stmt_type == MOVE)
    {
        Move *move = (Move *)stmt;

        if (move->rhs->exp_type == CONSTANT)
        {
            Constant *constant = (Constant *)move->rhs;
            uint32_t itstate = (uint32_t)constant->val;

            if (context->itstate != 0)
            {
                if (itstate != 0)
                {
                    panic("track_itstate(): Unexpected R_ITSTATE assignment");
                }

                // update instruction with actual ITSTATE value
                constant->val = (uint64_t)context->itstate;

                // go to the next ITSTATE lane
                context->itstate = context->itstate >> 8;
            }    
            else                 
            {
                context->itstate = itstate;

                if (itstate != 0)
                {
                    constant->val = 0;                    
                }
            }            
        }
    }
}

bap_context_t *init_bap_context(VexArch guest)
{
    bap_context_t *context = new bap_context_t;

    memset(context, 0, sizeof(bap_context_t));    
    
    context->guest = guest; 
    context->flag_thunks.op = CC_OP_UNDEF;    

    return context;
}

void generate_bap_ir(bap_context_t *context, bap_block_t *block)
{
    static unsigned int ir_addr = 100; // Argh, this is dumb

    assert(context);
    assert(block);    

    block->bap_ir = NULL;

    // Translate the block
    if (is_special(block->inst))
    {
        block->bap_ir = translate_special(context, block->inst);
    }
    else if (block->vex_ir)
    {
        block->bap_ir = translate_irbb(context, block->vex_ir);
    }

    if (block->bap_ir)
    {
        vector<Stmt *> *vir = block->bap_ir;

        // Go through block and add Special's for ret
        insert_specials(context, block);

        // Track current value of CC_OP guest register
        track_flags(context, block);

        // Track current value of ITSTATE guest register
        track_itstate(context, block);

        // Go through the block and add on eflags modifications
        modify_flags(context, block);

        if (context->guest == VexArchARM)
        {
            // Replace ITSTATE assignments with proper IR code
            arm_modify_itstate(context, block);

            // Replace ITSTATE assignments with proper IR code
            arm_modify_itstate_cond(context, block);
        }

        // Delete EFLAGS get thunks
        if (!use_eflags_thunks && !i386_op_is_very_broken(context->guest, block->str_mnem))
        {            
            del_get_thunk(block);
        }

        // Delete trash after eliminated x86g_use_seg_selector call
        del_seg_selector_trash(block);

        // Delete 'Internal' statements
        del_internal(block);

        // Add the asm and ir addresses
        for (int j = 0; j < vir->size(); j++)
        {
            if (block->inst)
            {
                vir->at(j)->asm_address = block->inst;
            }

            vir->at(j)->ir_address = ir_addr++;
        }
    } 
    else
    {
        block->bap_ir = translate_unknown(context, "Untranslated");
    }   
}

vector<bap_block_t *> generate_bap_ir(bap_context_t *context, vector<bap_block_t *> vblocks)
{
    unsigned int vblocksize = vblocks.size();

    for (int i = 0; i < vblocksize; i++)
    {
        bap_block_t *block = vblocks.at(i);

        generate_bap_ir(context, block);
    }

    return vblocks;
}

//----------------------------------------------------------------------
//
// Helpers
//
//----------------------------------------------------------------------

// Needed to be able to delete the Mux0X statements in shift instructions
int match_mux0x(vector<Stmt *> *ir, unsigned int i, Exp **cond, Exp **exp0,	Exp **expx, Exp **res)
{

// this code depends on the order of statements from emit_mux0x()
#ifndef MUX_AS_CJMP

    if ((int)i < 0 || 
        i >= ir->size() || 
        ir->at(i + 0)->stmt_type != VARDECL || /* temp */
        ir->at(i + 1)->stmt_type != VARDECL || /* widened_cond */
        ir->at(i + 2)->stmt_type != MOVE || 
        ir->at(i + 3)->stmt_type != MOVE)
    {
        return -1;
    }

    Move *s0 = (Move *)ir->at(i + 2);
    Move *s1 = (Move *)ir->at(i + 3);

    if (s0->lhs->exp_type != TEMP || 
        s1->lhs->exp_type != TEMP || 
        s0->rhs->exp_type != CAST || 
        s1->rhs->exp_type != BINOP)
    {
        return -1;
    }

    Cast *e1 = static_cast<Cast *>(s0->rhs);
    BinOp *e2 = static_cast<BinOp *>(s1->rhs);

    if (e1->cast_type != CAST_SIGNED || 
        e2->binop_type != BITOR || 
        e2->lhs->exp_type != BINOP || 
        e2->rhs->exp_type != BINOP)
    {
        return -1;
    }

    Exp *e3 = e1->exp; /* e3 is the condition */
    BinOp *e4 = static_cast<BinOp *>(e2->lhs); /* e4 is the true branch */
    BinOp *e5 = static_cast<BinOp *>(e2->rhs); /* e5 is the false branch */

    if (e4->binop_type != BITAND || e5->binop_type != BITAND)
    {
        return -1;
    }

    Exp *e6 = e4->lhs; /* this is exp0 */
    Exp *e7 = e5->lhs; /* this is expX */

    if (cond)
    {
        *cond = e3;
    }

    if (exp0)
    {
        *exp0 = e6;
    }

    if (expx)
    {
        *expx = e7;
    }

    if (res)
    {
        *res = NULL; /* XXX: Not sure what to do here */
    }

#else

    if ((int)i < 0 || i >= ir->size() || 
        ir->at(i)->stmt_type != MOVE || 
        ir->at(i + 3)->stmt_type != MOVE || 
        ir->at(i + 2)->stmt_type != LABEL || 
        ir->at(i + 4)->stmt_type != LABEL || 
        ir->at(i + 1)->stmt_type != CJMP)
    {
        return -1;
    }

    Move *s0 = (Move *)ir->at(i);
    CJmp *s1 = (CJmp *)ir->at(i + 1);
    Label *s2 = (Label *)ir->at(i + 2);
    Move *s3 = (Move *)ir->at(i + 3);
    Label *s4 = (Label *)ir->at(i + 4);

    if (s0->lhs->exp_type != TEMP || 
        s3->lhs->exp_type != TEMP || 
        ((Temp *)s0->lhs)->name != ((Temp *)s3->lhs)->name)
    {
        return -1;
    }

    if (s1->t_target->exp_type != NAME || 
        s1->f_target->exp_type != NAME || 
        ((Name *)s1->f_target)->name != s2->label || 
        ((Name *)s1->t_target)->name != s4->label)
    {
        return -1;
    }

    if (cond)
    {
        *cond = s1->cond;
    }

    if (exp0)
    {
        *exp0 = s0->rhs;
    }

    if (expx)
    {
        *expx = s3->rhs;
    }

    if (res)
    {
        *res = s0->lhs;
    }

#endif

    return 0;
}

reg_t get_exp_type_from_cast(Cast *cast)
{
    assert(cast);

    return cast->typ;
}

reg_t get_exp_type(Exp *exp)
{
    assert(exp);

    reg_t type;

    if (exp->exp_type == TEMP)
    {
        type = ((Temp *)exp)->typ;
    }

    else if (exp->exp_type == CONSTANT)
    {
        type = ((Constant *)exp)->typ;
    }

    else
    {
        panic("Expression has no type info: " + exp->tostring());
    }

    return type;
}

void do_cleanups_before_processing(bap_context_t *context)
{
    if (context->count_opnd)
    {
        context->count_opnd = NULL;
    }
}
