#include <string>
#include <vector>
#include <iostream>
#include <assert.h>

//#include "typecheck_ir.h"
#include "irtoir-internal.h"
#include "config.h"

#if VEX_VERSION >= 1793
#define Ist_MFence Ist_MBE
#endif

// flag to disable warnings (for unit test's sake)
bool print_warnings = 1;

// enable/disable lazy eflags computation.
// this is for transitional purposes, and should be removed soon.
bool use_eflags_thunks = 0;

// Use R_XS_BASE registers instead of gdt/ldt
bool use_simple_segments = 1;

bool translate_calls_and_returns = 0;

// Guest architecture we are translating from.
// Set in generate_bap_ir, accessed all over...
// It might be cleaner to pass this around, but that would require a lot of
// refactoring.
VexArch guest_arch = VexArch_INVALID;

using namespace std;

//
// For labeling untranslated VEX IR instructions
//
static string uTag = "Unknown: ";
static string sTag = "Skipped: ";

//
// Special Exp to record the AST for shl, shr
//

Exp * count_opnd = NULL;

//======================================================================
// Forward declarations
//======================================================================
Exp *emit_mux0x( vector<Stmt *> *irout, reg_t type, Exp *cond, Exp *exp0, Exp *expX );
void modify_flags( asm_program_t *prog, bap_block_t *block );
//string inst_to_str(asm_program_t *prog, Instruction *inst );
//static void add_special_returns(bap_block_t *block);
static void insert_specials(bap_block_t * block);
void do_cleanups_before_processing();


//======================================================================
// 
// Helper functions for output (generally should only be used by automated
// testing)
//
//======================================================================
void asmir_set_print_warning(bool value) {
  print_warnings = value;
}

bool asmir_get_print_warning() {
  return print_warnings;
}
//======================================================================
// 
// Helper functions for the translation
//
//======================================================================

/// Set whether to use the thunk code with function calls, or not.
void set_use_eflags_thunks(bool value){
  use_eflags_thunks = value;
}

/// Set whether to use code with simple segments or not.
void asmir_set_use_simple_segments(bool value){
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
  cerr <<"Warning: set_call_return_translation() is deprecated. Use replace_calls_and_returns instead.\n";
  translate_calls_and_returns = (bool) value;
}

//----------------------------------------------------------------------
// A panic function that prints a msg and terminates the program
//----------------------------------------------------------------------
void panic( string msg )
{
  ostringstream os;
  os << "Panic: " << msg;
  throw os.str().c_str();
  /*
    cerr << "Panic: " << msg << endl;
    exit(1);
  */
}


//---------------------------------------------------------------------
// Helper wrappers around arch specific functions
//---------------------------------------------------------------------

vector<VarDecl *> get_reg_decls(VexArch arch)
{
  switch (arch) {
  case VexArchX86:
    return i386_get_reg_decls();
  case VexArchARM:
    return arm_get_reg_decls();
  default:
    panic("irtoir.cpp: translate_get: unsupported arch");
  }
}

vector<VarDecl *> get_reg_decls(void) {
  return get_reg_decls(guest_arch);
}


Exp *translate_get( IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout )
{
    assert(expr);
    assert(irbb);
    assert(irout);

    switch (guest_arch) {
    case VexArchX86:
      return i386_translate_get(expr, irbb, irout);
    case VexArchARM:
      return arm_translate_get(expr, irbb, irout);
    default:
      panic("irtoir.cpp: translate_get: unsupported arch");
    }
}

Stmt *translate_put( IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout )
{
    switch (guest_arch) {
    case VexArchX86:
      return i386_translate_put(stmt, irbb, irout);
    case VexArchARM:
      return arm_translate_put(stmt, irbb, irout);
    default:
      panic("translate_put");
    }
}

Exp *translate_ccall( IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout )
{
    assert(expr);
    assert(irbb);
    assert(irout);

    switch (guest_arch) {
    case VexArchX86:
      return i386_translate_ccall(expr, irbb, irout);
     case VexArchARM:
      return arm_translate_ccall(expr, irbb, irout);
   default:
      panic("translate_ccall");
    }
}

void modify_flags( asm_program_t *prog, bap_block_t *block )
{
    assert(block);
    switch (guest_arch) {
    case VexArchX86:
      return i386_modify_flags(prog, block);
    case VexArchARM:
      return arm_modify_flags(prog, block);
    default:
      panic("modify_flags");
    }
}




// Note:
//  VEX uses IRType to specify both the width(in terms of bits) and
//  type(signed/unsigned/float) of an expression. To translate an
//  IRType, we use get_reg_width() and get_reg_type() to extract
//  these two aspects of the expression type from the IRType.

reg_t IRType_to_reg_type( IRType type )
{
    reg_t t;

    switch ( type )
    {
    case Ity_I1:    t = REG_1; break;
    case Ity_I8:    t = REG_8; break; 
    case Ity_I16:   t = REG_16; break;
    case Ity_I32:   t = REG_32; break;
    case Ity_I64:   t = REG_64; break;
    case Ity_F32:   
      if(print_warnings) { 
	fprintf(stderr, "warning: Float32 register encountered\n");
      } 
      t = REG_32; break;
    case Ity_F64:   
      if(print_warnings) { 
	fprintf(stderr, "warning: Float64 register encountered\n");
      }
      t = REG_64; break;
    default:
      throw "Unknown IRType";
    }

    return t;
}

reg_t regt_of_irexpr(IRSB *irbb, IRExpr *e)
{
  return IRType_to_reg_type(typeOfIRExpr(irbb->tyenv, e));
}

Temp *mk_temp( string name, IRType ty )
{
    reg_t typ = IRType_to_reg_type(ty);
    Temp *ret = new Temp( typ, "T_" + name );
    return ret;
}

Temp *mk_temp( reg_t type, vector<Stmt *> *stmts )
{
    static int temp_counter = 0;
    Temp *ret =  new Temp( type, "T_" + int_to_str(temp_counter++) );
    stmts->push_back(new VarDecl(ret));
    return ret;
}


Temp *mk_temp( IRType ty, vector<Stmt *> *stmts )
{
    reg_t typ = IRType_to_reg_type(ty);
    return mk_temp(typ, stmts);
}

//----------------------------------------------------------------------
// Takes a destination address and makes a Label out of it. 
// Note that this function and mk_dest_name must produce the same
// string for the same given address!
//----------------------------------------------------------------------
Label *mk_dest_label( Addr64 dest )
{
    return new Label("pc_0x" + int_to_hex(dest));
}

//----------------------------------------------------------------------
// Takes a destination address and makes a Name out of it. 
// Note that this function and mk_dest_label must produce the same
// string for the same given address!
//----------------------------------------------------------------------
Name *mk_dest_name( Addr64 dest )
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
Exp *translate_32HLto64( Exp *arg1, Exp *arg2 )
{
    assert(arg1);
    assert(arg2);

    Exp *high = new Cast( arg1, REG_64, CAST_UNSIGNED );
    Exp *low = new Cast( arg2, REG_64, CAST_UNSIGNED );

    high = new BinOp( LSHIFT, high, ex_const(REG_64,32) );

    return new BinOp( BITOR, high, low );
}
Exp *translate_64HLto64( Exp *high, Exp *low )
{
    assert(high);
    assert(low);

    high = new BinOp( LSHIFT, high, ex_const(REG_64, 32) );
    high = new BinOp( BITAND, high, ex_const(REG_64, 0xffffffff00000000LL) );
    low = new BinOp( BITAND, low, ex_const(REG_64, 0xffffffffL) );

    return new BinOp( BITOR, high, low );
}


Exp *translate_DivModU64to32( Exp *arg1, Exp *arg2 )
{
    assert(arg1);
    assert(arg2);
    arg2 = new Cast( arg2, REG_64, CAST_UNSIGNED );
    Exp *div = new BinOp( DIVIDE, arg1, arg2 );
    Exp *mod = new BinOp( MOD, ecl(arg1), ecl(arg2) );

    return translate_64HLto64( mod, div );
}

Exp *translate_DivModS64to32( Exp *arg1, Exp *arg2 )
{
    assert(arg1);
    assert(arg2);
    arg2 = new Cast( arg2, REG_64, CAST_SIGNED );
    Exp *div = new BinOp( SDIVIDE, arg1, arg2 );
    Exp *mod = new BinOp( SMOD, ecl(arg1), ecl(arg2) );

    return translate_64HLto64( mod, div );
}

Exp *translate_MullS8( Exp *arg1, Exp *arg2 )
{
    assert(arg1);
    assert(arg2);

    Exp *wide1 = new Cast( arg1, REG_16, CAST_SIGNED );
    Exp *wide2 = new Cast( arg2, REG_16, CAST_SIGNED );

    return new BinOp( TIMES, wide1, wide2 );
}

Exp *translate_MullU32( Exp *arg1, Exp *arg2 )
{
    assert(arg1);
    assert(arg2);

    Exp *wide1 = new Cast( arg1, REG_64, CAST_UNSIGNED );
    Exp *wide2 = new Cast( arg2, REG_64, CAST_UNSIGNED );
    
    return new BinOp( TIMES, wide1, wide2 );
}

Exp *translate_MullS32( Exp *arg1, Exp *arg2 )
{
    assert(arg1);
    assert(arg2);

    Exp *wide1 = new Cast( arg1, REG_64, CAST_SIGNED );
    Exp *wide2 = new Cast( arg2, REG_64, CAST_SIGNED );

    return new BinOp( TIMES, wide1, wide2 );
}

Exp *translate_Clz32( IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout )
{
    assert(expr);
    assert(irbb);
    assert(irout);

    Exp *arg = translate_expr( expr->Iex.Unop.arg, irbb, irout );

    Temp *counter = mk_temp( Ity_I32, irout );
    Temp *temp = mk_temp( typeOfIRExpr(irbb->tyenv, expr->Iex.Unop.arg),irout );

    Label *loop = mk_label();
    Label *label0 = mk_label();
    Label *label1 = mk_label();

    Exp *cond = new BinOp( NEQ, temp, ex_const(0) );

    irout->push_back( new Move( new Temp(*counter), ex_const(32) ) );
    irout->push_back( new Move( new Temp(*temp), arg ) );
    irout->push_back( loop );
    irout->push_back( new CJmp( cond, new Name(label0->label), new Name(label1->label) ) );
    irout->push_back( label0 );
    irout->push_back( new Move( new Temp(*temp), new BinOp( RSHIFT, new Temp(*temp), ex_const(1) ) ) );
    irout->push_back( new Move( new Temp(*counter), new BinOp( MINUS, new Temp(*counter), ex_const(1) ) ) );
    irout->push_back( new Jmp( new Name(loop->label) ) );
    irout->push_back( label1 );

    return counter;
}

Exp *translate_Ctz32( IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout )
{
    assert(expr);
    assert(irbb);
    assert(irout);

    Exp *arg = translate_expr( expr->Iex.Unop.arg, irbb, irout );

    Temp *counter = mk_temp( Ity_I32, irout );
    Temp *temp = mk_temp( typeOfIRExpr(irbb->tyenv, expr->Iex.Unop.arg),irout );

    Label *loop = mk_label();
    Label *label0 = mk_label();
    Label *label1 = mk_label();

    Exp *cond = new BinOp( NEQ, temp, ex_const(0) );

    irout->push_back( new Move( new Temp(*counter), ex_const(32) ) );
    irout->push_back( new Move( new Temp(*temp), arg ) );
    irout->push_back( loop );
    irout->push_back( new CJmp( cond, new Name(label0->label), new Name(label1->label) ) );
    irout->push_back( label0 );
    irout->push_back( new Move( new Temp(*temp), new BinOp( LSHIFT, new Temp(*temp), ex_const(1) ) ) );
    irout->push_back( new Move( new Temp(*counter), new BinOp( MINUS, new Temp(*counter), ex_const(1) ) ) );
    irout->push_back( new Jmp( new Name(loop->label) ) );
    irout->push_back( label1 );

    return counter;
}

Exp *translate_CmpF64( IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout )
{
    assert(expr);
    assert(irbb);
    assert(irout); 

    Exp *arg1 = translate_expr(expr->Iex.Binop.arg1, irbb, irout);
    Exp *arg2 = translate_expr(expr->Iex.Binop.arg2, irbb, irout);

    Exp *condEQ = new BinOp( EQ, arg1, arg2 );
    Exp *condGT = new BinOp( GT, ecl(arg1), ecl(arg2) );
    Exp *condLT = new BinOp( LT, ecl(arg1), ecl(arg2) );

    Label *labelEQ = mk_label();
    Label *labelGT = mk_label();
    Label *labelLT = mk_label();
    Label *labelUN = mk_label();
    Label *labelNext0 = mk_label();
    Label *labelNext1 = mk_label();
    Label *done = mk_label();

    Temp *temp = mk_temp( Ity_I32,irout );
   
    irout->push_back( new CJmp( condEQ, new Name(labelEQ->label), new Name(labelNext0->label) ) );
    irout->push_back( labelNext0 );
    irout->push_back( new CJmp( condGT, new Name(labelGT->label), new Name(labelNext1->label) ) );
    irout->push_back( labelNext1 );
    irout->push_back( new CJmp( condLT, new Name(labelLT->label), new Name(labelUN->label) ) );
    irout->push_back( labelUN );
    irout->push_back( new Move( new Temp(*temp), ex_const( 0x45 ) ) );
    irout->push_back( new Jmp( new Name(done->label) ) );
    irout->push_back( labelEQ );
    irout->push_back( new Move( new Temp(*temp), ex_const( 0x40 ) ) );
    irout->push_back( new Jmp( new Name(done->label) ) );
    irout->push_back( labelGT );
    irout->push_back( new Move( new Temp(*temp), ex_const( 0x00 ) ) );
    irout->push_back( new Jmp( new Name(done->label) ) );
    irout->push_back( labelLT );
    irout->push_back( new Move( new Temp(*temp), ex_const( 0x01 ) ) );
    irout->push_back( done );

    return temp;
}

Exp *translate_const( IRExpr *expr )
{
    assert(expr);

    IRConst *co = expr->Iex.Const.con;

    const_val_t value;
    reg_t width;

    switch ( co->tag )
      {
        // Your typical unsigned ints
        case Ico_U1:    width = REG_1;    value = co->Ico.U1;     break;
        case Ico_U8:    width = REG_8;    value = co->Ico.U8;     break;
        case Ico_U16:   width = REG_16;   value = co->Ico.U16;    break;
        case Ico_U32:   width = REG_32;   value = co->Ico.U32;    break;
        case Ico_U64:   width = REG_64;   value = co->Ico.U64;    break;

        // Not sure what the diff here is, VEX comments say F64 is IEEE754 floating
        // and F64i is 64 bit unsigned int interpreted literally at IEEE754 double
      case Ico_F64: //  width = I64;   value.floatval = co->Ico.F64;   kind = REG_FLOAT; break;
      case Ico_F64i: // width = I64;   value.floatval = co->Ico.F64i;  kind = REG_FLOAT; break; 
	//return new Unknown("Untranslatable float constant.");
 	                width = REG_64;   value = co->Ico.F64i;   break;
	
        // Dunno what this is for, so leave it unrecognized
      case Ico_V128:
            panic("Unsupported constant type (Ico_V128)");
      default:
            panic("Unrecognized constant type");
    }

    Constant *result = new Constant(width, value);

    return result;
}

Exp *translate_simple_unop( IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout )
{

    Exp *arg = translate_expr( expr->Iex.Unop.arg, irbb, irout );

    switch ( expr->Iex.Unop.op )
    {

        case Iop_Not8:
	case Iop_Not16:
	case Iop_Not32:
	case Iop_Not64:    return new UnOp( NOT, arg );
#if VEX_VERSION < 1770
        case Iop_Neg8:
	case Iop_Neg16:
	case Iop_Neg32:
	case Iop_Neg64:    return new UnOp( NEG, arg ); 
#endif
          //        case Iop_NegF64:                return new UnOp( NEG, arg );
        case Iop_NegF64:
          return new Unknown("NegF64", REG_64);
        case Iop_Not1:                  return new UnOp( NOT, arg );

        case Iop_8Uto16:    return new Cast( arg, REG_16, CAST_UNSIGNED );
        case Iop_8Sto16:    return new Cast( arg, REG_16, CAST_SIGNED );
        case Iop_8Uto32:    return new Cast( arg, REG_32, CAST_UNSIGNED );
        case Iop_8Sto32:    return new Cast( arg, REG_32, CAST_SIGNED );
        case Iop_8Uto64:    return new Cast( arg, REG_64, CAST_UNSIGNED );
        case Iop_8Sto64:    return new Cast( arg, REG_64, CAST_SIGNED );
        case Iop_16Uto32:   return new Cast( arg, REG_32, CAST_UNSIGNED );
        case Iop_16Sto32:   return new Cast( arg, REG_32, CAST_SIGNED );
        case Iop_16Uto64:   return new Cast( arg, REG_64, CAST_UNSIGNED );
        case Iop_16Sto64:   return new Cast( arg, REG_64, CAST_SIGNED );
        case Iop_16to8:     return new Cast( arg, REG_8,  CAST_LOW);
        case Iop_16HIto8:   return new Cast( arg, REG_8,  CAST_HIGH);
        case Iop_32Sto64:   return new Cast( arg, REG_64, CAST_SIGNED );
        case Iop_32Uto64:   return new Cast( arg, REG_64, CAST_UNSIGNED );
        case Iop_32to8:     return new Cast( arg, REG_8,  CAST_LOW );
        case Iop_32to16:    return new Cast( arg, REG_16, CAST_LOW );
        case Iop_64to8:     return new Cast( arg, REG_8,  CAST_LOW );
        case Iop_64to16:    return new Cast( arg, REG_16, CAST_LOW );
        case Iop_64to32:    return new Cast( arg, REG_32, CAST_LOW );
        case Iop_64HIto32:  return new Cast( arg, REG_32, CAST_HIGH );
        case Iop_32to1:     return new Cast( arg, REG_1,  CAST_LOW );
        case Iop_64to1:     return new Cast( arg, REG_1,  CAST_LOW );
        case Iop_1Uto8:     return new Cast( arg, REG_8,  CAST_UNSIGNED );
        case Iop_1Uto32:    return new Cast( arg, REG_32, CAST_UNSIGNED );
        case Iop_1Uto64:    return new Cast( arg, REG_64, CAST_UNSIGNED );
        case Iop_1Sto8:     return new Cast( arg, REG_8,  CAST_SIGNED );
        case Iop_1Sto16:    return new Cast( arg, REG_16, CAST_SIGNED );
        case Iop_1Sto32:    return new Cast( arg, REG_32, CAST_SIGNED );
        case Iop_1Sto64:    return new Cast( arg, REG_64, CAST_SIGNED );
//         case Iop_F32toF64:  return new Cast( arg, REG_64, CAST_FLOAT );
//         case Iop_I32toF64:  return new Cast( arg, REG_64, CAST_FLOAT );

//         case Iop_ReinterpI64asF64:  return new Cast( arg, REG_64, CAST_RFLOAT );
//         case Iop_ReinterpF64asI64:  return new Cast( arg, REG_64, CAST_RINTEGER );
            /*   case Iop_F32toF64: 
        case Iop_I32toF64:
        case Iop_ReinterpI64asF64:
        case Iop_ReinterpF64asI64:
        return new Unknown("floatcast", REG_64); */

        default:
            break;
    }

    return NULL;
}

Exp *translate_unop( IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout )
{
    assert(irbb);
    assert(expr);
    assert(irout);

    Exp *result;

    if ( (result = translate_simple_unop(expr, irbb, irout)) != NULL )
        return result;

    switch ( expr->Iex.Unop.op )
    {
        
        case Iop_Clz32:     return translate_Clz32( expr, irbb, irout );
        case Iop_Ctz32:     return translate_Ctz32( expr, irbb, irout );

        case Iop_AbsF64:
	  return new Unknown("Floating point op", REG_64);

        default:    
            throw "Unrecognized unary op";
    }

    return NULL;
}

Exp *translate_simple_binop( IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout )
{
    Exp *arg1 = translate_expr(expr->Iex.Binop.arg1, irbb, irout);
    Exp *arg2 = translate_expr(expr->Iex.Binop.arg2, irbb, irout);
    
    switch ( expr->Iex.Binop.op ) 
    {
        case Iop_Add8:
	case Iop_Add16:
	case Iop_Add32:
	case Iop_Add64:        return new BinOp(PLUS, arg1, arg2);
        case Iop_Sub8:
	case Iop_Sub16:
	case Iop_Sub32: 
	case Iop_Sub64:        return new BinOp(MINUS, arg1, arg2);
        case Iop_Mul8:
	case Iop_Mul16:
	case Iop_Mul32:
	case Iop_Mul64:        return new BinOp(TIMES, arg1, arg2);
        case Iop_Or8:
	case Iop_Or16:
	case Iop_Or32:
	case Iop_Or64:          return new BinOp(BITOR, arg1, arg2);
        case Iop_And8:
	case Iop_And16:
	case Iop_And32:
	case Iop_And64:        return new BinOp(BITAND, arg1, arg2);
        case Iop_Xor8:
	case Iop_Xor16:
	case Iop_Xor32:
	case Iop_Xor64:        return new BinOp(XOR, arg1, arg2);
    case Iop_Shl8:
        if (!count_opnd && !use_eflags_thunks)  count_opnd = arg2;
        return new BinOp(LSHIFT, arg1, new Cast(arg2, REG_8, CAST_UNSIGNED));
        break;
	case Iop_Shl16:
        if (!count_opnd && !use_eflags_thunks)  count_opnd = arg2;
        return new BinOp(LSHIFT, arg1, new Cast(arg2, REG_16, CAST_UNSIGNED));
        break;
	case Iop_Shl32:
        if (!count_opnd && !use_eflags_thunks)  count_opnd = arg2;
        return new BinOp(LSHIFT, arg1, new Cast(arg2, REG_32, CAST_UNSIGNED));
        break;
    case Iop_Shl64:
        if (!count_opnd && !use_eflags_thunks)  count_opnd = arg2;
        return new BinOp(LSHIFT, arg1, new Cast(arg2, REG_64, CAST_UNSIGNED));
        break;
    case Iop_Shr8:
        if (!count_opnd && !use_eflags_thunks)  count_opnd = arg2;
        return new BinOp(RSHIFT, arg1, new Cast(arg2, REG_8, CAST_UNSIGNED));
        break;
	case Iop_Shr16:
        if (!count_opnd && !use_eflags_thunks)  count_opnd = arg2;
        return new BinOp(RSHIFT, arg1, new Cast(arg2, REG_16, CAST_UNSIGNED));
        break;
	case Iop_Shr32:
        if (!count_opnd && !use_eflags_thunks)  count_opnd = arg2;
        return new BinOp(RSHIFT, arg1, new Cast(arg2, REG_32, CAST_UNSIGNED));
        break;
	case Iop_Shr64:
        if (!count_opnd && !use_eflags_thunks)  count_opnd = arg2;
        return new BinOp(RSHIFT, arg1, new Cast(arg2, REG_64, CAST_UNSIGNED));
        break;
    case Iop_Sar8:
        if (!count_opnd && !use_eflags_thunks)  count_opnd = arg2;
        return new BinOp(ARSHIFT, arg1, new Cast(arg2, REG_8, CAST_UNSIGNED));
        break;
	case Iop_Sar16:
        if (!count_opnd && !use_eflags_thunks)  count_opnd = arg2;
        return new BinOp(ARSHIFT, arg1, new Cast(arg2, REG_16, CAST_UNSIGNED));
        break;
	case Iop_Sar32:
        if (!count_opnd && !use_eflags_thunks)  count_opnd = arg2;
        return new BinOp(ARSHIFT, arg1, new Cast(arg2, REG_32, CAST_UNSIGNED));
        break;
    case Iop_Sar64:
        if (!count_opnd && !use_eflags_thunks)  count_opnd = arg2;
        return new BinOp(ARSHIFT, arg1, new Cast(arg2, REG_64, CAST_UNSIGNED));
        break;
        case Iop_CmpEQ8:
	case Iop_CmpEQ16:
	case Iop_CmpEQ32:
	case Iop_CmpEQ64:    return new BinOp(EQ, arg1, arg2);
        case Iop_CmpNE8:
	case Iop_CmpNE16:
	case Iop_CmpNE32:
	case Iop_CmpNE64:    return new BinOp(NEQ, arg1, arg2);

        case Iop_CmpLT32U: return new BinOp(LT, arg1, arg2);

        case Iop_32HLto64:          return translate_32HLto64(arg1, arg2);
        case Iop_MullS8:            return translate_MullS8(arg1, arg2);
        case Iop_MullU32:           return translate_MullU32(arg1, arg2);
        case Iop_MullS32:           return translate_MullS32(arg1, arg2);
        case Iop_DivModU64to32:     return translate_DivModU64to32(arg1, arg2);
        case Iop_DivModS64to32:     return translate_DivModS64to32(arg1, arg2);

    case Iop_DivU32: return new BinOp(DIVIDE, arg1, arg2);
    case Iop_DivS32: return new BinOp(SDIVIDE, arg1, arg2);
    case Iop_DivU64: return new BinOp(DIVIDE, arg1, arg2);
    case Iop_DivS64: return new BinOp(SDIVIDE, arg1, arg2);
        default:
            break;
    }

    // TODO: free args

    return NULL;
}

Exp *translate_binop( IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout )
{
    assert(irbb);
    assert(expr);
    assert(irout);

    //Exp *arg2;
    Exp *result;

    if ( (result = translate_simple_binop(expr, irbb, irout)) != NULL )
        return result;

    switch ( expr->Iex.Binop.op ) 
        {

 
        case Iop_CmpF32:
        case Iop_CmpF64:     
        case Iop_CmpF128:

        // arg1 in this case, specifies rounding mode, and so is ignored
            /*        case Iop_I64toF64:
        case Iop_F64toI64:
        case Iop_F64toF32:          
        case Iop_F64toI32:
        case Iop_F64toI16:
        case Iop_RoundF64toInt:
        case Iop_2xm1F64:*/


        case Iop_F64toI16S: /* IRRoundingMode(I32) x F64 -> signed I16 */
        case Iop_F64toI32S: /* IRRoundingMode(I32) x F64 -> signed I32 */
        case Iop_F64toI64S: /* IRRoundingMode(I32) x F64 -> signed I64 */
        case Iop_F64toI64U: /* IRRoundingMode(I32) x F64 -> unsigned I64 */

        case Iop_F64toI32U: /* IRRoundingMode(I32) x F64 -> unsigned I32 */

        case Iop_I16StoF64: /*                       signed I16 -> F64 */
        case Iop_I32StoF64: /*                       signed I32 -> F64 */
        case Iop_I64StoF64: /* IRRoundingMode(I32) x signed I64 -> F64 */
        case Iop_I64UtoF64: /* IRRoundingMode(I32) x unsigned I64 -> F64 */
        case Iop_I64UtoF32: /* IRRoundingMode(I32) x unsigned I64 -> F32 */

        case Iop_I32UtoF64: /*                       unsigned I32 -> F64 */

        case Iop_F32toI16S: /* IRRoundingMode(I32) x F32 -> signed I16 */
        case Iop_F32toI32S: /* IRRoundingMode(I32) x F32 -> signed I32 */
        case Iop_F32toI64S: /* IRRoundingMode(I32) x F32 -> signed I64 */

        case Iop_I16StoF32: /*                       signed I16 -> F32 */
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

//         case Iop_CmpF64:            return translate_CmpF64(expr, irbb, irout);

//         // arg1 in this case, specifies rounding mode, and so is ignored
//         case Iop_I64toF64:
//             arg2 = translate_expr(expr->Iex.Binop.arg2, irbb, irout);
//             return new Cast( arg2, REG_64, CAST_FLOAT );
//         case Iop_F64toI64:
//             arg2 = translate_expr(expr->Iex.Binop.arg2, irbb, irout);
//             return new Cast( arg2, REG_64, CAST_INTEGER );
//         case Iop_F64toF32:          
//             arg2 = translate_expr(expr->Iex.Binop.arg2, irbb, irout);
//             return new Cast( arg2, REG_32, CAST_FLOAT );
//         case Iop_F64toI32:
//             arg2 = translate_expr(expr->Iex.Binop.arg2, irbb, irout);
//             return new Cast( arg2, REG_32, CAST_INTEGER );
//         case Iop_F64toI16:
//             arg2 = translate_expr(expr->Iex.Binop.arg2, irbb, irout);
//             return new Cast( arg2, REG_16, CAST_INTEGER );

//         case Iop_RoundF64toInt:
//         case Iop_2xm1F64:
//             return new Unknown("Floating point op");

        default:  
	  throw "Unrecognized binary op";
    }

    return NULL;
}

Exp *translate_triop( IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout )
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
    Exp *arg2 = translate_expr(expr->Iex.Triop.arg2, irbb, irout);
    Exp *arg3 = translate_expr(expr->Iex.Triop.arg3, irbb, irout);

    switch ( expr->Iex.Triop.op ) 
    {

        case Iop_AddF64:
        case Iop_SubF64:
        case Iop_MulF64:
        case Iop_DivF64:
//         case Iop_AddF64:    return new BinOp(PLUS, arg2, arg3);
//         case Iop_SubF64:    return new BinOp(MINUS, arg2, arg3);
//         case Iop_MulF64:    return new BinOp(TIMES, arg2, arg3);
//         case Iop_DivF64:    return new BinOp(DIVIDE, arg2, arg3);

        case Iop_Yl2xF64:   
        case Iop_Yl2xp1F64:
        case Iop_ScaleF64:
	  return new Unknown("Floating point triop", REG_64);

        default:  
            throw "Unrecognized ternary op";
    }

    return NULL;
}


Exp *emit_mux0x( vector<Stmt *> *irout, reg_t type,
		 Exp *cond, Exp *exp0, Exp *expX )
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
    
    Temp *temp = mk_temp(type,irout);

#ifndef MUX_AS_CJMP
    // FIXME: modify match_mux0x to recognize this
    Exp *widened_cond;
    reg_t result_type;

    /*
     *
     *
     *
     *
     * README
     *
     *
     *
     *
     * IF YOU CHANGE THIS, MAKE SURE YOU CHANGE MATCH_MUX0X TOO.
     *
     *
     *
     *
     *
     *
     * IF YOU DONT, THINGS WILL BREAK.
     *
     *
     *
     *
     * THANK YOU.
     */

    widened_cond = mk_temp(type,irout);
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

#else // def MUX_AS_CJMP

    Label *labelX = mk_label();
    Label *done = mk_label();

    // match_mux0x depends on the order/types of these statements
    // if changing them here, make sure to make the corresponding changes there
    irout->push_back( new Move( new Temp(*temp), exp0 ) );
    irout->push_back( new CJmp( cond, new Name(done->label), new Name(labelX->label) ) );
    irout->push_back( labelX );
    irout->push_back( new Move( new Temp(*temp), expX ) );
    irout->push_back( done );
#endif

    /* Make sure that MUX_LENGTH is correct */
    assert( (initialSize + MUX_LENGTH) == irout->size());
    
    return temp;
}


Exp *translate_mux0x( IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout )
{
    assert(expr);
    assert(irbb);
    assert(irout);

    IRExpr *cond = expr->Iex.Mux0X.cond;
    IRExpr *expr0 = expr->Iex.Mux0X.expr0;
    IRExpr *exprX = expr->Iex.Mux0X.exprX;

    assert(cond);
    assert(expr0);
    assert(exprX);

    // It's assumed that both true and false expressions have the
    // same type so that when we create a temp to assign it to, we
    // can just use one type
    reg_t type = IRType_to_reg_type( typeOfIRExpr(irbb->tyenv, expr0) );
    reg_t cond_type = IRType_to_reg_type( typeOfIRExpr(irbb->tyenv, cond) );

    Exp *condE = translate_expr(cond, irbb, irout);
    Exp *exp0 = translate_expr(expr0, irbb, irout);
    Exp *expX = translate_expr(exprX, irbb, irout);

    //reg_t cond_type;
    //typecheck_exp(&cond_type, NULL, condE);
    condE = _ex_eq(condE, ex_const(cond_type, 0));

    return emit_mux0x(irout, type, condE, exp0, expX);
}

Exp *translate_load( IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout )
{
    assert(expr);
    assert(irbb);
    assert(irout);

    Exp *addr;
    Mem *mem;
    reg_t rtype;

    rtype = IRType_to_reg_type(expr->Iex.Load.ty);

    addr = translate_expr(expr->Iex.Load.addr, irbb, irout);
    mem = new Mem(addr, rtype);

    return mem;
}

Exp *translate_tmp_ex( IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout )
{
    assert(expr);
    assert(irbb);
    assert(irout);

    IRType type;
    string name;

    type = typeOfIRExpr(irbb->tyenv, expr);
    // VEX uses same temp name with different widths.
    // Incorporate VEX type into name so that we don't have same
    // temp with different widths.
    //    name = "t" + int_to_str(expr->Iex.Tmp.tmp);

    // VEX no longer uses the same temp name with different widths, so we could
    // stop doing this. --aij
    name = int_to_str(get_type_size(IRType_to_reg_type(type))) + "t" + int_to_str(expr->Iex.RdTmp.tmp);
    return mk_temp(name, type);

}


//----------------------------------------------------------------------
// Translate a single expression
//----------------------------------------------------------------------
Exp *translate_expr( IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout )
{
    assert(irout);

    Exp *result = NULL;
    string func;

    switch ( expr->tag )
    {
        case Iex_Binder:
	    result = new Unknown(sTag + "Binder", regt_of_irexpr(irbb, expr));
	    break;
        case Iex_Get:
            result = translate_get(expr, irbb, irout);
            break;
        case Iex_GetI:
	    result = new Unknown(uTag + "GetI", regt_of_irexpr(irbb, expr));
	    break;
        case Iex_RdTmp:
            result = translate_tmp_ex(expr, irbb, irout);
            break;
        case Iex_Triop:
            result = translate_triop(expr, irbb, irout);
            break;
        case Iex_Binop:
            result = translate_binop(expr, irbb, irout);
            break;
        case Iex_Unop:
            result = translate_unop(expr, irbb, irout);
            break;
        case Iex_Load:
            result = translate_load(expr, irbb, irout);
            break;
        case Iex_Const:
            result = translate_const(expr);
            break;
        case Iex_Mux0X:
            result = translate_mux0x(expr, irbb, irout);
            break;
        case Iex_CCall:
            result = translate_ccall(expr, irbb, irout);
            break;
        default:
            throw "Unrecognized expression type";
    }

    return result;
}

Stmt *translate_tmp_st( IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout )
{
    assert(stmt);
    assert(irbb);
    assert(irout);

    IRType type;
    Exp *data;
    string name;

    type = typeOfIRExpr(irbb->tyenv, stmt->Ist.WrTmp.data);
    data = translate_expr(stmt->Ist.WrTmp.data, irbb, irout);
    name = int_to_str(get_type_size(IRType_to_reg_type(type))) + "t" + int_to_str(stmt->Ist.WrTmp.tmp);
    //    name = "t" + int_to_str(stmt->Ist.Tmp.tmp);

    return new Move( mk_temp(name, type), data );
}



Stmt *translate_store( IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout )
{
    assert(stmt);
    assert(irbb);
    assert(irout);

    Exp *dest;
    Exp *data;
    Mem *mem;
    IRType itype;
    reg_t rtype;

    dest = translate_expr(stmt->Ist.Store.addr, irbb, irout);
    itype = typeOfIRExpr(irbb->tyenv, stmt->Ist.Store.data);
    rtype = IRType_to_reg_type(itype);
    mem = new Mem(dest, rtype);
    data = translate_expr(stmt->Ist.Store.data, irbb, irout);

    return new Move(mem, data); 
}

Stmt *translate_imark( IRStmt *stmt, IRSB *irbb )
{
    assert(stmt);
    
    return mk_dest_label( stmt->Ist.IMark.addr );
}

Stmt *translate_exit( IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout )
{
    assert(stmt);
    assert(irbb);
    assert(irout);

    // We assume the jump destination is always ever a 32 bit uint
    assert(stmt->Ist.Exit.dst->tag == Ico_U32);

    Exp *cond = translate_expr( stmt->Ist.Exit.guard, irbb, irout );

    if (stmt->Ist.Exit.jk == Ijk_MapFail) {
      // MapFail is only used for bailing out, and the target is always
      // the beginning of the same instruction, right? At least that seems
      // to currently be true in VEX r1774.
      
      return new Assert(new UnOp(NOT,cond));
    }
    
    Name *dest = mk_dest_name( stmt->Ist.Exit.dst->Ico.U32 );
    Label *next = mk_label();

    irout->push_back( new CJmp( cond, dest, new Name(next->label) ) );

    return next;
}

//----------------------------------------------------------------------
// Translate a single statement
//----------------------------------------------------------------------
Stmt *translate_stmt( IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout )
{
    assert(stmt);
    assert(irout);

    Stmt *result = NULL;

    switch ( stmt->tag )
        {
        case Ist_NoOp:
            result = new Comment("NoOp");
            break;
        case Ist_IMark:
            result = translate_imark(stmt, irbb);
            break;
        case Ist_AbiHint:
            result = new Comment(sTag + "AbiHint");
            break;
        case Ist_Put:
            result = translate_put(stmt, irbb, irout);
            break;
        case Ist_PutI:
            result = new Special(uTag + "PutI");
            break;
        case Ist_WrTmp:
            result = translate_tmp_st(stmt, irbb, irout);
            break;
        case Ist_Store:
            result = translate_store(stmt, irbb, irout);
            break;
        case Ist_CAS:
            result = new Special(uTag + "CAS");
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
            result = translate_exit(stmt, irbb, irout);
            break;
        default:
            throw "Unrecognized statement type";
        }

    assert(result);

    return result;
}

Stmt *translate_jumpkind( IRSB *irbb, vector<Stmt *> *irout )
{
  assert(irbb);

  Stmt *result = NULL;

  Exp *dest = NULL;
  if ( irbb->next->tag == Iex_Const ) {
    dest = mk_dest_name( irbb->next->Iex.Const.con->Ico.U32 );
 
    // removing the insignificant jump statment 
    // that actually goes to the next instruction
    // except for conditional jump cases
    if ( irbb->jumpkind == Ijk_Boring && irbb->stmts[irbb->stmts_used-1]->tag != Ist_Exit )     
      if ( irbb->stmts[0]->Ist.IMark.addr + irbb->stmts[0]->Ist.IMark.len 
            == irbb->next->Iex.Const.con->Ico.U32 )
      {
        Exp::destroy(dest);
        return NULL;
      }
  }
  else
    dest = translate_expr( irbb->next, irbb, irout );

  switch ( irbb->jumpkind )
    {
        case Ijk_Boring: 
        case Ijk_Yield:
          result = new Jmp(dest);
          break; 
        case Ijk_Call:
          if(!translate_calls_and_returns)
            result = new Jmp(dest);
          else
            result = new Call(NULL, dest, vector<Exp *>());
          break;
        case Ijk_Ret:
          if(!translate_calls_and_returns)
            result = new Jmp(dest);
          else {
            Exp::destroy(dest);
            result = new Return(NULL);
          }
          break;
        case Ijk_NoDecode:
          Exp::destroy(dest);
          result = new Special("VEX decode error");
          break;
        case Ijk_Sys_syscall:
        case Ijk_Sys_int32:
        case Ijk_Sys_int128:
        case Ijk_Sys_sysenter:
          // Since these will create a special (insert_specials), we
          // won't translate these as a jump here.
          Exp::destroy(dest);
          return NULL;
          break;
        default:
          Exp::destroy(dest);
          throw "Unrecognized jump kind";    
    }

  assert(result);

  return result;
}


// FIXME: call arch specific functions
bool is_special( bfd_vma inst )
{
  return false;
}

vector<Stmt *> *translate_special( bfd_vma inst )
{
  panic("Why did this get called? We are now saying that no instruction is a special.");
}

//----------------------------------------------------------------------
// Translate an IRSB into a vector of Stmts in our IR
//----------------------------------------------------------------------
vector<Stmt *> *translate_irbb( IRSB *irbb )
{

    //
    // It's assumed that each irbb only contains the translation for
    // 1 instruction
    //
    
    // For some instructions, the eflag affecting IR needs out-of-band
    // arguments. This function cleans up those operands.
    do_cleanups_before_processing ();
    
    assert(irbb);

    vector<Stmt *> *irout = new vector<Stmt *>();    

    assert(irout);

    int i;
    Stmt *st = NULL;

    //
    // Translate all the statements
    //
    IRStmt *stmt;
    stmt = irbb->stmts[0];
    assert(stmt->tag == Ist_IMark);

    st = translate_stmt(stmt, irbb, irout);
    assert(st->stmt_type == LABEL);
    irout->push_back(st);

    for(int i = 0; i < irbb->tyenv->types_used; i++){
      IRType ty = irbb->tyenv->types[i];
      reg_t typ = IRType_to_reg_type(ty);
      // FIXME: Can we safely remove the T_n prefix? No need to specify the type
      // twice...
      string name = "T_" + int_to_str(get_type_size(typ)) + "t" + int_to_str(i);
      irout->push_back(new VarDecl(name, typ));
    }
    
    for ( i = 1; i < irbb->stmts_used; i++ )
    {
        IRStmt *stmt = irbb->stmts[i];

        try
        {
          st = translate_stmt(stmt, irbb, irout);
        }
        catch ( const char *e )
        {
          //            st = new Comment( "Untranslated IR statement. Cause: \"" + string(e) + "\"" );
            st = new Special( "Untranslated IR statement. Cause: " 
                              + string(e));
        }

        irout->push_back(st);
    }

    //
    // Translate the jump at the end of the BB
    //
    try
    {
        st = translate_jumpkind(irbb, irout);
    }
    catch ( const char *e )
    {
      //        st = new Comment( "Untranslated Jump out of BB. Cause: \"" + string(e) + "\"" );
        st = new Special( "Untranslated Jump out of BB. Cause: " + string(e) );
    }

    if (st != NULL)
        irout->push_back(st);

    return irout;
}

VexArch vexarch_of_bfdarch(bfd_architecture arch) {
  switch (arch) {
  case bfd_arch_i386:
    // check for amd64?
    return VexArchX86;
  case bfd_arch_arm:
    return VexArchARM;
  default:
    return VexArch_INVALID;
  }
}
VexArch vexarch_of_prog(asm_program_t *prog) {
  return vexarch_of_bfdarch(bfd_get_arch(prog->abfd));
}

//======================================================================
// 
// Utility functions that wrap the raw translation functions.
// These are what should be used to do the actual translation.
// See print-ir.cpp in ../../apps for examples of how to use these.
//
//======================================================================

// TODO: It might be worth optimizing get_section_of to work fast for
// consecutive addressess
bap_block_t* generate_vex_ir(asm_program_t *prog, address_t inst)
{
  bap_block_t *vblock = new bap_block_t;
  VexArch guest = vexarch_of_prog(prog);

  vblock->inst = inst;
  // Skip the VEX translation of special instructions because these
  // are also the ones that VEX does not handle
  if ( !is_special( inst )) {
      vblock->vex_ir = translate_insn(guest, asmir_get_ptr_to_instr(prog, inst), inst);
  } else
    vblock->vex_ir = NULL;

  return vblock;
}


//----------------------------------------------------------------------
// Take a vector of instrs function and translate it into VEX IR blocks
// and store them in the vector of bap blocks
//----------------------------------------------------------------------
vector<bap_block_t *> generate_vex_ir(asm_program_t *prog,
				       address_t start, address_t end)
{
  vector<bap_block_t *> results;
  address_t inst;
  assert(prog);
  translate_init();
  for(inst = start; inst < end; inst += asmir_get_instr_length(prog, inst)){
    bap_block_t *vblock = generate_vex_ir(prog, inst);
    results.push_back(vblock);
  }
  return results;
}


//----------------------------------------------------------------------
// Take a disassembled program and translate it into VEX IR blocks
// and store them in the vector of bap blocks
//----------------------------------------------------------------------
vector<bap_block_t *> generate_vex_ir( asm_program_t *prog )
{
  assert(prog);
  
  // Init the translation library
  translate_init();
  
  vector<bap_block_t *> results;

  for (section_t *sec = prog->segs; sec; sec = sec->next) {
      if (sec->is_code) {
          vector<bap_block_t *> tmp = generate_vex_ir(prog, sec->start_addr, sec->end_addr);
          results.insert(results.end(), tmp.begin(), tmp.end());
      }
  }

    return results;
}


/**
 * Insert both special("call") and special("ret") into the code.
 * This function should be replacing add_special_returns()
 */
void insert_specials(bap_block_t * block) {
    IRSB* bb = block->vex_ir;
    if (bb == NULL)
        return;
    IRJumpKind kind = bb->jumpkind;
    switch (kind) {
        case Ijk_Sys_syscall:
        case Ijk_Sys_int32:
        case Ijk_Sys_int128:
        case Ijk_Sys_sysenter:
	  if(!translate_calls_and_returns)
            block->bap_ir->push_back(new Special("syscall"));
	  break;
        case Ijk_Call:
	  if(!translate_calls_and_returns)
            block->bap_ir->push_back(new Special("call"));
	  break;
        case Ijk_Ret:
	  if(!translate_calls_and_returns) {
            block->bap_ir->push_back(new Special("ret"));
	    block->bap_ir->push_back(mk_label());
	  }
	  break;
        default:
            // do nothing
            break;
    }
}


// Translate a single block to Vine. guest_arch must be set already
void generate_bap_ir_block( asm_program_t *prog, bap_block_t *block )
{
  static unsigned int ir_addr = 100; // Argh, this is dumb

  assert(block);
  
  // Set the global everyone else will look at.
  guest_arch = vexarch_of_prog(prog);

  // Translate the block
  if (is_special(block->inst)) 
    block->bap_ir = translate_special(block->inst);
  else
    block->bap_ir = translate_irbb( block->vex_ir );

  assert(block->bap_ir);
  vector<Stmt *> *vir = block->bap_ir;
  
  // Go through block and add Special's for ret
  insert_specials(block);
  
  // Go through the block and add on eflags modifications
  //if(!use_eflags_thunks)
    modify_flags(prog, block);
  
  // Delete EFLAGS get thunks
  if(!use_eflags_thunks)
    del_get_thunk(prog, block);
  
  // Add the asm and ir addresses
  for ( int j = 0; j < vir->size(); j++ )
    {
      if(block->inst)
	vir->at(j)->asm_address = block->inst;
      vir->at(j)->ir_address = ir_addr++;
    }
}

vector<bap_block_t *>
generate_bap_ir( asm_program_t *prog, vector<bap_block_t *> vblocks )
{
    unsigned int vblocksize = vblocks.size();

    for ( int i = 0; i < vblocksize; i++ )
    {
        bap_block_t *block = vblocks.at(i);
	generate_bap_ir_block(prog, block);
    }
    return vblocks;
}


//----------------------------------------------------------------------
// 
// Helpers
//
//----------------------------------------------------------------------


string inst_to_str(asm_program_t *prog, address_t inst )
{
    return string(asmir_string_of_insn(prog, inst));
}

string get_op_str(asm_program_t *prog, address_t inst )
{
    string str = inst_to_str(prog, inst);
    istringstream stream(str);
    string token;
    getline(stream, token, '\t');
    getline(stream, token, '\t');

    return token;
}


// Needed to be able to delete the Mux0X statements in shift instructions
int match_mux0x(vector<Stmt*> *ir, unsigned int i,
		Exp **cond, Exp **exp0,	Exp **expx, Exp **res)
{

// this code depends on the order of statements from emit_mux0x()

#ifndef MUX_AS_CJMP

  if (i < 0
      || i >= ir->size()
      || ir->at(i+0)->stmt_type != VARDECL /* temp */
      || ir->at(i+1)->stmt_type != VARDECL  /* widened_cond */
      || ir->at(i+2)->stmt_type != MOVE
      || ir->at(i+3)->stmt_type != MOVE)
    return -1;

  Move *s0 = (Move*)ir->at(i+2);
  Move *s1 = (Move*)ir->at(i+3);

  if (s0->lhs->exp_type != TEMP
      || s1->lhs->exp_type != TEMP
      || s0->rhs->exp_type != CAST
      || s1->rhs->exp_type != BINOP)
    return -1;
  
  Cast *e1 = static_cast<Cast*> (s0->rhs);
  BinOp *e2 = static_cast<BinOp*> (s1->rhs);
  if (e1->cast_type != CAST_SIGNED
      || e2->binop_type != BITOR
      || e2->lhs->exp_type != BINOP
      || e2->rhs->exp_type != BINOP)
    return -1;
  
  Exp *e3 = e1->exp; /* e3 is the condition */
  BinOp *e4 = static_cast<BinOp*> (e2->lhs); /* e4 is the true branch */
  BinOp *e5 = static_cast<BinOp*> (e2->rhs); /* e5 is the false branch */

  if (e4->binop_type != BITAND
      || e5->binop_type != BITAND)
    return -1;

  Exp *e6 = e4->lhs; /* this is exp0 */
  Exp *e7 = e5->lhs; /* this is expX */
  
  if (cond)
    *cond = e3;
  if (exp0)
    *exp0 = e6;
  if (expx)
    *expx = e7;
  if (res)
    *res = NULL; /* XXX: Not sure what to do here */

  // cout << "MATCH!!!!" << endl;
  
#else
  
  if (i < 0 || i >= ir->size()
      || ir->at(i)->stmt_type != MOVE
      || ir->at(i+3)->stmt_type != MOVE
      || ir->at(i+2)->stmt_type != LABEL
      || ir->at(i+4)->stmt_type != LABEL
      || ir->at(i+1)->stmt_type != CJMP)
    return -1;
  Move *s0 = (Move*)ir->at(i);
  CJmp *s1 = (CJmp*)ir->at(i+1);
  Label *s2 = (Label*)ir->at(i+2);
  Move *s3 = (Move*)ir->at(i+3);
  Label *s4 = (Label*)ir->at(i+4);

  if (s0->lhs->exp_type != TEMP
      || s3->lhs->exp_type != TEMP
      || ((Temp*)s0->lhs)->name != ((Temp*)s3->lhs)->name)
    return -1;

  if (s1->t_target->exp_type != NAME
      || s1->f_target->exp_type != NAME
      || ((Name*)s1->f_target)->name != s2->label
      || ((Name*)s1->t_target)->name != s4->label
      )
    return -1;

  //cout <<"match_mux0x matched!\n";
  if (cond)
    *cond = s1->cond;
  if (exp0)
    *exp0 = s0->rhs;
  if (expx)
    *expx = s3->rhs;
  if (res)
    *res = s0->lhs;

#endif

  return 0;
}

reg_t get_exp_type_from_cast( Cast *cast )
{
    assert(cast);

    return cast->typ;

}

reg_t get_exp_type( Exp *exp )
{
    assert(exp);

    reg_t type;

    if ( exp->exp_type == TEMP )
    {
        type = ((Temp *)exp)->typ;
    }
    else if ( exp->exp_type == CONSTANT )
    {
        type = ((Constant *)exp)->typ;
    }
    else
    {
        panic("Expression has no type info: " + exp->tostring());
    }

    return type;
}


void 
do_cleanups_before_processing()
{
    if (count_opnd) {
	count_opnd = NULL;
    }
}

