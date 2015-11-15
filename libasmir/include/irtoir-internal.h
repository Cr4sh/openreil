#ifndef IRTOIR_INTERNAL_H
#define IRTOIR_INTERNAL_H

#include "common.h"
#include "irtoir.h"

/* We need this for emit_mux/match_mux etc. */
#ifndef MUX_AS_CJMP

/* temp + temp + move + move */
#define MUX_LENGTH 4

#else

#error "Please define MUX_LENGTH.. I think it is probably 6."
#define MUX_LENGTH 6

#endif

/* Offset from end of mux to setting of CC_OP */
#define MUX_OFFSET 2

#define MUX_SUB (MUX_LENGTH + MUX_OFFSET - 1)

/* Undefined value for CC_OP */
#define CC_OP_UNDEF -1


#define MAX_VEX_FN_ARGS 6

/* See INTERNAL_VEX_FN_ARG_LIST in stmt.h */
typedef struct internal_vex_fn_arg_s
{
    IRType type;
    IRTemp temp;

} internal_vex_fn_arg;

typedef struct internal_vex_fn_arg_list_s
{
    int count;
    internal_vex_fn_arg args[MAX_VEX_FN_ARGS];

} internal_vex_fn_arg_list;


Temp *mk_reg(string name, reg_t width);
reg_t IRType_to_reg_type(IRType type);
reg_t regt_of_irexpr(IRSB *irbb, IRExpr *e);

reg_t get_exp_type(Exp *exp);

inline int get_type_size(reg_t typ)
{
    return Exp::reg_to_bits(typ);
}

string translate_tmp_name(bap_context_t *context, IRType type, IRTemp tmp);
string translate_tmp_name(bap_context_t *context, IRSB *irbb, IRExpr *expr, IRTemp tmp);
IRType translate_tmp_type(bap_context_t *context, IRSB *irbb, IRExpr *expr);

Temp *mk_temp(string name, IRType ty);
Temp *mk_temp(reg_t type, vector<Stmt *> *stmts);
Temp *mk_temp(IRType ty, vector<Stmt *> *stmts);

Label *mk_dest_label(Addr64 dest);
Name *mk_dest_name(Addr64 dest);

Exp *translate_expr(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout);

Exp *emit_mux0x(vector<Stmt *> *irout, reg_t type, Exp *cond, Exp *exp0, Exp *expX);
int match_mux0x(vector<Stmt *> *ir, unsigned int i, Exp **cond, Exp **exp0,	Exp **expx, Exp **res);

extern bool use_eflags_thunks;
extern bool use_simple_segments;

//
// arch specific functions used in irtoir.cpp
//

//
// These typedefs are specifically for casting mod_eflags_func to the
// function that accepts the right number of arguments
//
typedef vector<Stmt *> Mod_Func_0(bap_context_t *context);
typedef vector<Stmt *> Mod_Func_2(bap_context_t *context, reg_t, Exp *, Exp *);
typedef vector<Stmt *> Mod_Func_3(bap_context_t *context, reg_t, Exp *, Exp *, Exp *);

void modify_eflags_helper(bap_context_t *context, bap_block_t *block, string op, reg_t type, int argnum, Mod_Func_0 *mod_eflags_func);

// defined in irtoir.cpp
void set_flag(vector<Stmt *> *irout, reg_t type, Temp *flag, Exp *cond);

void del_get_thunk(bap_block_t *block);
int del_put_thunk(bap_block_t *block, int op, int dep1, int dep2, int ndep, int mux0x);
void get_put_thunk(bap_block_t *block, int *op, int *dep1, int *dep2, int *ndep, int *mux0x);

void del_get_itstate(bap_block_t *block);
void del_put_itstate(bap_block_t *block, int itstate);
void get_put_itstate(bap_block_t *block, int *itstate);

// defined in irtoir-i386.cpp
vector<VarDecl *> i386_get_reg_decls();
Exp  *i386_translate_get(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout);
Stmt *i386_translate_put(bap_context_t *context, IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout);
Exp  *i386_translate_ccall(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout);
void i386_modify_flags(bap_context_t *context, bap_block_t *block);
bool i386_op_is_very_broken(VexArch guest, string op);

// defined in irtoir-arm.cpp
vector<VarDecl *> arm_get_reg_decls();
Exp  *arm_translate_get(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout);
Stmt *arm_translate_put(bap_context_t *context, IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout);
Exp  *arm_translate_ccall(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout);
void arm_modify_flags(bap_context_t *context, bap_block_t *block);
void arm_modify_itstate(bap_context_t *context, bap_block_t *block);
void arm_modify_itstate_cond(bap_context_t *context, bap_block_t *block);

#endif
