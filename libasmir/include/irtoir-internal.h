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

#define CC_OP_UNDEF -1

#define CC_OP_SET(_context_, _val_) (_context_)->flag_thunks.op = (_val_)
#define CC_OP_GET(_context_) (_context_)->flag_thunks.op

#define CC_OP_RESET(_context_) CC_OP_SET(_context_, CC_OP_UNDEF)

Temp *mk_reg(string name, reg_t width);
reg_t IRType_to_reg_type(IRType type);
reg_t regt_of_irexpr(IRSB *irbb, IRExpr *e);

reg_t get_exp_type(Exp *exp);

inline int get_type_size(reg_t typ)
{
    return Exp::reg_to_bits(typ);
}

Temp *mk_temp(string name, IRType ty);
Temp *mk_temp(reg_t type, vector<Stmt *> *stmts);
Temp *mk_temp(IRType ty, vector<Stmt *> *stmts);

Exp *translate_expr(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout);

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

void modify_eflags_helper(bap_context_t *context, string op, reg_t type, vector<Stmt *> *ir, int argnum, Mod_Func_0 *mod_eflags_func);

// defined in irtoir.cpp
void set_flag(vector<Stmt *> *irout, reg_t type, Temp *flag, Exp *cond);
void del_get_thunk(bap_block_t *block);
int del_put_thunk(vector<Stmt *> *ir, string mnemonic, int opi, int dep1, int dep2, int ndep, int mux0x);
void get_thunk_index(vector<Stmt *> *ir, int *op, int *dep1, int *dep2, int *ndep, int *mux0x);

// defined in irtoir-i386.cpp
vector<VarDecl *> i386_get_reg_decls();
Exp  *i386_translate_get(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout);
Stmt *i386_translate_put(bap_context_t *context, IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout);
Exp  *i386_translate_ccall(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout);
void  i386_modify_flags(bap_context_t *context, bap_block_t *block);
bool i386_op_is_very_broken(string op);

// defined in irtoir-arm.cpp
vector<VarDecl *> arm_get_reg_decls();
Exp  *arm_translate_get(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout);
Stmt *arm_translate_put(bap_context_t *context, IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout);
Exp  *arm_translate_ccall(bap_context_t *context, IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout);
void  arm_modify_flags(bap_context_t *context, bap_block_t *block);


#endif
