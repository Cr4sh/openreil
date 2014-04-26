/*
 Owned and copyright BitBlaze, 2007. All rights reserved.
 Do not copy, disclose, or distribute without explicit written
 permission.
*/
#ifndef _STMT_H
#define _STMT_H
#include "irvisitor.h"
#include "exp.h"

enum cval_type_t {NONE, BOOL, CHR, INT_16, INT_32, INT_64, INT_128};
enum stmt_type_t {JMP,CJMP, SPECIAL, MOVE,  COMMENT,  LABEL, EXPSTMT, VARDECL,
                  CALL, RETURN, FUNCTION, ASSERT};


#ifndef __cplusplus
typedef struct Stmt Stmt;
#endif

#ifdef __cplusplus
#include <stdint.h>
#include <string>
#include <vector>
using namespace std;

typedef vector<const_val_t> big_val_t;

typedef struct ConcPair {
  string name;
  bool mem;
  cval_type_t type;
  const_val_t index;
  big_val_t value;
  int usage;
  int taint;
  ConcPair(string str, bool m, cval_type_t typ, 
           const_val_t ind, big_val_t val, 
	   int usg,int tnt){
    name = str; mem = m; 
    type = typ; index = ind; 
    value = val; usage = usg;
    taint = tnt;
  };
} conc_map;
typedef vector<conc_map *> conc_map_vec;

typedef uint32_t threadid_t;

typedef struct TraceAttrs_s {
  TraceAttrs_s() { cv = NULL; tid = -1; }
  conc_map_vec *cv;
  threadid_t tid;
} TraceAttrs_t;

class Stmt {
 public:
  virtual ~Stmt() { };
  virtual void accept(IRVisitor *v) = 0;
  virtual string tostring() = 0;
  static Stmt *clone(Stmt *s);
  static void destroy(Stmt *s);

  Stmt(stmt_type_t st, address_t asm_ad, address_t ir_ad, threadid_t tid = -1)
    { asm_address = asm_ad; ir_address = ir_ad; 
      stmt_type = st; };
  
  /// Make a deep copy of the stmt
  virtual Stmt *clone() const = 0;

  /// The assembly instruction address for this statement.
  /// Many statements may have the same asm_address since a single
  /// assembly instruction may translate into many IR statements
  address_t asm_address;
  /// The unique ir instruction address for this statement.
  /// The uniqueness if determined by the application using this
  /// object, e.g., translation keeps a counter.
  address_t ir_address;
  stmt_type_t stmt_type;
  // The attributes containing the concrete values
  TraceAttrs_t attributes;
  string assembly;
};

class VarDecl : public Stmt {
 public:
  VarDecl(string name, reg_t typ, address_t asm_ad = 0x0, 
	  address_t ir_ad = 0x0);
  VarDecl(Temp *t);
  VarDecl(const VarDecl &other);
  virtual ~VarDecl() { };
  virtual void accept(IRVisitor *v) { v->visitVarDecl(this); };
  virtual string tostring();
  virtual VarDecl *clone() const;
  string name;
  reg_t typ;
};

class Move : public Stmt {
 public:
  Move(Exp *l, Exp *r, address_t asm_ad = 0x0, address_t ir_ad = 0x0);
  Move(const Move &other);
  virtual ~Move() { };
  virtual void accept(IRVisitor *v) { v->visitMove(this); };
  virtual string tostring();
  virtual Move *clone() const;

  Exp *lhs;
  Exp *rhs;
};

class Label : public Stmt {
 public:
  Label(string l, address_t asm_ad = 0x0, address_t ir_ad = 0x0);
  Label(const Label &other);
  virtual ~Label() { };
  virtual void accept(IRVisitor *v) { v->visitLabel(this); };
  virtual string tostring();
  virtual Label *clone() const;
  string label;
};

class Jmp : public Stmt {
 public:
  Jmp( Exp *e, address_t asm_ad = 0x0, address_t ir_ad = 0x0 );
  Jmp(const Jmp &other);
  virtual ~Jmp() { };
  virtual void accept(IRVisitor *v) { v->visitJmp(this); };
  virtual string tostring();
  virtual Jmp *clone() const;
  Exp *target;
};

class CJmp : public Stmt {
 public:
  CJmp(Exp *c, Exp *t, Exp *f, address_t asm_ad = 0x0, 
       address_t ir_ad = 0x0);
  CJmp(const CJmp &other);
  virtual ~CJmp() { };

  virtual void accept(IRVisitor *v) { v->visitCJmp(this); }
  virtual string tostring();
  virtual CJmp *clone() const;
  Exp *cond;
  Exp * t_target;    // true edge target
  Exp * f_target;    // false edge target
  
};

class Special : public Stmt {
 public:
  Special(string s, address_t asm_ad = 0x0, address_t ir_ad = 0x0);
  Special(const Special &other);
  virtual ~Special(){};
  
  virtual void accept(IRVisitor *v) { v->visitSpecial(this); };
  virtual string tostring(); 
  virtual Special *clone() const;
  string special;
  
};



class Comment : public Stmt {
 public:
  Comment(const Comment &other);
  Comment(string s, address_t asm_ad = 0x0, address_t ir_ad = 0x0);
  virtual ~Comment() { };
  virtual void accept(IRVisitor *v) { v->visitComment(this); };
  virtual string tostring();
  virtual Comment *clone() const;
  string comment;
};

class ExpStmt: public Stmt {
 public:
  ExpStmt(Exp *e, address_t asm_ad = 0x0, address_t ir_ad = 0x0);
  ExpStmt(const ExpStmt &other);
  virtual ~ExpStmt() {};
  virtual string tostring();
  virtual void accept(IRVisitor *v) { v->visitExpStmt(this); };
  virtual ExpStmt *clone() const;
  Exp *exp;
};

class Call: public Stmt {
 public:
  Call(Exp *lval_opt, string fnname, vector<Exp*> params,
       address_t asm_ad = 0x0, address_t ir_ad = 0x0);
  Call(Exp *lval_opt, Exp *callee, vector<Exp *> params,
       address_t asm_ad = 0x0, address_t ir_ad = 0x0);

  Call(const Call &other);
  virtual ~Call() {};
  virtual string tostring();
  virtual void accept(IRVisitor *v) { v->visitCall(this); };
  virtual Call *clone() const;

  Exp* lval_opt;
  //  string fnname;
  Exp * callee;
  vector<Exp*> params;
};

class Return: public Stmt {
 public:
  Return(Exp *exp_opt,
         address_t asm_ad = 0x0, address_t ir_ad = 0x0);
  Return(const Return &other);
  virtual ~Return() {};
  virtual string tostring();
  virtual void accept(IRVisitor *v) { v->visitReturn(this); };
  virtual Return *clone() const;

  Exp* exp_opt;
};

class Func: public Stmt {
 public:
  Func(string fnname, bool has_rv, reg_t rt, 
           vector<VarDecl*> params, 
           bool external, vector<Stmt*> body,
           address_t asm_ad = 0x0, address_t ir_ad = 0x0);
  Func(const Func &other);
  virtual ~Func() {};
  virtual string tostring();
  virtual void accept(IRVisitor *v) { v->visitFunc(this); };
  virtual Func *clone() const;

  string fnname;
  bool has_rv;
  reg_t rt;
  vector<VarDecl*> params;
  bool external;
  vector<Stmt*> body;
};

class Assert: public Stmt {
 public:
  Assert(Exp *cond, address_t asm_ad = 0x0, address_t ir_ad = 0x0);
  Assert(const Assert &other);
  virtual ~Assert() {};
  virtual string tostring();
  virtual void accept(IRVisitor *v) { v->visitAssert(this); };
  virtual Assert* clone() const { return new Assert(*this); };

  Exp *cond;
};

string int_to_str( int i );
string int_to_hex( int i );
Label *mk_label();

extern "C" {
#endif // def __cplusplus
  extern stmt_type_t stmt_type(Stmt*);
  extern Exp* move_lhs(Stmt*);
  extern Exp* move_rhs(Stmt*);
  extern const char* label_string(Stmt*);
  extern TraceAttrs_t* stmt_attributes(Stmt*);
  extern threadid_t trace_tid(TraceAttrs_t*);
  extern int conc_map_size(TraceAttrs_t*);
  extern conc_map* get_cval(TraceAttrs_t*,int);
  extern const char* cval_name(conc_map*);
  extern big_val_t* cval_value(conc_map*);
  extern long cval_value_size(big_val_t*);
  extern const_val_t cval_value_part(big_val_t*, int);
  extern long cval_mem(conc_map*);
  extern const_val_t cval_ind(conc_map*);
  extern cval_type_t cval_type(conc_map*);
  extern int cval_usage(conc_map*);
  extern int cval_taint(conc_map*);
  extern void setAttribute(string);
  extern const char* asm_string_from_stmt(Stmt*);
  extern const char* special_string(Stmt*);
  extern const char* comment_string(Stmt*);
  extern Exp* jmp_target(Stmt*);
  extern Exp* cjmp_cond(Stmt*);
  extern Exp* cjmp_ttarget(Stmt*);
  extern Exp* cjmp_ftarget(Stmt*);
  extern Exp* expstmt_exp(Stmt*);
  extern const char* vardecl_name(Stmt*);
  extern reg_t vardecl_type(Stmt*);
  extern long call_has_lval(Stmt*);
  extern Exp* call_lval_opt(Stmt*);
  extern Exp* call_fnname(Stmt*);
  extern Exp** call_params(Stmt*);
  extern long ret_has_exp(Stmt *s);
  extern Exp* ret_exp(Stmt *s);
  extern const char* func_name(Stmt *s);
  extern long func_has_rv(Stmt *s);
  extern reg_t func_rt(Stmt *s);
  extern Stmt** func_params(Stmt *s);
  extern long func_is_external(Stmt *s);
  extern Stmt** func_body(Stmt *s);
  extern Exp* assert_cond(Stmt*);
#ifdef __cplusplus
}
#endif

#endif
