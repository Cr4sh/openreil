/*
 Owned and copyright BitBlaze, 2007. All rights reserved.
 Do not copy, disclose, or distribute without explicit written
 permission.
*/
#include "exp.h"
#include "stmt.h"
#include <iostream>
#include <map>
#include <cassert>
#include <cstdio>

using namespace std;

// Do NOT change this. It is used in producing XML output.
static string binopnames[] = {
  "PLUS",
  "MINUS",
  "TIMES",
  "DIVIDE",
  "MOD",
  "LSHIFT",
  "RSHIFT",
  "ARSHIFT",
  "LROTATE",
  "RROTATE",
  "LOGICAND",
  "LOGICOR",
  "BITAND",
  "BITOR",
  "XOR",
  "EQ",
  "NEQ",
  "GT",
  "LT",
  "GE",
  "LE",
  "SDIVIDE",
  "SMOD"
};
static string strs[] = {
  "+",
  "-",
  "*",
  "/",
  "%",
  "<<",
  ">>",
  "@>>",
  "<<",
  ">>",
  "&&",
  "||",
  "&",
  "|",
  "^",
  "==",
  "<>",
  ">",
  "<",
  ">=",
  "<=",
  "$/",
  "$%"
};

uint64_t
Exp::cast_value(reg_t t, uint64_t v)
{
  // Makes debugging easier actually assigning to correct type.
  uint64_t u64;
  uint32_t u32;
  uint16_t u16;
  uint8_t u8;
  switch(t){
  case REG_1: if(v == 0) return 0; else return 1; break;
  case REG_8: u8 = (uint8_t) v; return u8; break;
  case REG_16: u16 = (uint16_t) v; return u16; break;
  case REG_32: u32 = (uint32_t) v; return u32; break;
  case REG_64: u64 = (uint64_t) v; return u64; break;
  }
  assert(0); // eliminates warnings.
}

uint32_t 
Exp::reg_to_bits(const reg_t &reg)
{
  switch(reg)
    {
    case REG_1: return 1; break;
    case REG_8: return 8; break;
    case REG_16: return 16; break;
    case REG_32: return 32; break;
    case REG_64: return 64; break;
    }
  assert(0); // eliminates warnings.
}


Exp*
Exp::clone(Exp* copy)
{
  return copy->clone();
}


void Exp::destroy( Exp *expr )
{
    switch ( expr->exp_type )
    {
    case BINOP:     BinOp::destroy((BinOp *)expr);          break;
    case UNOP:      UnOp::destroy((UnOp *)expr);            break;
    case CONSTANT:  Constant::destroy((Constant *)expr);    break;
    case MEM:       Mem::destroy((Mem *)expr);              break;
    case TEMP:      Temp::destroy((Temp *)expr);            break;
    case PHI:       Phi::destroy((Phi *)expr);              break;
    case UNKNOWN:   Unknown::destroy((Unknown *)expr);      break;
    case CAST:      Cast::destroy((Cast *)expr);            break;
    case NAME:      Name::destroy((Name *)expr);            break;
    case LET:       Let::destroy((Let *)expr);              break; 
    case EXTENSION:   
      // Fixme: need to make a destroy virtual function that all
      // exp have. 
      fprintf(stderr, "warning: Memory lost on unhandled destroy"); break;
    }

}
string
Exp::string_type(const reg_t &typ)
{
  //  return string_type(typ.kind, typ.width);
  string s;
  switch(typ)
    {
    case REG_1: s= "reg1_t"; break;
    case REG_8: s = "reg8_t"; break;
    case REG_16: s = "reg16_t"; break;
    case REG_32: s = "reg32_t"; break;
    case REG_64: s = "reg64_t"; break;
    }
  return s;
}


BinOp::BinOp(binop_type_t t, Exp *l, Exp *r) 
  : Exp(BINOP), lhs(l), rhs(r), binop_type(t)
{ 

}

BinOp::BinOp(const BinOp& copy)
  : Exp(BINOP), binop_type(copy.binop_type)
{
  lhs = copy.lhs->clone();
  rhs = copy.rhs->clone();
}

BinOp *
BinOp::clone() const
{
  return new BinOp(*this);
}

string
BinOp::tostring() const
{
  string ret;
  ret =  lhs->tostring() + strs[binop_type] + rhs->tostring();
  ret = "(" + ret + ")";
  return ret;
}

string
BinOp::optype_to_string(const binop_type_t binop_type)
{
  return strs[binop_type];
}

string
BinOp::optype_to_name(const binop_type_t binop_type)
{
  return binopnames[binop_type];
}

binop_type_t
BinOp::string_to_optype(const string s)
{
  for(unsigned i = 0; i < sizeof(strs); i++){
    if(strs[i] == s)
      return (binop_type_t) i;
  }
  assert(1 == 0);
  return (binop_type_t) 0;
}

void BinOp::destroy( BinOp *expr )
{
    assert(expr);

    Exp::destroy(expr->lhs);
    Exp::destroy(expr->rhs);

    delete expr;
}

UnOp::UnOp(const UnOp& copy)
  : Exp(UNOP), unop_type(copy.unop_type)
{
  exp = copy.exp->clone();
}

UnOp::UnOp(unop_type_t typ, Exp *e)  
  : Exp(UNOP), unop_type(typ), exp(e)
{ 
}

UnOp *
UnOp::clone() const
{
  return new UnOp(*this);
}


string
UnOp::tostring() const
{
   string ret;
   switch(unop_type){
   case NEG:
     ret = "-" + exp->tostring();
    break;
   case NOT:
     ret = "!" + exp->tostring();
     break;
   }
   ret = "(" + ret + ")";
   return ret;
}

string
UnOp::optype_to_string(const unop_type_t op)
{
  // Do NOT change this. It is used in producing XML output.
  string ret;
  switch(op){
  case NEG:
    ret = "NEG"; break;
  case NOT:
    ret = "NOT"; break;
  }
  return ret;
}

unop_type_t
UnOp::string_to_optype(const string s)
{
  if(s == "NEG") return NEG;
  if(s == "NOT") return NOT;
  assert(1 == 0);
  return NEG;
}

void UnOp::destroy( UnOp *expr )
{
    assert(expr);

    Exp::destroy(expr->exp);

    delete expr;
}

/*
Mem::Mem(Exp *a)
  : Exp(MEM), addr(a)
{
  typ = REG_ADDRESS_T;
}
*/

Mem::Mem(Exp *a, reg_t t)
  : Exp(MEM), addr(a), typ(t)
{
}


Mem::Mem(const Mem& copy)
  : Exp(MEM)
{
  addr = copy.addr->clone();
  typ = copy.typ;
}

Mem *
Mem::clone() const
{
  return new Mem(*this);
}

string
Mem::tostring() const
{
  return "mem[" + addr->tostring() + "]";
// + Exp::string_type(typ);
  //os << "mem[" << addr->tostring() << "]";
}

void Mem::destroy( Mem *expr )
{
    assert(expr);

    Exp::destroy(expr->addr);

    delete expr;    
}

Constant::Constant(reg_t t, const_val_t v)
  : Exp(CONSTANT), typ(t), val(v)
{
}



Constant::Constant(const Constant& other)
  : Exp(CONSTANT),typ(other.typ), val(other.val)
{
}


Constant *
Constant::clone() const
{
  return new Constant(*this);
}

string
Constant::tostring() const
{
  ostringstream os;
  uint8_t u8;
  uint16_t u16;
  uint32_t u32;
  uint64_t u64;
  
  switch(typ){
  case REG_1: if(val == 0) os << "0"; else os << "1"; break;
  case REG_8: u8 = (uint8_t) val; os << dec << (int) u8; break;
  case REG_16: u16 = (uint16_t) val; os << u16; break;
  case REG_32: u32 = (uint32_t) val; os << u32; break;
  case REG_64: u64 = (uint64_t) val; os << u64; break;
  }
  os << ":" << Exp::string_type(typ);
  return os.str();
}

void Constant::destroy( Constant *expr )
{
    assert(expr);

    delete expr;
}

Constant Constant::t = Constant(REG_1,
				(const_val_t)1);
Constant Constant::f = Constant(REG_1,
				(const_val_t)0);


// ------------------------------------------------------------
// Class Phi
//         Phi functions
// ------------------------------------------------------------
Phi::Phi(const Phi& copy)
  : Exp(PHI)
{
  for(vector<Temp *>::const_iterator it = copy.vars.begin();
      it != copy.vars.end(); it++){
    Temp *t = *it;
    this->vars.push_back(t->clone());
  }
  phi_name = copy.phi_name;
}

Phi::Phi(string orig_name, vector<Temp*> v)
  : Exp(PHI), vars(v), phi_name(orig_name)
{ 
}

Phi *
Phi::clone() const
{
  return new Phi(*this);
}

string
Phi::tostring() const
{ 
  string ret = "PHI(";
  string comma = " ";
  for (vector<Temp *>::const_iterator it = vars.begin();
       it != vars.end(); it++) {
    ret += comma;
    ret += (*it)->tostring();
    comma = ",";
  }
  ret += " )";
  return ret;
}

void Phi::destroy( Phi *expr )
{
    assert(expr);

    unsigned int i;

    for ( i = 0; i < expr->vars.size(); i++ )
    {
        Exp::destroy(expr->vars.at(i));
    }

    delete expr;
}

Temp::Temp(reg_t t, string n) 
  : Exp(TEMP), typ(t), name(n)
{ }


Temp::Temp(const Temp &other)
  : Exp(TEMP), typ(other.typ), name(other.name)
{
}


Temp *
Temp::clone() const
{
  return new Temp(*this);
}

string 
Temp::tostring() const
{
  // Argh! Stop removing useful error checking and debugging information.
  // It doesn't hurt anyone.
  return name + ":" + Exp::string_type(typ);
  //return name;
}



void Temp::destroy( Temp *expr )
{
    assert(expr);

    delete expr;
}

Unknown::Unknown(string s, reg_t t) : Exp(UNKNOWN), str(s), typ(t)
{ }

Unknown::Unknown(const Unknown &other)
  : Exp(UNKNOWN), str(other.str), typ(other.typ)
{ }

Unknown *
Unknown::clone() const
{
  return new Unknown(*this);
}

void Unknown::destroy( Unknown *expr )
{
    assert(expr);

    delete expr;
}

Name::Name( string s ) : Exp(NAME), name(s)
{ 
  //typ = REG_ADDRESS_T;
}

Name::Name( const Name &other ) : Exp(NAME), name(other.name)
{ 
  //typ = REG_ADDRESS_T;
}

Name *
Name::clone() const
{
  return new Name(*this);
}

string Name::tostring() const
{
  //  return "name(L_" + name + ")";
  return "name(" + name + ")";
}

void Name::destroy( Name *expr )
{
    assert(expr);

    delete expr;
}

Cast::Cast( Exp *e, reg_t w, cast_t t ) 
  : Exp(CAST), exp(e), typ(w), cast_type(t)
{ }

Cast::Cast( const Cast &other ) 
  : Exp(CAST), typ(other.typ), cast_type(other.cast_type)
{
  exp = other.exp->clone();
}

Cast *
Cast::clone() const
{
  return new Cast(*this);
}

void Cast::destroy( Cast *expr )
{
    assert(expr);

    Exp::destroy(expr->exp);

    delete expr;
}

string Cast::tostring() const
{
  string wstr, tstr;

  wstr = Exp::string_type(this->typ);

  switch ( cast_type )
  {
    case CAST_UNSIGNED: tstr = "U"; break;
    case CAST_SIGNED:   tstr = "S"; break;
    case CAST_HIGH:     tstr = "H"; break;
    case CAST_LOW:      tstr = "L"; break;
    case CAST_FLOAT:    tstr = "F"; break;
    case CAST_INTEGER:  tstr = "I"; break;
    case CAST_RFLOAT:   tstr = "RF"; break;
    case CAST_RINTEGER: tstr = "RI"; break;
    default: 
      cout << "Unrecognized cast type" << endl;
      assert(0);
  }
  string ret = "cast(" + exp->tostring() + ")" + tstr + ":" + wstr;
  return ret;
}

string Cast::cast_type_to_string( const cast_t ctype )
{
  string  tstr;
  switch ( ctype )
  {
  //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // Do NOT change this. It is used in producing XML output.
  //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    case CAST_UNSIGNED: tstr = "Unsigned"; break;
    case CAST_SIGNED:   tstr = "Signed"; break;
    case CAST_HIGH:     tstr = "High"; break;
    case CAST_LOW:      tstr = "Low"; break;
    case CAST_FLOAT:    tstr = "Float"; break;
    case CAST_INTEGER:  tstr = "Integer"; break;
    case CAST_RFLOAT:   tstr = "ReinterpFloat"; break;
    case CAST_RINTEGER: tstr = "ReinterpInteger"; break;
    default:
      cout << "Unrecognized cast type" << endl;
      assert(0);
  }

  return tstr;
}

///////////////////////////// LET ////////////////////////////////
Let::Let(Exp *v, Exp *e, Exp *i) : Exp(LET), var(v), exp(e), in(i)
{
  /* empty */
}

Let *Let::clone() const
{

  return new Let(*this);
}


Let::Let(const Let &other) : Exp(LET)
{
  var = other.var->clone();
  exp = other.exp->clone();
  in = other.in->clone();
}

void
Let::destroy(Exp *exp)
{
  assert(LET == exp->exp_type);
  Let *let = (Let *)exp;
  Exp::destroy(let->var);
  Exp::destroy(let->exp);
  Exp::destroy(let->in);
  delete exp;
}

string
Let::tostring() const
{
  string s = "(let " + var->tostring() + " = " + exp->tostring()
    + " in " + in->tostring() + ")";
  return s;
}

//======================================================================
// 
// Shorthand functions for creating expressions
//
// Functions preceded with a _ are versions that do not make deep
// copies of their expression arguments before using them.
// 
//======================================================================

Exp *ecl( Exp *exp )
{
    assert(exp);
    return exp->clone();
}

Constant *ex_const(uint32_t value )
{
  return new Constant(REG_32, value);
}


Constant *ex_const(reg_t t, const_val_t value )
{
    return new Constant(t, value);
}


Name *ex_name( string name )
{
    return new Name(name);
}

UnOp *_ex_not( Exp *arg )
{
    return new UnOp(NOT, arg);
}

UnOp *ex_not( Exp *arg )
{
  arg = arg->clone();
    return _ex_not(arg);
}

BinOp *_ex_add( Exp *arg1, Exp *arg2 )
{
    return new BinOp(PLUS, arg1, arg2);
}

BinOp *ex_add( Exp *arg1, Exp *arg2 )
{
  arg1 = arg1->clone();
  arg2 = arg2->clone();
    return _ex_add(arg1, arg2);
}

BinOp *_ex_sub( Exp *arg1, Exp *arg2 )
{
    return new BinOp(MINUS, arg1, arg2);
}

BinOp *ex_sub( Exp *arg1, Exp *arg2 )
{
  arg1 = arg1->clone();
  arg2 = arg2->clone();
    return _ex_sub(arg1, arg2);
}

BinOp *_ex_mul( Exp *arg1, Exp *arg2 )
{
    return new BinOp(TIMES, arg1, arg2);
}

BinOp *ex_mul( Exp *arg1, Exp *arg2 )
{
  arg1 = arg1->clone();
  arg2 = arg2->clone();
    return _ex_mul(arg1, arg2);
}

BinOp *_ex_div( Exp *arg1, Exp *arg2 )
{
    return new BinOp(DIVIDE, arg1, arg2);
}

BinOp *ex_div( Exp *arg1, Exp *arg2 )
{
  arg1 = arg1->clone();
  arg2 = arg2->clone();
  return _ex_div(arg1, arg2);
}

BinOp *_ex_and( Exp *arg1, Exp *arg2 )
{
    return new BinOp(BITAND, arg1, arg2);
}

BinOp *_ex_and( Exp *arg1, Exp *arg2, Exp *arg3 )
{
    return _ex_and(arg1, _ex_and(arg2, arg3));
}

BinOp *_ex_and( Exp *arg1, Exp *arg2, Exp *arg3, Exp *arg4, Exp *arg5, Exp *arg6, Exp *arg7 )
{
    return _ex_and( _ex_and(arg1, arg2, arg3), _ex_and(arg4, arg5, arg6), arg7 );
}

BinOp *ex_and( Exp *arg1, Exp *arg2 )
{
  arg1 = arg1->clone();
  arg2 = arg2->clone();
    return _ex_and(arg1, arg2);
}

BinOp *ex_and( Exp *arg1, Exp *arg2, Exp *arg3 )
{
  arg1 = arg1->clone();
  arg2 = arg2->clone();
  arg3 = arg3->clone();
    return _ex_and(arg1, arg2, arg3);
}

BinOp *ex_and( Exp *arg1, Exp *arg2, Exp *arg3, Exp *arg4, Exp *arg5, Exp *arg6, Exp *arg7 )
{
  arg1 = arg1->clone();
  arg2 = arg2->clone();
  arg3 = arg3->clone();
  arg4 = arg4->clone();
  arg5 = arg5->clone();
  arg6 = arg6->clone();
  arg7 = arg7->clone();
    return _ex_and(arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}

BinOp *_ex_or( Exp *arg1, Exp *arg2 )
{
    return new BinOp(BITOR, arg1, arg2);
}

BinOp *_ex_or( Exp *arg1, Exp *arg2, Exp *arg3 )
{
    return _ex_or(arg1, _ex_or(arg2, arg3));
}

BinOp *_ex_or( Exp *arg1, Exp *arg2, Exp *arg3, Exp *arg4, Exp *arg5, Exp *arg6 )
{
    return _ex_or( _ex_or(arg1, arg2, arg3), _ex_or(arg4, arg5, arg6) );
}

BinOp *_ex_or( Exp *arg1, Exp *arg2, Exp *arg3, Exp *arg4, Exp *arg5, Exp *arg6, Exp *arg7 )
{
    return _ex_or( _ex_or(arg1, arg2, arg3), _ex_or(arg4, arg5, arg6), arg7 );
}

BinOp *ex_or( Exp *arg1, Exp *arg2 )
{
  arg1 = arg1->clone();
  arg2 = arg2->clone();
    return _ex_or(arg1, arg2);
}

BinOp *ex_or( Exp *arg1, Exp *arg2, Exp *arg3 )
{
    arg1 = arg1->clone();
    arg2 = arg2->clone();
    arg3 = arg3->clone();
    return _ex_or(arg1, arg2, arg3);
}

BinOp *ex_or( Exp *arg1, Exp *arg2, Exp *arg3, Exp *arg4, Exp *arg5, Exp *arg6 )
{
    arg1 = arg1->clone();
    arg2 = arg2->clone();
    arg3 = arg3->clone();
    arg4 = arg4->clone();
    arg5 = arg5->clone();
    arg6 = arg6->clone();
    return _ex_or(arg1, arg2, arg3, arg4, arg5, arg6);
}

BinOp *ex_or( Exp *arg1, Exp *arg2, Exp *arg3, Exp *arg4, Exp *arg5, Exp *arg6, Exp *arg7 )
{
    arg1 = arg1->clone();
    arg2 = arg2->clone();
    arg3 = arg3->clone();
    arg4 = arg4->clone();
    arg5 = arg5->clone();
    arg6 = arg6->clone();
    arg7 = arg7->clone();
    return _ex_or(arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}

BinOp *_ex_xor( Exp *arg1, Exp *arg2 )
{
    return new BinOp(XOR, arg1, arg2);
}

BinOp *_ex_xor( Exp *arg1, Exp *arg2, Exp *arg3 )
{
    return _ex_xor(arg1, _ex_xor(arg2, arg3));
}

BinOp *_ex_xor( Exp *arg1, Exp *arg2, Exp *arg3, Exp *arg4 )
{
    return _ex_xor( _ex_xor(arg1, arg2), _ex_xor(arg3, arg4) );
}

BinOp *_ex_xor( Exp *arg1, Exp *arg2, Exp *arg3, Exp *arg4,
                       Exp *arg5, Exp *arg6, Exp *arg7, Exp *arg8 )
{
    return _ex_xor( _ex_xor(arg1, arg2, arg3, arg4), _ex_xor(arg5, arg6, arg7, arg8) );
}

BinOp *ex_xor( Exp *arg1, Exp *arg2 )
{
    arg1 = arg1->clone();
    arg2 = arg2->clone();
    return _ex_xor(arg1, arg2);
}

BinOp *ex_xor( Exp *arg1, Exp *arg2, Exp *arg3 )
{
    arg1 = arg1->clone();
    arg2 = arg2->clone();
    arg3 = arg3->clone();
    return _ex_xor(arg1, arg2, arg3);
}

BinOp *_ex_shl( Exp *arg1, Exp *arg2 )
{
    return new BinOp(LSHIFT, arg1, arg2);
}

BinOp *ex_shl( Exp *arg1, Exp *arg2 )
{
    arg1 = arg1->clone();
    arg2 = arg2->clone();
    return new BinOp(LSHIFT, arg1, arg2);
}

BinOp *ex_shl( Exp *arg1, int arg2 )
{
    arg1 = arg1->clone();
    return new BinOp(LSHIFT, arg1, ex_const(arg2));
}

BinOp *_ex_shr( Exp *arg1, Exp *arg2 )
{
    return new BinOp(RSHIFT, arg1, arg2);
}

BinOp *ex_shr( Exp *arg1, Exp *arg2 )
{
    arg1 = arg1->clone();
    arg2 = arg2->clone();
    return new BinOp(RSHIFT, arg1, arg2);
}

BinOp *ex_shr( Exp *arg1, int arg2 )
{
    arg1 = arg1->clone();
    return new BinOp(RSHIFT, arg1, ex_const(arg2));
}

BinOp *_ex_sar( Exp *arg1, Exp *arg2 )
{
    return new BinOp(ARSHIFT, arg1, arg2);
}

BinOp *ex_sar( Exp *arg1, Exp *arg2 )
{
    arg1 = arg1->clone();
    arg2 = arg2->clone();
    return new BinOp(ARSHIFT, arg1, arg2);
}

BinOp *ex_sar( Exp *arg1, int arg2 )
{
    arg1 = arg1->clone();
    return new BinOp(ARSHIFT, arg1, ex_const(arg2));
}

BinOp *_ex_eq( Exp *arg1, Exp *arg2 )
{
    return new BinOp(EQ, arg1, arg2);
}

BinOp *ex_eq( Exp *arg1, Exp *arg2 )
{
    arg1 = arg1->clone();
    arg2 = arg2->clone();
    return new BinOp(EQ, arg1, arg2);
}

BinOp *_ex_neq( Exp *arg1, Exp *arg2 )
{
    return new BinOp(NEQ, arg1, arg2);
}

BinOp *ex_neq( Exp *arg1, Exp *arg2 )
{
    arg1 = arg1->clone();
    arg2 = arg2->clone();
    return new BinOp(NEQ, arg1, arg2);
}

BinOp *ex_gt( Exp *arg1, Exp *arg2 )
{
    arg1 = arg1->clone();
    arg2 = arg2->clone();
    return new BinOp(GT, arg1, arg2);
}

BinOp *ex_lt( Exp *arg1, Exp *arg2 )
{   
    arg1 = arg1->clone();
    arg2 = arg2->clone();
    return new BinOp(LT, arg1, arg2);
}

BinOp *ex_ge( Exp *arg1, Exp *arg2 )
{
    arg1 = arg1->clone();
    arg2 = arg2->clone();
    return new BinOp(GE, arg1, arg2);
}

BinOp *ex_le( Exp *arg1, Exp *arg2 )
{   
    arg1 = arg1->clone();
    arg2 = arg2->clone();
    return new BinOp(LE, arg1, arg2);
}

Cast *ex_u_cast( Exp *arg, reg_t width )
{
  arg = arg->clone();
  return new Cast(arg, width, CAST_UNSIGNED);
}

Cast *_ex_u_cast( Exp *arg, reg_t width )
{
    return new Cast(arg, width, CAST_UNSIGNED);
}

Cast *ex_s_cast( Exp *arg, reg_t width )
{
    arg = arg->clone();
    return new Cast(arg, width, CAST_SIGNED);
}

Cast *_ex_s_cast( Exp *arg, reg_t width )
{
    return new Cast(arg, width, CAST_SIGNED);
}

Cast *ex_h_cast( Exp *arg, reg_t width )
{
    arg = arg->clone();
    return new Cast(arg, width, CAST_HIGH);
}

Cast *ex_l_cast( Exp *arg, reg_t width )
{
    arg = arg->clone();
    return new Cast(arg, width, CAST_LOW);
}

Cast *_ex_l_cast( Exp *arg, reg_t width )
{
    return new Cast(arg, width, CAST_LOW);
}

Cast *ex_i_cast( Exp *arg, reg_t width )
{
    arg = arg->clone();
    return new Cast(arg, width, CAST_INTEGER);
}

Cast *ex_f_cast( Exp *arg, reg_t width )
{
    arg = arg->clone();
    return new Cast(arg, width, CAST_FLOAT);
}

Cast *ex_ri_cast( Exp *arg, reg_t width )
{
    arg = arg->clone();
    return new Cast(arg, width, CAST_RINTEGER);
}

Cast *ex_rf_cast( Exp *arg, reg_t width )
{
    arg = arg->clone();
    return new Cast(arg, width, CAST_RFLOAT);
}

