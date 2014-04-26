/*
 Owned and copyright BitBlaze, 2007. All rights reserved.
 Do not copy, disclose, or distribute without explicit written
 permission.
*/
#include "stmt.h"
#include <assert.h>
#include <stdlib.h>

stmt_type_t stmt_type(Stmt *s) {
  return s->stmt_type;
}
Exp* move_lhs(Stmt *s) {
  return ((Move*)s)->lhs;
}
Exp* move_rhs(Stmt *s) {
  return ((Move*)s)->rhs;
}
const char* label_string(Stmt *s) {
  return ((Label*)s)->label.c_str();
}
TraceAttrs_t* stmt_attributes(Stmt *s) {
  return &(s->attributes);
}

threadid_t trace_tid(TraceAttrs_t *ta) {
  if (ta == NULL) return 0;
  return ta->tid;
}

int conc_map_size(TraceAttrs_t *ta) {
  if (ta == NULL || ta->cv == NULL) return 0;
  return ta->cv->size();
}
conc_map* get_cval(TraceAttrs_t *ta, int i) {
  if (ta == NULL || ta->cv == NULL) return NULL;
  return ta->cv->at(i);
}
const char* cval_name(conc_map *m) {
  return m->name.c_str();
}
big_val_t* cval_value(conc_map *m) {
  return &(m->value);
}
long cval_value_size(big_val_t *v) {
  return v->size();
}
const_val_t cval_value_part(big_val_t *v, int i) {
  return v->at(i);
}
const_val_t cval_ind(conc_map *m) {
  return m->index;
}
long cval_mem(conc_map *m) {
  return m->mem;
}
cval_type_t cval_type(conc_map *m) {
  return m->type;
}
int cval_usage(conc_map *m) {
  return m->usage;
}
int cval_taint(conc_map *m) {
  return m->taint;
}
const char* asm_string_from_stmt(Stmt*s) {
  return s->assembly.c_str();
}
const char* special_string(Stmt *s) {
  return ((Special*)s)->special.c_str();
}
const char* comment_string(Stmt *s) {
  return ((Comment*)s)->comment.c_str();
}
Exp* jmp_target(Stmt *s) {
  return ((Jmp*)s)->target;
}
Exp* cjmp_cond(Stmt *s) {
  return ((CJmp*)s)->cond;
}
Exp* cjmp_ttarget(Stmt *s) {
  return ((CJmp*)s)->t_target;
}
Exp* cjmp_ftarget(Stmt *s) {
  return ((CJmp*)s)->f_target;
}
Exp* expstmt_exp(Stmt *s) {
  return ((ExpStmt*)s)->exp;
}
const char* vardecl_name(Stmt *s) {
  return ((VarDecl*)s)->name.c_str();
}
reg_t vardecl_type(Stmt *s) {
  return ((VarDecl*)s)->typ;
}
long call_has_lval(Stmt *s) {
  return (((Call*)s)->lval_opt != NULL);
}
Exp* call_lval_opt(Stmt *s) {
  return ((Call*)s)->lval_opt;
}
Exp * call_fnname(Stmt *s) {
  return ((Call*)s)->callee;
}


// cut and paste from exectrace_c. should probably put in a utility file.
template< class T >
static T* vec_to_list(const vector<T> &vec)
{
  //  T* rv = new T[vec.size()+1];
  T* rv = (T*)malloc((vec.size()+1) * sizeof(T));
  assert(rv != NULL);

  for(unsigned int i = 0; i < vec.size(); i++) {
    rv[i] = vec[i];
  }
  rv[vec.size()] = NULL;

  return rv;
}

Exp** call_params(Stmt *s) {
  return vec_to_list(((Call*)s)->params);
}

long ret_has_exp(Stmt *s) {
  return (((Return*)s)->exp_opt != NULL);
}

Exp* ret_exp(Stmt *s) {
  assert(ret_has_exp(s));
  return (((Return*)s)->exp_opt);
}

const char* func_name(Stmt *s) {
  return (((Func*)s)->fnname.c_str());
}

long func_has_rv(Stmt *s) {
  return (((Func*)s)->has_rv);
}

reg_t func_rt(Stmt *s) {
  return (((Func*)s)->rt);
}

Stmt** func_params(Stmt *s) {
  // going from vector of vardecls to vector of stmts
  vector<Stmt*> params;
  params.insert(params.begin(), 
                ((Func*)s)->params.begin(),
                ((Func*)s)->params.end());
  return vec_to_list(params);
}

long func_is_external(Stmt *s) {
  return (((Func*)s)->external);
}

Stmt** func_body(Stmt *s) {
  return vec_to_list(((Func*)s)->body);
}

Exp* assert_cond(Stmt *s)
{
  return ((Assert*)s)->cond;
}
