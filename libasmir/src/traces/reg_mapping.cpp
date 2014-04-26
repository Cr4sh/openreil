/* Handling the mapping from instruction operands to registers */

#include <assert.h>
#include <stmt.h>
#include "reg_mapping.h"

static
string regid_to_name(uint32_t id)
{
  string name;
  switch(id) {
  case es_reg:
    name = string("ES");
    break;
  case cs_reg:
    name = string("CS");
    break;
  case ss_reg:
    name = string("SS");
    break;
  case ds_reg:
    name = string("DS");
    break;
  case fs_reg:
    name = string("FS");
    break;
  case gs_reg:
    name = string("GS");
    break;
  case al_reg:
    name = string("AL");
    break;
  case ah_reg:
    name = string("AH");
    break;
  case ax_reg:
    name = string("AX");
    break;
  case eax_reg:
    name = string("EAX");
    break;
  case cl_reg:
    name = string("CL");
    break;
  case ch_reg:
    name = string("CH");
    break;
  case cx_reg:
    name = string("CX");
    break;
  case ecx_reg:
    name = string("ECX");
    break;
  case dl_reg:
    name = string("DL");
    break;
  case dh_reg:
    name = string("DH");
    break;
  case dx_reg:
    name = string("DX");
    break;
  case edx_reg:
    name = string("EDX");
    break;
  case bl_reg:
    name = string("BL");
    break;
  case bh_reg:
    name = string("BH");
    break;
  case bx_reg:
    name = string("BX");
    break;
  case ebx_reg:
    name = string("EBX");
    break;
  case esp_reg:
    name = string("ESP");
    break;
  case sp_reg:
    name = string("SP");
    break;
  case ebp_reg:
    name = string("EBP");
    break;
  case bp_reg:
    name = string("BP");
    break;
  case esi_reg:
    name = string("ESI");
    break;
  case si_reg:
    name = string("SI");
    break;
  case edi_reg:
    name = string("EDI");
    break;
  case di_reg:
    name = string("DI");
    break;
  default:
    assert(0);
  }
  return name;
}

uint32_t regid_to_full(uint32_t id)
{
  uint32_t full;

  switch(id) {
  case es_reg:
  case cs_reg:
  case ss_reg:
  case ds_reg:
  case fs_reg:
  case gs_reg:
    full = id;
    break;
  case al_reg:
  case ah_reg:
  case ax_reg:
  case eax_reg:
    full = eax_reg;
    break;
  case cl_reg:
  case ch_reg:
  case cx_reg:
  case ecx_reg:
    full = ecx_reg;
    break;
  case dl_reg:
  case dh_reg:
  case dx_reg:
  case edx_reg:
    full = edx_reg;
    break;
  case bl_reg:
  case bh_reg:
  case bx_reg:
  case ebx_reg:
    full = ebx_reg;
    break;
  case esp_reg:
  case sp_reg:
    full = esp_reg;
    break;
  case ebp_reg:
  case bp_reg:
    full = ebp_reg;
    break;
  case esi_reg:
  case si_reg:
    full = esi_reg;
    break;
  case edi_reg:
  case di_reg:
    full = edi_reg;
    break;
  default:
    assert(0);
  }
  return full;
}

static uint32_t regid_to_write_mask(uint32_t id)
{
  uint32_t mask = 0;

  switch(id) {
  /* 8 bit low */
  case al_reg: 
  case cl_reg:
  case dl_reg:
  case bl_reg:
    mask = 0xFFFFFF00;
    break;
  /* 8 bit high */
  case ah_reg:
  case ch_reg:
  case dh_reg:
  case bh_reg:
    mask = 0xFFFF00FF;
    break;
  /* 16 bit */
  case ax_reg:
  case cx_reg:
  case dx_reg:
  case bx_reg:
  case sp_reg:
  case bp_reg:
  case si_reg:
  case di_reg:
  /* segment regs (also 16 bit) */
  case es_reg:
  case cs_reg:
  case ss_reg:
  case ds_reg:
  case fs_reg:
  case gs_reg:
    //    mask = 0xFFFF0000;
    mask = 0x0000;
    break;
  /* 32 bit */
  case eax_reg:
  case ecx_reg:
  case edx_reg:
  case ebx_reg:
  case esp_reg:
  case ebp_reg:
  case esi_reg:
  case edi_reg:
    mask = 0;
    break;
  default:
    assert(0);
  }
  return mask;
}

reg_t regid_to_type(uint32_t id)
{
  reg_t t = REG_1;

  switch(id) {
  /* 8 bit low */
  case al_reg: 
  case cl_reg:
  case dl_reg:
  case bl_reg:

  /* 8 bit high */
  case ah_reg:
  case ch_reg:
  case dh_reg:
  case bh_reg:
    t = REG_8;
    break;

  /* 16 bit */
  case ax_reg:
  case cx_reg:
  case dx_reg:
  case bx_reg:
  case sp_reg:
  case bp_reg:
  case si_reg:
  case di_reg:
  /* segment regs (also 16 bit) */
  case es_reg:
  case cs_reg:
  case ss_reg:
  case ds_reg:
  case fs_reg:
  case gs_reg:
    t = REG_16;
    break;
  /* 32 bit */
  case eax_reg:
  case ecx_reg:
  case edx_reg:
  case ebx_reg:
  case esp_reg:
  case ebp_reg:
  case esi_reg:
  case edi_reg:
    t = REG_32;
    break;
  default:
    assert(0);
  }
  return t;
}

static 
uint32_t regid_to_read_mask(uint32_t id)
{
  uint32_t mask = 0;

  switch(id) {
  /* special case for segment regs, which are 16 bit */
  case es_reg:
  case cs_reg:
  case ss_reg:
  case ds_reg:
  case fs_reg:
  case gs_reg:
    mask = 0xFFFF;
    break;
  default:
    mask = ~regid_to_write_mask(id);
  }

  return mask;
}

string register_name(uint32_t id)
{
  uint32_t fullreg = regid_to_full(id);
  string regname = regid_to_name(fullreg);
  return string("R_") + regname;
}

// generate a move instruction, assigning 'val' to register 'id'.
// the move is always into the full name of the register.
// i.e., an assignment to AX is translated to a modification of EAX
Move* write_reg(uint32_t id, Exp *val, int offset)
{
  uint32_t mask = regid_to_write_mask(id);
  uint32_t fullreg = regid_to_full(id);
  string regname = regid_to_name(fullreg);
  reg_t t = regid_to_type(fullreg);

  // set up lhs
  Temp *lhs = new Temp(t, string("R_") + regname);

  // set up value
  // needs to be shifted for "high bit" registers
  if(id == ah_reg || id == ch_reg || id == dh_reg || id == bh_reg) {
    if(val->exp_type == CONSTANT)
      ((Constant*)val)->val <<= 8;
    else
      val = new BinOp(LSHIFT,
		      val,
		      new Constant(REG_32,
				   8));
  }
  // make sure type is correct
  if(val->exp_type == CONSTANT)
    ((Constant*)val)->typ = t;
  else
    // make a narrowing cast. 
    // XXX: this is ugly since,
    // a) don't know if we need a cast at all
    // b) could need a widening cast
    // this should work for now. typechecker would help here, but
    // not available from c++
    // XXX NEW: should be widening?
    val = new Cast(val, t, CAST_UNSIGNED);

  if (offset >= 0) {
    // only writing to one byte.

    // drop old mask- make a new one based on writing one byte
    mask = ~(0xff << (offset * 8));
    if (id == ah_reg || id == ch_reg || id == dh_reg || id == bh_reg)
      mask <<= 8;

    // shift value
    if (offset != 0) {
      if(val->exp_type == CONSTANT)
	((Constant*)val)->val <<= (offset * 8);
      else
	val = new BinOp(LSHIFT,
			val,
			new Constant(REG_32,
				     offset * 8));
    }

  }

  if (mask != 0) {
    if (val->exp_type == CONSTANT) {
      ((Constant*)val)->val &= ~mask;
    } else {
      val = new BinOp(BITAND,
		      val,
		      new Constant(t,
				   ~mask));
    }
  }

  // set up rhs as:
  // reg & mask | val
  Exp *rhs = NULL;
  if (mask != 0) {
    Constant *mask_exp = new Constant(t,
				      mask);
    rhs = new BinOp(BITOR,
		    val,
		    new BinOp(BITAND,
			      mask_exp,
			      lhs->clone()));
  } else {
    rhs = val;
  }

  return new Move(lhs, rhs);
}

// always returns a 32 bit wide unsigned int
Exp* read_reg(uint32_t id)
{
  uint32_t fullreg = regid_to_full(id);
  string regname = regid_to_name(fullreg);  
  uint32_t mask = regid_to_read_mask(id);
  reg_t t = regid_to_type(fullreg);

  Exp *exp = new BinOp(BITAND,
		       new Temp(t, 
				string("R_") + regname),
		       new Constant(t,
				    mask));

  if(id == ah_reg || id == ch_reg || id == dh_reg || id == bh_reg) {
    exp = new BinOp(RSHIFT,
		    exp,
		    new Constant(REG_32,
				 8));
  }

  if(t != REG_32) {
    assert(t != REG_64);
    exp = new Cast(exp, REG_32, CAST_UNSIGNED);
  }

  return exp;
}

// returns the byte at the given offset of the given register,
// as a REG_8 Constant
Exp* read_reg(uint32_t id, int offset)
{
  Exp *exp = read_reg(id);
  uint32_t mask = 0xff;

  if (offset > 0)
    exp =  new BinOp(RSHIFT,
		     exp,
		     new Constant(REG_32,
				  offset * 8));

  exp = new BinOp(BITAND,
		  exp,
		  new Constant(REG_32,
			       mask));

  exp = new Cast(exp, REG_8, CAST_LOW);

  return exp;
}

cval_type_t get_type(uint32_t typ)
{
   switch (typ) {
   case VT_REG128:
   case VT_MEM128:
     return INT_128;
     break;

       case VT_REG64:
       case VT_MEM64:
         return INT_64;
         break;
       case VT_REG32: 
       case VT_MEM32: 
         return INT_32;
         break;
      case VT_REG16: 
      case VT_MEM16: 
         return INT_16;
         break;
      case VT_REG8: 
      case VT_MEM8: 
         return CHR;
         break;
      default:
        assert(false);
   }
   return NONE;
}
