/*
 Owned and copyright BitBlaze, 2007. All rights reserved.
 Do not copy, disclose, or distribute without explicit written
 permission.
*/
/*
 * When editing this file, please make sure to also update libasmir.idl
 * appropriately.
 */

#ifndef _EXP_H
#define _EXP_H
#include "common.h"
#include "irvisitor.h"

/* defines for types which will be declared either globally for C, or 
 * whithin certain classes for C++ */
// Stuff in Exp
enum exp_type_t {
  BINOP, UNOP, CONSTANT, MEM, TEMP, PHI, CAST,
  NAME, UNKNOWN, LET, EXTENSION };

enum reg_t { REG_1, REG_8, REG_16, REG_32, REG_64 };

// The size of an address.
#define REG_ADDRESS_T  REG_32


// Stuff in BinOp
  /// IMPORTANT: If you add/remove anything from this, you must
  /// re-synchronize the binoptype with the correct symbol in
  /// exp.cpp so tostring() works. For example, PLUS is index 0 so
  /// that strs[0] = "+". 
enum binop_type_t {
  PLUS = 0, MINUS ,   TIMES ,  DIVIDE,
  MOD,      LSHIFT,   RSHIFT,  ARSHIFT,
  LROTATE,  RROTATE,  LOGICAND, LOGICOR,
  BITAND,  BITOR,       XOR,      EQ,
  NEQ,  GT,       LT,       GE,
  LE, SDIVIDE, SMOD    };

// Stuff in UnOp
enum unop_type_t {NEG, NOT};
// Stuff in Constant
typedef uint64_t const_val_t;

// Stuff in Cast
    //
    // Widening casts (e.g. 32 to 64) can either be signed or unsigned.
    // Narrowing casts (e.g. 64 to 32) use either the high or the low half.
    // Casts to/from and between floating points number use the float 
    //    and integer types depending on which way the cast is going.
    // Reinterpretations (i.e. reinterpret a 32 bit int as a 32 bit float)
    //    use either the rfloat or the rinteger types depending on which way
    //    the cast is going.
    //
enum cast_t {
  CAST_UNSIGNED, CAST_SIGNED, CAST_HIGH, CAST_LOW,
  CAST_FLOAT, CAST_INTEGER, CAST_RFLOAT, CAST_RINTEGER };

#ifndef __cplusplus
typedef struct Exp Exp;
#endif


#ifdef __cplusplus

#include <stdint.h>
#include <string>
#include <set>
#include <map>
#include <sstream>
#include <vector>


using namespace std;


/// Exp are pure expressions, i.e., side-effect free. 
/// Our expression types are straight-forward:
///   - BinOp is a binary operation, e.g., addition
///   - UnOp is a unary operation, e.g., negation
///   - Constant is a constant in the code.
///   - Mem is a memory location
///   - Temp is an abstract register. There are an infinite number of
///     abstract registers.
///   - Phi is the SSA Phi node.  Although not technically necessary,
///     we expect it will be useful to have.
///  Temp, Mem, and Constant's are all operands, and have associated
/// with them their bit-width and a type of register
/// (floating/unsigned/signed).  
class Exp {
 public:
  Exp(exp_type_t e) { exp_type = e; };

  /// Make a deep copy of the @param exp by calling @param exp->clone()
  static Exp *clone(Exp *exp);
  static void destroy(Exp *expr);
  static string string_type(const reg_t &reg);
  static uint32_t reg_to_bits(const reg_t &reg);
  static uint64_t cast_value(reg_t t, uint64_t v);

  /// Make a deep copy of the Exp
  virtual Exp* clone() const = 0;
  virtual void accept(IRVisitor *v) = 0;
  virtual string tostring() const = 0;
  virtual ~Exp() {};
  exp_type_t   exp_type;
};


class BinOp : public Exp {
 public:
  BinOp(binop_type_t t, Exp *l, Exp *r);
  virtual void accept(IRVisitor *v) { v->visitBinOp(this); }
  virtual ~BinOp() {};
  BinOp(const BinOp& copy);
  virtual string tostring() const;
  virtual BinOp *clone() const;

  /// Utility to convert binop to the string representation, e.g.
  /// PLUS -> "+"
  static string optype_to_string(const binop_type_t t);
  // PLUS -> "PLUS"
  static string optype_to_name(const binop_type_t t);
  /// Reverse mapping from string to binop_type_t
  static binop_type_t string_to_optype(const string s);
  static void destroy( BinOp *expr );

  Exp *lhs;
  Exp *rhs;
  binop_type_t binop_type;
};

class UnOp : public Exp {
 public:
  UnOp(unop_type_t typ, Exp *e);
  UnOp(const UnOp& copy);
  virtual ~UnOp(){}; 
  virtual void accept(IRVisitor *v) { v->visitUnOp(this); };
  virtual string tostring() const;
  virtual UnOp *clone() const;
  static string optype_to_string(const unop_type_t t);
  static unop_type_t string_to_optype(const string s);
  static void destroy( UnOp *expr );

  unop_type_t unop_type;
  Exp *exp;
};


class Mem : public Exp {
 public:
  //Mem(Exp *e);
  Mem(Exp *e, reg_t t);
  Mem(const Mem& copy);

  virtual ~Mem(){ };
  virtual Mem *clone() const;
  virtual void accept(IRVisitor *v) 
  { v->visitMem(this); }
  virtual string tostring() const;
  static void destroy( Mem *expr );

  Exp *addr;
  reg_t typ;
};

class Constant : public Exp {
 public:
  Constant(reg_t t, const_val_t val); 
  Constant(const Constant& other);
  virtual Constant *clone() const;
  virtual ~Constant() {}
  static void destroy( Constant *expr );

  virtual string tostring() const;
  virtual void accept(IRVisitor *v) { v->visitConstant(this); }
  reg_t typ;
  const_val_t val;
  // True and false constants.
  static Constant t,f;
};

class Phi : public Exp {
  public:
  Phi(string phi_name, vector<Temp*> vars);
  Phi(const Phi& copy);
  virtual ~Phi() {}
  void addVar(Exp* e);
  virtual string tostring() const;
  virtual Phi *clone() const;
  virtual void accept(IRVisitor *v) { v->visitPhi(this); }
  static void destroy( Phi *expr );
  vector<Temp*> vars;		/// phi arguments.
  string phi_name; /// The original name for these phi variables.
};

class Temp : public Exp {
 public:
  Temp(reg_t typ, string n);
  Temp(const Temp & other);

  virtual Temp *clone() const;
  virtual ~Temp() {} ;
  virtual void accept(IRVisitor *v)
    { v->visitTemp(this); }
  virtual string tostring() const;
  static void destroy( Temp *expr );

  reg_t typ;
  string name;
};

class Unknown : public Exp {
 public:
  Unknown(string s, reg_t typ); 
  Unknown(const Unknown &other);
  virtual ~Unknown(){};
  virtual Unknown *clone() const;
  virtual string tostring() const { return "unknown \""+str+"\" : " + Exp::string_type(typ); };
  virtual void accept(IRVisitor *v) { v->visitUnknown(this); };
  static void destroy( Unknown *expr );
  string str;
  reg_t typ;
};

class Cast : public Exp {

  public:
  Cast( Exp *exp, reg_t reg, cast_t type );
  Cast( const Cast &copy );
  virtual ~Cast() { };
  virtual Cast *clone() const;
  virtual string tostring() const;
  virtual void accept( IRVisitor *v ) { v->visitCast(this); }
  static string cast_type_to_string( const cast_t ctype );
  static void destroy( Cast *expr );

  Exp *exp;
  reg_t typ;
  cast_t cast_type;
};

class Name : public Exp {

  public:
  Name(string n);
  Name( const Name &copy );
  virtual ~Name() { };
  virtual Name *clone() const;
  virtual string tostring() const;
  virtual void accept( IRVisitor *v ) { v->visitName(this); }
  static void destroy( Name *expr );
  string name;
};

class Let : public Exp {
  public:
    Let(Exp *var, Exp *exp, Exp *in);
    Let(const Let &other);
    virtual ~Let(){}; 
    virtual void accept(IRVisitor *v) { v->visitLet(this); }
    static void destroy(Exp *e);
    virtual Let *clone() const;
    virtual string tostring() const;
    Exp *var, *exp, *in;
};

//======================================================================
//
// Shorthand functions for creating expressions
//
//======================================================================
Exp *ecl( Exp *exp );
//Constant *_ex_const( Constant co );
Constant *ex_const(uint32_t value);
Constant *ex_const(reg_t t, const_val_t value );
Name *ex_name( string name );
UnOp *_ex_not( Exp *arg );
UnOp *ex_not( Exp *arg );
BinOp *_ex_add( Exp *arg1, Exp *arg2 );
BinOp *ex_add( Exp *arg1, Exp *arg2 );
BinOp *_ex_sub( Exp *arg1, Exp *arg2 );
BinOp *ex_sub( Exp *arg1, Exp *arg2 );
BinOp *_ex_mul( Exp *arg1, Exp *arg2 );
BinOp *ex_mul( Exp *arg1, Exp *arg2 );
BinOp *_ex_div( Exp *arg1, Exp *arg2 );
BinOp *ex_div( Exp *arg1, Exp *arg2 );
BinOp *_ex_and( Exp *arg1, Exp *arg2 );
BinOp *_ex_and( Exp *arg1, Exp *arg2, Exp *arg3 );
BinOp *_ex_and( Exp *arg1, Exp *arg2, Exp *arg3, Exp *arg4, Exp *arg5, Exp *arg6, Exp *arg7 );
BinOp *ex_and( Exp *arg1, Exp *arg2 );
BinOp *ex_and( Exp *arg1, Exp *arg2, Exp *arg3 );
BinOp *ex_and( Exp *arg1, Exp *arg2, Exp *arg3, Exp *arg4, Exp *arg5, Exp *arg6, Exp *arg7 );
BinOp *_ex_or( Exp *arg1, Exp *arg2 );
BinOp *_ex_or( Exp *arg1, Exp *arg2, Exp *arg3 );
BinOp *_ex_or( Exp *arg1, Exp *arg2, Exp *arg3, Exp *arg4, Exp *arg5, Exp *arg6 );
BinOp *_ex_or( Exp *arg1, Exp *arg2, Exp *arg3, Exp *arg4, Exp *arg5, Exp *arg6, Exp *arg7 );
BinOp *ex_or( Exp *arg1, Exp *arg2 );
BinOp *ex_or( Exp *arg1, Exp *arg2, Exp *arg3 );
BinOp *ex_or( Exp *arg1, Exp *arg2, Exp *arg3, Exp *arg4, Exp *arg5, Exp *arg6 );
BinOp *ex_or( Exp *arg1, Exp *arg2, Exp *arg3, Exp *arg4, Exp *arg5, Exp *arg6, Exp *arg7 );
BinOp *_ex_xor( Exp *arg1, Exp *arg2 );
BinOp *_ex_xor( Exp *arg1, Exp *arg2, Exp *arg3 );
BinOp *_ex_xor( Exp *arg1, Exp *arg2, Exp *arg3, Exp *arg4 );
BinOp *_ex_xor( Exp *arg1, Exp *arg2, Exp *arg3, Exp *arg4,
                       Exp *arg5, Exp *arg6, Exp *arg7, Exp *arg8 );
BinOp *ex_xor( Exp *arg1, Exp *arg2 );
BinOp *ex_xor( Exp *arg1, Exp *arg2, Exp *arg3 );
BinOp *_ex_shl( Exp *arg1, Exp *arg2 );
BinOp *ex_shl( Exp *arg1, Exp *arg2 );
BinOp *ex_shl( Exp *arg1, int arg2 );
BinOp *_ex_shr( Exp *arg1, Exp *arg2 );
BinOp *ex_shr( Exp *arg1, Exp *arg2 );
BinOp *ex_shr( Exp *arg1, int arg2 );
BinOp *_ex_sar( Exp *arg1, Exp *arg2 );
BinOp *ex_sar( Exp *arg1, Exp *arg2 );
BinOp *ex_sar( Exp *arg1, int arg2 );
BinOp *_ex_eq( Exp *arg1, Exp *arg2 );
BinOp *ex_eq( Exp *arg1, Exp *arg2 );
BinOp *_ex_neq( Exp *arg1, Exp *arg2 );
BinOp *ex_neq( Exp *arg1, Exp *arg2 );
BinOp *ex_gt( Exp *arg1, Exp *arg2 );
BinOp *ex_lt( Exp *arg1, Exp *arg2 );
BinOp *ex_ge( Exp *arg1, Exp *arg2 );
BinOp *ex_le( Exp *arg1, Exp *arg2 );
Cast *ex_u_cast( Exp *arg, reg_t r );
Cast *_ex_u_cast( Exp *arg, reg_t r );
Cast *ex_s_cast( Exp *arg, reg_t r );
Cast *_ex_s_cast( Exp *arg, reg_t r );
Cast *ex_h_cast( Exp *arg, reg_t width );
Cast *ex_l_cast( Exp *arg, reg_t width );
Cast *_ex_l_cast( Exp *arg, reg_t width );
Cast *ex_i_cast( Exp *arg, reg_t width );
Cast *ex_f_cast( Exp *arg, reg_t width );
Cast *ex_ri_cast( Exp *arg, reg_t width );
Cast *ex_rf_cast( Exp *arg, reg_t width );

extern "C" {
#endif // def __cplusplus

  extern exp_type_t exp_type(Exp*);
  extern binop_type_t binop_type(Exp*);
  extern Exp* binop_lhs(Exp*);
  extern Exp* binop_rhs(Exp*);
  extern unop_type_t unop_type(Exp*);
  extern Exp* unop_subexp(Exp*);
  extern Exp* mem_addr(Exp*);
  extern reg_t mem_regtype(Exp*);
  extern const_val_t constant_val(Exp*);
  extern reg_t constant_regtype(Exp*);
  extern const char* phi_phiname(Exp*);
  extern int phi_numnodes(Exp*);
  extern Exp* phi_nodeat(Exp*, int);
  extern reg_t temp_regtype(Exp*);
  extern const char* temp_name(Exp*);
  extern const char* unknown_str(Exp*);
  extern reg_t unknown_regtype(Exp*);
  extern reg_t cast_width(Exp*);
  extern cast_t cast_casttype(Exp*);
  extern Exp* cast_subexp(Exp*);
  extern const char* name_string(Exp*);
  extern Exp *let_var(Exp *);
  extern Exp *let_exp(Exp *);
  extern Exp *let_in(Exp *);

#ifdef __cplusplus
}
#endif

#endif
