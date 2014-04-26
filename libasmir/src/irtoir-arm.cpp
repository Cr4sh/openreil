#include "irtoir-internal.h"
#include "libvex_guest_arm.h"
#include <assert.h>
#include <stddef.h>


//
// Register offsets, copied from VEX/priv/guest_arm/toIR.c
//

/*------------------------------------------------------------*/
/*--- Offsets of various parts of the arm guest state.     ---*/
/*------------------------------------------------------------*/

#define OFFB_R0       offsetof(VexGuestARMState,guest_R0)
#define OFFB_R1       offsetof(VexGuestARMState,guest_R1)
#define OFFB_R2       offsetof(VexGuestARMState,guest_R2)
#define OFFB_R3       offsetof(VexGuestARMState,guest_R3)
#define OFFB_R4       offsetof(VexGuestARMState,guest_R4)
#define OFFB_R5       offsetof(VexGuestARMState,guest_R5)
#define OFFB_R6       offsetof(VexGuestARMState,guest_R6)
#define OFFB_R7       offsetof(VexGuestARMState,guest_R7)
#define OFFB_R8       offsetof(VexGuestARMState,guest_R8)
#define OFFB_R9       offsetof(VexGuestARMState,guest_R9)
#define OFFB_R10      offsetof(VexGuestARMState,guest_R10)
#define OFFB_R11      offsetof(VexGuestARMState,guest_R11)
#define OFFB_R12      offsetof(VexGuestARMState,guest_R12)
#define OFFB_R13      offsetof(VexGuestARMState,guest_R13)
#define OFFB_R14      offsetof(VexGuestARMState,guest_R14)
#define OFFB_R15T      offsetof(VexGuestARMState,guest_R15T)

// We assume you are compiling with the included patched version of 
// VEX. If not, you may need to define ARM_THUNKS to revert to 
// (at least at the time of this writing) default VEX ARM behavior.

#define OFFB_CC_OP    offsetof(VexGuestARMState,guest_CC_OP)
#define OFFB_CC_DEP1  offsetof(VexGuestARMState,guest_CC_DEP1)
#define OFFB_CC_DEP2  offsetof(VexGuestARMState,guest_CC_DEP2)
#define OFFB_CC_NDEP  offsetof(VexGuestARMState,guest_CC_NDEP)

vector<VarDecl *> arm_get_reg_decls()
{
  vector<VarDecl *> ret;
  reg_t r32 = REG_32;

  ret.push_back(new VarDecl("R0",      r32));
  ret.push_back(new VarDecl("R1",      r32));
  ret.push_back(new VarDecl("R2",      r32));
  ret.push_back(new VarDecl("R3",      r32));
  ret.push_back(new VarDecl("R4",      r32));
  ret.push_back(new VarDecl("R5",      r32));
  ret.push_back(new VarDecl("R6",      r32));
  ret.push_back(new VarDecl("R7",      r32));
  ret.push_back(new VarDecl("R8",      r32));
  ret.push_back(new VarDecl("R9",      r32));
  ret.push_back(new VarDecl("R10",     r32));
  ret.push_back(new VarDecl("R11",     r32));
  ret.push_back(new VarDecl("R12",     r32));
  ret.push_back(new VarDecl("R13",     r32));
  ret.push_back(new VarDecl("R14",     r32));
  ret.push_back(new VarDecl("R15",     r32));
  ret.push_back(new VarDecl("CC_OP",   r32));	 
  ret.push_back(new VarDecl("CC_DEP1", r32));
  ret.push_back(new VarDecl("CC_DEP2", r32));

  return ret;
}


//----------------------------------------------------------------------
// Translate VEX IR offset into ARM register name
// This is only called for 32-bit registers.
//----------------------------------------------------------------------
static string reg_offset_to_name( int offset )
{
    assert(offset >= 0);

    //static string name = "";

    switch ( offset )
    {
    case OFFB_R0:       return "R0";     
    case OFFB_R1:     	return "R1";     
    case OFFB_R2:     	return "R2";     
    case OFFB_R3:     	return "R3";     
    case OFFB_R4:     	return "R4";     
    case OFFB_R5:     	return "R5";     
    case OFFB_R6:     	return "R6";     
    case OFFB_R7:     	return "R7";     
    case OFFB_R8:     	return "R8";     
    case OFFB_R9:     	return "R9";     
    case OFFB_R10:    	return "R10";    
    case OFFB_R11:    	return "R11";    
    case OFFB_R12:    	return "R12";    
    case OFFB_R13:    	return "R13";    
    case OFFB_R14:    	return "R14";    
    case OFFB_R15T:    	return "R15T";    

    case OFFB_CC_OP:  	return "CC_OP";	 
    case OFFB_CC_DEP1:	return "CC_DEP1";
    case OFFB_CC_DEP2:	return "CC_DEP2";
    case OFFB_CC_NDEP:  return "CC_NDEP";
    default:
      panic("reg_offset_to_name(arm): Unrecognized register offset");
    }

}

static inline Temp *mk_reg( string name, reg_t width )
{
    return new Temp(width, name);
}

 
static Exp *translate_get_reg_32( int offset )
{
    assert(offset >= 0);
    
    Temp *reg = mk_reg(reg_offset_to_name(offset), REG_32);
    
    return reg;
}


Exp  *arm_translate_get( IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout )
{
  int offset = expr->Iex.Get.offset;

  assert(typeOfIRExpr(irbb->tyenv, expr) == Ity_I32);

  return translate_get_reg_32(offset);
}


static Stmt *translate_put_reg_32( int offset, Exp *data, IRSB *irbb )
{
    assert(data);
    
    Temp *reg = mk_reg(reg_offset_to_name(offset), REG_32);
    
    return new Move( reg, data );
}


Stmt *arm_translate_put( IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout )
{
  int offset = stmt->Ist.Put.offset;

  Exp *data = translate_expr(stmt->Ist.Put.data, irbb, irout);

  assert(typeOfIRExpr(irbb->tyenv, stmt->Ist.Put.data) == Ity_I32);

  return translate_put_reg_32(offset, data, irbb);
}


Exp  *arm_translate_ccall( IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout )
{
  string func = string(expr->Iex.CCall.cee->name);


  return new Unknown("CCall: " + func, regt_of_irexpr(irbb, expr));
}

void  arm_modify_flags( asm_program_t *prog, bap_block_t *block )
{
  //vector<Stmt *> *ir = block->bap_ir;
  // FIXME: implement this
}
  
