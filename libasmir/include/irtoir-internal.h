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

Exp *translate_expr(IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout);

int match_mux0x(vector<Stmt *> *ir, unsigned int i, Exp **cond, Exp **exp0,	Exp **expx, Exp **res);

extern bool use_eflags_thunks;
extern bool use_simple_segments;
extern Exp *count_opnd;

//
// arch specific functions used in irtoir.cpp
//

// defined in irtoir-i386.cpp
vector<VarDecl *> i386_get_reg_decls();
Exp  *i386_translate_get(IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout);
Stmt *i386_translate_put(IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout);
Exp  *i386_translate_ccall(IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout);
void  i386_modify_flags(bap_block_t *block);
bool i386_op_is_very_broken(string op);
void del_get_thunk(bap_block_t *block);

// defined in irtoir-arm.cpp
vector<VarDecl *> arm_get_reg_decls();
Exp  *arm_translate_get(IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout);
Stmt *arm_translate_put(IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout);
Exp  *arm_translate_ccall(IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout);
void  arm_modify_flags(bap_block_t *block);


#endif
