/*
 Owned and copyright BitBlaze, 2007. All rights reserved.
 Do not copy, disclose, or distribute without explicit written
 permission.
*/
#include "stmt.h"
#include <iostream>
#include <fstream>
#include <assert.h>

using namespace std;

Stmt *
Stmt::clone(Stmt *s)
{
  return s->clone();
}

void Stmt::destroy( Stmt *s )
{
    Move    *move       = NULL;
    Jmp     *jmp        = NULL;
    CJmp    *cjmp       = NULL;
    ExpStmt *expstmt    = NULL;
    Call    *call       = NULL;
    Return  *ret        = NULL;
    Func    *fun        = NULL;
    
    switch ( s->stmt_type )
    {
    case MOVE:    
      move = (Move *)s;
      Exp::destroy(move->lhs);
      Exp::destroy(move->rhs);
      break;
    case JMP:
      jmp = (Jmp *)s;
      Exp::destroy(jmp->target);
      break;
    case CJMP:
      cjmp = (CJmp *)s;
      Exp::destroy(cjmp->cond);
      Exp::destroy(cjmp->t_target);
      Exp::destroy(cjmp->f_target);
      break;
    case EXPSTMT:
      expstmt = (ExpStmt *)s;
      Exp::destroy(expstmt->exp);
      break;
    case CALL:
      call = (Call*)s;
      if (call->lval_opt != NULL)
        Exp::destroy(call->lval_opt);
      for(vector<Exp*>::iterator
            i=call->params.begin(); i!=call->params.end(); i++)
        Exp::destroy(*i);
      break;
    case RETURN:
      ret = (Return*) s;
      if (ret->exp_opt != NULL)
        Exp::destroy(ret->exp_opt);
      break;
    case FUNCTION:
      fun = (Func*) s;
      for(vector<VarDecl*>::iterator
            i=fun->params.begin(); i!=fun->params.end(); i++)
        Stmt::destroy(*i);
      for(vector<Stmt*>::iterator
            i=fun->body.begin(); i!=fun->body.end(); i++)
        Stmt::destroy(*i);
      break;
    case COMMENT:
    case SPECIAL:
    case LABEL:
    case VARDECL:
      break;
    }    

    delete s;
}

VarDecl::VarDecl(string n, reg_t t, address_t asm_ad,
		address_t ir_ad) 
  : Stmt(VARDECL, asm_ad, ir_ad), name(n), typ(t)
{
}

VarDecl::VarDecl(const VarDecl &other) : 
  Stmt(VARDECL, other.asm_address, other.ir_address), 
  name(other.name), typ(other.typ)
{
}

VarDecl::VarDecl(Temp *t) : Stmt(VARDECL, 0x0, 0x0),
			    name(t->name),
			    typ(t->typ)
{
}


string 
VarDecl::tostring()
{
  string ret = "var " + name + ":" + Exp::string_type(this->typ) + ";";
  return ret;
}

VarDecl *
VarDecl::clone() const
{
  return new VarDecl(*this);
}


string 
Move::tostring() 
{
    return lhs->tostring() + " = " + rhs->tostring() + ";";
}

Move::Move(const Move &other) 
  : Stmt(MOVE, other.asm_address, other.ir_address)
{
  this->lhs = other.lhs->clone();
  this->rhs = other.rhs->clone();
}

Move::Move(Exp *l, Exp *r, address_t asm_addr, address_t ir_addr) :  
  Stmt(MOVE, asm_addr, ir_addr), lhs(l), rhs(r)
{  
}


Move *
Move::clone() const
{
  return new Move(*this);
}

Label::Label(const Label &other) 
  : Stmt(LABEL, other.asm_address, other.ir_address)
{
  this->label = string(other.label);
}

Label::Label(string l, address_t asm_addr, address_t ir_addr)  
  : Stmt(LABEL,asm_addr, ir_addr)
{ label = l; }


Label *
Label::clone() const
{
  return new Label(*this);
}

string
Label::tostring()
{
  //  return "label L_" + label + ":";
  return "label " + label + ":";
}


Jmp::Jmp(Exp *e, address_t asm_addr, address_t ir_addr) : 
  Stmt(JMP, asm_addr, ir_addr), target(e)
{  }

Jmp::Jmp(const Jmp &other) 
  : Stmt(JMP, other.asm_address, other.ir_address)
{
  target = other.target->clone();
}

Jmp *
Jmp::clone() const
{
  return new Jmp(*this);
}



string Jmp::tostring() {
  string ret = "jmp(" + target->tostring() + ");";
  return ret;
}


CJmp::CJmp(Exp *c, Exp *t, Exp *f, address_t asm_addr, address_t ir_addr) 
  : Stmt(CJMP, asm_addr, ir_addr), cond(c), t_target(t), f_target(f)
{  }


CJmp::CJmp(const CJmp &other) 
  : Stmt(CJMP, other.asm_address, other.ir_address)
{
  cond = other.cond->clone();
  f_target = other.f_target->clone();
  t_target = other.t_target->clone();
}

CJmp *
CJmp::clone() const
{
  return new CJmp(*this);
}

string 
CJmp::tostring() 
{
	string ret = "cjmp(" + cond->tostring() + "," + 
	
	t_target->tostring() + "," + f_target->tostring() + ");";
	
    return ret;
}

Special::Special(string s, address_t asm_addr, address_t ir_addr) 
  : Stmt(SPECIAL, asm_addr, ir_addr), special(s)
{  }

Special::Special(const Special &other) 
  : Stmt(SPECIAL, other.asm_address, other.ir_address)
{
  special = other.special;
}

Special *
Special::clone() const
{
  return new Special(*this);
}

string Special::tostring() {
  string ret = "special(\"" + special + "\");";
  return ret;
}


Comment::Comment(string s, address_t asm_addr, address_t ir_addr) 
  : Stmt(COMMENT, asm_addr, ir_addr)
{ comment = s; }

Comment::Comment(const Comment &other) 
  : Stmt(COMMENT, other.asm_address, other.ir_address)
{
  comment = other.comment;
}


Comment *
Comment::clone() const
{
  return new Comment(*this);
}

string Comment::tostring() {
  string s = "//" + string(comment);
  return s;
}

ExpStmt::ExpStmt(Exp *e, address_t asm_addr, address_t ir_addr) 
  : Stmt(EXPSTMT, asm_addr, ir_addr)
{
  exp =e ;
}

ExpStmt::ExpStmt(const ExpStmt &other) 
  : Stmt(EXPSTMT, other.asm_address, other.ir_address)
{
  exp = other.exp->clone();
}

ExpStmt *
ExpStmt::clone() const
{
  return new ExpStmt(*this);
}

string
ExpStmt::tostring(){
  string s = exp->tostring() + ";";
  return s;
}

Call::Call(Exp *lval_opt, string fnname, vector<Exp*> params,
       address_t asm_ad, address_t ir_ad)
  : Stmt(CALL, asm_ad, ir_ad)
{
  this->lval_opt = lval_opt;
  this->callee = new Name(fnname);
  this->params = params;
}

Call::Call(Exp *lval_opt, Exp *callee, vector<Exp *> params,
	   address_t asm_ad, address_t ir_ad)
  : Stmt(CALL,asm_ad, ir_ad)
{
  this->lval_opt = lval_opt;
  this->callee = callee;
  this->params = params;
}

Call::Call(const Call &other)
  : Stmt(CALL, other.asm_address, other.ir_address)
{
  this->lval_opt = (other.lval_opt == NULL) ? NULL : other.lval_opt->clone();
  assert(other.callee);
  this->callee = other.callee->clone();
  this->params.clear();

  for(vector<Exp*>::const_iterator 
        i = other.params.begin(); i != other.params.end(); i++) {
    this->params.push_back((*i)->clone());
  }
}

string Call::tostring()
{
  ostringstream ostr;
  Name *name;
  if(this->lval_opt != NULL)
    ostr << this->lval_opt->tostring() << " = ";
  
  if(this->callee->exp_type == NAME){
    name = (Name *) this->callee;
    ostr << name->name;
  } else {
    ostr << "call " << this->callee->tostring();
  }
  ostr << "(";
  for(vector<Exp*>::iterator
        i=this->params.begin(); i != this->params.end(); i++) {
    ostr << (*i)->tostring();
    if ((i+1) != this->params.end())
      ostr << ", ";
  }
  ostr << ");";
  string str = ostr.str();
  return str;
}

Call* Call::clone() const
{
  return new Call(*this);
}


Return::Return(Exp *exp_opt,
               address_t asm_ad, address_t ir_ad)
  : Stmt(RETURN, asm_ad, ir_ad)
{
  this->exp_opt = exp_opt;
}

Return::Return(const Return &other)
  : Stmt(RETURN, other.asm_address, other.ir_address)
{
  this->exp_opt = (other.exp_opt == NULL) ? NULL : other.exp_opt->clone();
}

string Return::tostring()
{
  ostringstream ostr;

  ostr << "return";

  if(this->exp_opt != NULL)
    ostr << " " << this->exp_opt->tostring();

  ostr << ";";

  return ostr.str();
}

Return* Return::clone() const
{
  return new Return(*this);
}

Func::Func(string fnname, bool has_rv, reg_t rt, 
                   vector<VarDecl*> params, 
                   bool external, vector<Stmt*> body,
                   address_t asm_ad, address_t ir_ad)
  : Stmt(FUNCTION, asm_ad, ir_ad)
{
  this->fnname = fnname;
  this->has_rv = has_rv;
  this->rt = rt;
  this->params = params;
  this->external = external;
  this->body = body;
}

Func::Func(const Func &other)
  : Stmt(FUNCTION, other.asm_address, other.ir_address)
{
  this->fnname = other.fnname;
  this->has_rv = other.has_rv;
  this->rt = other.rt;
  this->params.clear();
  for(vector<VarDecl*>::const_iterator
        i=other.params.begin(); i!=other.params.end(); i++)
    this->params.push_back((*i)->clone());
  this->external = other.external;
  this->body.clear();
  for(vector<Stmt*>::const_iterator
        i=other.body.begin(); i!=other.body.end(); i++)
    this->body.push_back((*i)->clone());
}

string Func::tostring()
{
  ostringstream ostr;
  if (external)
    ostr << "extern ";

  if (has_rv) 
    ostr << Exp::string_type(rt) << " ";
  else
    ostr << "void ";
  
  //ostr << fnname;

  ostr << this->fnname << "(";
  for(vector<VarDecl*>::iterator
        i=this->params.begin(); i != this->params.end(); i++) {
    ostr << (*i)->tostring();
    if ((i+1) != this->params.end())
      ostr << ", ";
  }
  ostr << ")";

  if (this->body.empty()) {
    ostr << ";";
  } else {
    ostr << "\n";
    ostr << "{\n";
    for(vector<Stmt*>::iterator
          i=this->body.begin(); i != this->body.end(); i++) {
      ostr << "\t" << (*i)->tostring() << endl;
    }
    ostr << "}";
  }

  return ostr.str();
}

Func* Func::clone() const
{
  return new Func(*this);
}




Assert::Assert(Exp *cond, address_t asm_ad, address_t ir_ad)
  : Stmt(ASSERT, asm_ad, ir_ad), cond(cond)
{ }

Assert::Assert(const Assert &other)
  : Stmt(ASSERT, other.asm_address, other.ir_address) 
{
  cond = other.cond->clone();
}

string Assert::tostring()
{
  return "assert(" + cond->tostring() + ");";
}
 



//----------------------------------------------------------------------
// Convert int to std::string in decimal form
//----------------------------------------------------------------------
string int_to_str( int i )
{
    ostringstream stream;
    stream << i << flush;
    return (stream.str());
}

//----------------------------------------------------------------------
// Convert int to std::string in hex form
//----------------------------------------------------------------------
string int_to_hex( int i )
{
    ostringstream stream;
    stream << hex << i << flush;
    return (stream.str());
}

//----------------------------------------------------------------------
// Generate a unique label, this is done using a static counter
// internal to the function.
//----------------------------------------------------------------------
Label *mk_label()
{
    static int label_counter = 0;
    return new Label("L_" + int_to_str(label_counter++) );
}


