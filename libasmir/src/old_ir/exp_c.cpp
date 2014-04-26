/*
 Owned and copyright BitBlaze, 2007. All rights reserved.
 Do not copy, disclose, or distribute without explicit written
 permission.
*/
/* File: exp_c.cpp
   Author: Ivan Jager <aij@andrew.cmu.edu>
   C interface for stuff in exp.cpp
*/

/* to fix most files that used the old types:
   sed -i -e 's/Exp:://' -e 's/BinOp:://' -e 's/UnOp:://' -e 's/Constant:://' -e 's/Cast::CAST_/CAST_/' <filename>
*/

#include "exp.h"

// all of these are declared extern in exp.h

exp_type_t exp_type(Exp *e) {
  return e->exp_type;
}
binop_type_t binop_type(Exp *e) {
  return ((BinOp*)e)->binop_type;
}
Exp* binop_lhs(Exp *e) {
  return ((BinOp*)e)->lhs;
}
Exp* binop_rhs(Exp *e) {
  return ((BinOp*)e)->rhs;
}
unop_type_t unop_type(Exp *e) {
  return ((UnOp*)e)->unop_type;
}
Exp* unop_subexp(Exp *e) {
  return ((UnOp*)e)->exp;
}
Exp* mem_addr(Exp *e) {
  return ((Mem*)e)->addr;
}
reg_t mem_regtype(Exp *e) {
  return ((Mem*)e)->typ;
}
const_val_t constant_val(Exp *e) {
  return ((Constant*)e)->val;
}
reg_t constant_regtype(Exp *e) {
  return ((Constant*)e)->typ;
}
const char* phi_phiname(Exp *e) {
  return ((Phi*)e)->phi_name.c_str();
}
int phi_numnodes(Exp *e) {
  return ((Phi*)e)->vars.size();
}
Exp* phi_nodeat(Exp *e, int i) {
  return ((Phi*)e)->vars[i];
}
reg_t temp_regtype(Exp *e) {
  return ((Temp*)e)->typ;
}
const char* temp_name(Exp *e) {
  return ((Temp*)e)->name.c_str();
}
const char* unknown_str(Exp *e) {
  return ((Unknown*)e)->str.c_str();
}
reg_t unknown_regtype(Exp *e) {
  return ((Unknown*)e)->typ;
}
reg_t cast_width(Exp *e) {
  return ((Cast*)e)->typ;
}
cast_t cast_casttype(Exp *e) {
  return ((Cast*)e)->cast_type;
}
Exp* cast_subexp(Exp *e) {
  return ((Cast*)e)->exp;
}
const char* name_string(Exp *e) {
  return ((Name*)e)->name.c_str();
}

Exp *
let_var(Exp *e)
{
  return ((Let *)e)->var;
}

Exp *
let_exp(Exp *e)
{
  return ((Let *)e)->exp;
}

Exp *
let_in(Exp *e)
{
  return ((Let *)e)->in;
}
