#include <string>
#include <vector>
#include <iostream>
#include <assert.h>
#include <stddef.h>

//#include "typecheck_ir.h"
#include "irtoir-internal.h"
#include "libvex_guest_x86.h"

//
// Register offsets, copied from VEX/priv/guest_x86/toIR.c
//

#define OFFB_EAX       offsetof(VexGuestX86State,guest_EAX)
#define OFFB_EBX       offsetof(VexGuestX86State,guest_EBX)
#define OFFB_ECX       offsetof(VexGuestX86State,guest_ECX)
#define OFFB_EDX       offsetof(VexGuestX86State,guest_EDX)
#define OFFB_ESP       offsetof(VexGuestX86State,guest_ESP)
#define OFFB_EBP       offsetof(VexGuestX86State,guest_EBP)
#define OFFB_ESI       offsetof(VexGuestX86State,guest_ESI)
#define OFFB_EDI       offsetof(VexGuestX86State,guest_EDI)

#define OFFB_EIP       offsetof(VexGuestX86State,guest_EIP)

#define OFFB_CC_OP     offsetof(VexGuestX86State,guest_CC_OP)
#define OFFB_CC_DEP1   offsetof(VexGuestX86State,guest_CC_DEP1)
#define OFFB_CC_DEP2   offsetof(VexGuestX86State,guest_CC_DEP2)
#define OFFB_CC_NDEP   offsetof(VexGuestX86State,guest_CC_NDEP)

#define OFFB_FPREGS    offsetof(VexGuestX86State,guest_FPREG[0])
#define OFFB_FPTAGS    offsetof(VexGuestX86State,guest_FPTAG[0])
#define OFFB_DFLAG     offsetof(VexGuestX86State,guest_DFLAG)
#define OFFB_IDFLAG    offsetof(VexGuestX86State,guest_IDFLAG)
#define OFFB_ACFLAG    offsetof(VexGuestX86State,guest_ACFLAG)
#define OFFB_FTOP      offsetof(VexGuestX86State,guest_FTOP)
#define OFFB_FC3210    offsetof(VexGuestX86State,guest_FC3210)
#define OFFB_FPROUND   offsetof(VexGuestX86State,guest_FPROUND)

#define OFFB_CS        offsetof(VexGuestX86State,guest_CS)
#define OFFB_DS        offsetof(VexGuestX86State,guest_DS)
#define OFFB_ES        offsetof(VexGuestX86State,guest_ES)
#define OFFB_FS        offsetof(VexGuestX86State,guest_FS)
#define OFFB_GS        offsetof(VexGuestX86State,guest_GS)
#define OFFB_SS        offsetof(VexGuestX86State,guest_SS)
#define OFFB_LDT       offsetof(VexGuestX86State,guest_LDT)
#define OFFB_GDT       offsetof(VexGuestX86State,guest_GDT)

#define OFFB_SSEROUND  offsetof(VexGuestX86State,guest_SSEROUND)
#define OFFB_XMM0      offsetof(VexGuestX86State,guest_XMM0)
#define OFFB_XMM1      offsetof(VexGuestX86State,guest_XMM1)
#define OFFB_XMM2      offsetof(VexGuestX86State,guest_XMM2)
#define OFFB_XMM3      offsetof(VexGuestX86State,guest_XMM3)
#define OFFB_XMM4      offsetof(VexGuestX86State,guest_XMM4)
#define OFFB_XMM5      offsetof(VexGuestX86State,guest_XMM5)
#define OFFB_XMM6      offsetof(VexGuestX86State,guest_XMM6)
#define OFFB_XMM7      offsetof(VexGuestX86State,guest_XMM7)

#define OFFB_EMWARN    offsetof(VexGuestX86State,guest_EMWARN)

#define OFFB_TISTART   offsetof(VexGuestX86State,guest_TISTART)
#define OFFB_TILEN     offsetof(VexGuestX86State,guest_TILEN)
#define OFFB_NRADDR    offsetof(VexGuestX86State,guest_NRADDR)

#define OFFB_IP_AT_SYSCALL offsetof(VexGuestX86State,guest_IP_AT_SYSCALL)

//
// Sub register offsets, calculated manually
//

#define OFFB_AX         (OFFB_EAX)
#define OFFB_AL         (OFFB_EAX)
#define OFFB_AH         (OFFB_EAX+1)
#define OFFB_BX         (OFFB_EBX)
#define OFFB_BL         (OFFB_EBX)
#define OFFB_BH         (OFFB_EBX+1)
#define OFFB_CX         (OFFB_ECX)
#define OFFB_CL         (OFFB_ECX)
#define OFFB_CH         (OFFB_ECX+1)
#define OFFB_DX         (OFFB_EDX)
#define OFFB_DL         (OFFB_EDX)
#define OFFB_DH         (OFFB_EDX+1)
#define OFFB_DI         (OFFB_EDI)
#define OFFB_SI         (OFFB_ESI)
#define OFFB_BP         (OFFB_EBP)
#define OFFB_SP         (OFFB_ESP)

//
// Some unusual register offsets
//
#define OFFB_CC_DEP1_0  (OFFB_CC_DEP1)

//
// EFLAGS masks 
//
#define CF_MASK (1)
#define PF_MASK (1 << 2)
#define AF_MASK (1 << 4)
#define ZF_MASK (1 << 6)
#define SF_MASK (1 << 7)
#define OF_MASK (1 << 11)

//
// EFLAGS positions
//
#define CF_POS  0
#define PF_POS  2
#define AF_POS  4
#define ZF_POS  6
#define SF_POS  7
#define OF_POS  11

//
// Condition code enum copied from VEX/priv/guest-x86/gdefs.h
// Note: If these constants are ever changed, then they would
//       need to be re-copied from the newer version of VEX.
//

typedef
   enum {
      X86CondO      = 0,  /* overflow           */
      X86CondNO     = 1,  /* no overflow        */

      X86CondB      = 2,  /* below              */
      X86CondNB     = 3,  /* not below          */

      X86CondZ      = 4,  /* zero               */
      X86CondNZ     = 5,  /* not zero           */

      X86CondBE     = 6,  /* below or equal     */
      X86CondNBE    = 7,  /* not below or equal */

      X86CondS      = 8,  /* negative           */
      X86CondNS     = 9,  /* not negative       */

      X86CondP      = 10, /* parity even        */
      X86CondNP     = 11, /* not parity even    */

      X86CondL      = 12, /* jump less          */
      X86CondNL     = 13, /* not less           */

      X86CondLE     = 14, /* less or equal      */
      X86CondNLE    = 15, /* not less or equal  */

      X86CondAlways = 16  /* HACK */
   }
   X86Condcode;




// XXX: copied from VEX/priv/guest-x86/gdefs.h
enum {
    X86G_CC_OP_COPY=0,  /* DEP1 = current flags, DEP2 = 0, NDEP = unused */
                        /* just copy DEP1 to output */

    X86G_CC_OP_ADDB,    /* 1 */
    X86G_CC_OP_ADDW,    /* 2 DEP1 = argL, DEP2 = argR, NDEP = unused */
    X86G_CC_OP_ADDL,    /* 3 */

    X86G_CC_OP_SUBB,    /* 4 */
    X86G_CC_OP_SUBW,    /* 5 DEP1 = argL, DEP2 = argR, NDEP = unused */
    X86G_CC_OP_SUBL,    /* 6 */

    X86G_CC_OP_ADCB,    /* 7 */
    X86G_CC_OP_ADCW,    /* 8 DEP1 = argL, DEP2 = argR ^ oldCarry, NDEP = oldCarry */
    X86G_CC_OP_ADCL,    /* 9 */

    X86G_CC_OP_SBBB,    /* 10 */
    X86G_CC_OP_SBBW,    /* 11 DEP1 = argL, DEP2 = argR ^ oldCarry, NDEP = oldCarry */
    X86G_CC_OP_SBBL,    /* 12 */

    X86G_CC_OP_LOGICB,  /* 13 */
    X86G_CC_OP_LOGICW,  /* 14 DEP1 = result, DEP2 = 0, NDEP = unused */
    X86G_CC_OP_LOGICL,  /* 15 */

    X86G_CC_OP_INCB,    /* 16 */
    X86G_CC_OP_INCW,    /* 17 DEP1 = result, DEP2 = 0, NDEP = oldCarry (0 or 1) */
    X86G_CC_OP_INCL,    /* 18 */

    X86G_CC_OP_DECB,    /* 19 */
    X86G_CC_OP_DECW,    /* 20 DEP1 = result, DEP2 = 0, NDEP = oldCarry (0 or 1) */
    X86G_CC_OP_DECL,    /* 21 */

    X86G_CC_OP_SHLB,    /* 22 DEP1 = res, DEP2 = res', NDEP = unused */
    X86G_CC_OP_SHLW,    /* 23 where res' is like res but shifted one bit less */
    X86G_CC_OP_SHLL,    /* 24 */

    X86G_CC_OP_SHRB,    /* 25 DEP1 = res, DEP2 = res', NDEP = unused */
    X86G_CC_OP_SHRW,    /* 26 where res' is like res but shifted one bit less */
    X86G_CC_OP_SHRL,    /* 27 */

    X86G_CC_OP_ROLB,    /* 28 */
    X86G_CC_OP_ROLW,    /* 29 DEP1 = res, DEP2 = 0, NDEP = old flags */
    X86G_CC_OP_ROLL,    /* 30 */

    X86G_CC_OP_RORB,    /* 31 */
    X86G_CC_OP_RORW,    /* 32 DEP1 = res, DEP2 = 0, NDEP = old flags */
    X86G_CC_OP_RORL,    /* 33 */

    X86G_CC_OP_UMULB,   /* 34 */
    X86G_CC_OP_UMULW,   /* 35 DEP1 = argL, DEP2 = argR, NDEP = unused */
    X86G_CC_OP_UMULL,   /* 36 */

    X86G_CC_OP_SMULB,   /* 37 */
    X86G_CC_OP_SMULW,   /* 38 DEP1 = argL, DEP2 = argR, NDEP = unused */
    X86G_CC_OP_SMULL,   /* 39 */

    X86G_CC_OP_NUMBER
};

// eflags helpers
// (making these public to help generate thunks)
vector<Stmt *> mod_eflags_copy( reg_t type, Exp *arg1, Exp *arg2 );
vector<Stmt *> mod_eflags_add( reg_t type, Exp *arg1, Exp *arg2 );
vector<Stmt *> mod_eflags_sub( reg_t type, Exp *arg1, Exp *arg2 );
vector<Stmt *> mod_eflags_adc( reg_t type, Exp *arg1, Exp *arg2, Exp *arg3 );
vector<Stmt *> mod_eflags_sbb( reg_t type, Exp *arg1, Exp *arg2, Exp *arg3 );
vector<Stmt *> mod_eflags_logic( reg_t type, Exp *arg1, Exp *arg2 );
vector<Stmt *> mod_eflags_inc( reg_t type, Exp *arg1, Exp *arg2, Exp *arg3 );
vector<Stmt *> mod_eflags_dec( reg_t type, Exp *arg1, Exp *arg2, Exp *arg3 );
vector<Stmt *> mod_eflags_shl( reg_t type, Exp *arg1, Exp *arg2 );
vector<Stmt *> mod_eflags_shr( reg_t type, Exp *arg1, Exp *arg2 );
vector<Stmt *> mod_eflags_rol( reg_t type, Exp *arg1, Exp *arg2, Exp *arg3 );
vector<Stmt *> mod_eflags_ror( reg_t type, Exp *arg1, Exp *arg2, Exp *arg3 );
vector<Stmt *> mod_eflags_umul( reg_t type, Exp *arg1, Exp *arg2 );
vector<Stmt *> mod_eflags_smul( reg_t type, Exp *arg1, Exp *arg2 );



using namespace std;

//
// For labeling untranslated VEX IR instructions
//
static string uTag = "Unknown: ";
static string sTag = "Skipped: ";




//======================================================================
// 
// Helper functions for the translation
//
//======================================================================





vector<VarDecl *> i386_get_reg_decls()
{
  vector<VarDecl *> ret;
  reg_t r32 = REG_32;
  reg_t r16 = REG_16;
  reg_t r8 = REG_8;
  reg_t r1 = REG_1;


  ret.push_back(new VarDecl("EFLAGS", r32));
  ret.push_back(new VarDecl("R_LDT", r32)); 
  ret.push_back(new VarDecl("R_GDT", r32)); 
  ret.push_back(new VarDecl("R_DFLAG", r32)); 

  ret.push_back(new VarDecl("R_CS", r16)); 
  ret.push_back(new VarDecl("R_DS", r16)); 
  ret.push_back(new VarDecl("R_ES", r16)); 
  ret.push_back(new VarDecl("R_FS", r16)); 
  ret.push_back(new VarDecl("R_GS", r16)); 
  ret.push_back(new VarDecl("R_SS", r16)); 

  // Status bit flags
  ret.push_back(new VarDecl("R_CF", r1));
  ret.push_back(new VarDecl("R_CF", r1));  
  ret.push_back(new VarDecl("R_PF", r1));  
  ret.push_back(new VarDecl("R_AF", r1));  
  ret.push_back(new VarDecl("R_ZF", r1));  
  ret.push_back(new VarDecl("R_SF", r1));  
  ret.push_back(new VarDecl("R_OF", r1));  
  ret.push_back(new VarDecl("R_CC_OP", r32));  
  ret.push_back(new VarDecl("R_CC_DEP1", r32));  
  ret.push_back(new VarDecl("R_CC_DEP2", r32));  
  ret.push_back(new VarDecl("R_CC_NDEP", r32));  

  // other flags
  ret.push_back(new VarDecl("R_DFLAG", r32)); // Direction Flag
  ret.push_back(new VarDecl("R_IDFLAG", r1)); 
        // Id flag (support for cpu id instruction)
  ret.push_back(new VarDecl("R_ACFLAG", r1)); // Alignment check
  ret.push_back(new VarDecl("R_EMWARN", r32));

  // General purpose 32-bit registers
  ret.push_back(new VarDecl("R_EAX", r32));  
  ret.push_back(new VarDecl("R_EBX", r32));  
  ret.push_back(new VarDecl("R_ECX", r32));  
  ret.push_back(new VarDecl("R_EDX", r32));  
  ret.push_back(new VarDecl("R_ESI", r32));  
  ret.push_back(new VarDecl("R_EDI", r32));  
  ret.push_back(new VarDecl("R_EBP", r32));  
  ret.push_back(new VarDecl("R_ESP", r32));  

  // 16-bit registers (bits 0-15)
  ret.push_back(new VarDecl("R_AX", r16));  
  ret.push_back(new VarDecl("R_BX", r16));  
  ret.push_back(new VarDecl("R_CX", r16));  
  ret.push_back(new VarDecl("R_DX", r16));  
  ret.push_back(new VarDecl("R_BP", r16));  
  ret.push_back(new VarDecl("R_SI", r16));  
  ret.push_back(new VarDecl("R_DI", r16));  
  ret.push_back(new VarDecl("R_SP", r16));  


  // 8-bit registers (bits 0-7)
  ret.push_back(new VarDecl("R_AL", r8));  
  ret.push_back(new VarDecl("R_BL", r8));  
  ret.push_back(new VarDecl("R_CL", r8));  
  ret.push_back(new VarDecl("R_DL", r8));  

  // 8-bit registers (bits 8-15)
  ret.push_back(new VarDecl("R_AH", r8));  
  ret.push_back(new VarDecl("R_BH", r8));  
  ret.push_back(new VarDecl("R_CH", r8));  
  ret.push_back(new VarDecl("R_DH", r8));  

  // 32-bit segment base registers
  ret.push_back(new VarDecl("R_CS_BASE", r32));
  ret.push_back(new VarDecl("R_DS_BASE", r32));
  ret.push_back(new VarDecl("R_ES_BASE", r32));
  ret.push_back(new VarDecl("R_FS_BASE", r32));
  ret.push_back(new VarDecl("R_GS_BASE", r32));
  ret.push_back(new VarDecl("R_SS_BASE", r32));
  return ret;
}


//----------------------------------------------------------------------
// Translate VEX IR offset into x86 register name
// This is only called for 32-bit registers.
//----------------------------------------------------------------------
static string reg_offset_to_name( int offset )
{
    assert(offset >= 0);

    static string name = "";

    switch ( offset )
    {

        case OFFB_EAX:      name = "EAX";       break;
        case OFFB_EBX:      name = "EBX";       break;
        case OFFB_ECX:      name = "ECX";       break;
        case OFFB_EDX:      name = "EDX";       break;
        case OFFB_ESP:      name = "ESP";       break;
        case OFFB_EBP:      name = "EBP";       break;
        case OFFB_ESI:      name = "ESI";       break;
        case OFFB_EDI:      name = "EDI";       break;

        case OFFB_EIP:      name = "EIP";       break;

        case OFFB_CC_OP:    name = "CC_OP";     break;
        case OFFB_CC_DEP1:  name = "CC_DEP1";   break;
        case OFFB_CC_DEP2:  name = "CC_DEP2";   break;
        case OFFB_CC_NDEP:  name = "CC_NDEP";   break;

        case OFFB_FPREGS:   name = "FPREGS";    break;
        case OFFB_FPTAGS:   name = "FPTAGS";    break;
        case OFFB_DFLAG:    name = "DFLAG";     break;
        case OFFB_IDFLAG:   name = "IDFLAG";    break;
        case OFFB_ACFLAG:   name = "ACFLAG";    break;
        case OFFB_FTOP:     name = "FTOP";      break;
        case OFFB_FC3210:   name = "FC3210";    break;
        case OFFB_FPROUND:  name = "FPROUND";   break;

        case OFFB_LDT:      name = "LDT";       break;
        case OFFB_GDT:      name = "GDT";       break;

        case OFFB_SSEROUND: name = "SSEROUND";  break;
        case OFFB_XMM0:     name = "XMM0";      break;
        case OFFB_XMM1:     name = "XMM1";      break;
        case OFFB_XMM2:     name = "XMM2";      break;
        case OFFB_XMM3:     name = "XMM3";      break;
        case OFFB_XMM4:     name = "XMM4";      break;
        case OFFB_XMM5:     name = "XMM5";      break;
        case OFFB_XMM6:     name = "XMM6";      break;
        case OFFB_XMM7:     name = "XMM7";      break;

        case OFFB_EMWARN:   name = "EMWARN";    break;

        case OFFB_TISTART:  name = "TISTART";   break;
        case OFFB_TILEN:    name = "TILEN";     break;
        case OFFB_NRADDR:   name = "NRADDR";    break;

        case OFFB_IP_AT_SYSCALL: name = "IP_AT_SYSCALL"; break;

        default:
            panic("Unrecognized register name");
    }

    return name;
}

static inline Temp *mk_reg( string name, reg_t width )
{
    return new Temp(width, "R_" + name);
}



//======================================================================
// 
// Actual translation functions
//
//======================================================================


static Exp *translate_get_reg_8( int offset )
{
    bool low;
    string name;

    // Determine which 32 bit register this 8 bit sub
    // register is a part of
    switch ( offset )
    {
        case OFFB_AL:   name = "EAX";    low = true;    break;
        case OFFB_AH:   name = "EAX";    low = false;   break;
        case OFFB_BL:   name = "EBX";    low = true;    break;
        case OFFB_BH:   name = "EBX";    low = false;   break;
        case OFFB_CL:   name = "ECX";    low = true;    break;
        case OFFB_CH:   name = "ECX";    low = false;   break;
        case OFFB_DL:   name = "EDX";    low = true;    break;
        case OFFB_DH:   name = "EDX";    low = false;   break;
        default:
            throw "Unrecognized 8-bit register";
    }   

    // Create the corresponding named register
    Temp *reg = mk_reg(name, REG_32);
    

    if ( low )
    {
        return new Cast(reg, REG_8, CAST_LOW);
    }
    else
    {
      Exp *value = new Cast(reg, REG_16, CAST_LOW);
      return new Cast(value, REG_8, CAST_HIGH);
    }
}

static Exp *translate_get_segreg_base( int offset )
{
    string name = "";
    bool usebase = false;
    Exp *value = NULL;

    switch ( offset )
    {
        // 
        // These are regular 16 bit registers
        //
        // case OFFB_CS:
        //   name = "CS_BASE";
        //   usebase = true;
        //   break;
        // case OFFB_DS:
        //   name = "DS_BASE";
        //   usebase = true;
        //   break;
        // case OFFB_ES:
        //   name = "ES_BASE";
        //   usebase = true;
        //   break;

      /* Use base registers for FS and GS. */
        case OFFB_FS:
          name = "FS_BASE";
          usebase = true;
          break;
        case OFFB_GS:
          name = "GS_BASE";
          usebase = true;
          break;
        // case OFFB_SS:
        //   name = "SS_BASE";
        //   usebase = true;
        //   break;

          /* These are all assumed to be zero */
        case OFFB_CS:
        case OFFB_DS:
        case OFFB_ES:
        case OFFB_SS:
          usebase = false;
          value = ex_const(REG_32, 0);
          break;

        default:
            throw "Unrecognized register offset";
    }

    if (usebase) {
      Temp *reg = mk_reg(name, REG_32);
      value = reg;
      return value;
    } else {
      return value;
    }

}

static Exp *translate_get_reg_16( int offset )
{
    string name;
    bool sub;

    switch ( offset )
    {
        //
        // These are 16 bit sub registers
        //
        case OFFB_AX:   name = "EAX";   sub = true; break;
        case OFFB_BX:   name = "EBX";   sub = true; break;
        case OFFB_CX:   name = "ECX";   sub = true; break;
        case OFFB_DX:   name = "EDX";   sub = true; break;
        case OFFB_DI:   name = "EDI";   sub = true; break;
        case OFFB_SI:   name = "ESI";   sub = true; break;
        case OFFB_BP:   name = "EBP";   sub = true; break;
        case OFFB_SP:   name = "ESP";   sub = true; break;

        // 
        // These are regular 16 bit registers
        //
        case OFFB_CS:   name = "CS";    sub = false; break;
        case OFFB_DS:   name = "DS";    sub = false; break;
        case OFFB_ES:   name = "ES";    sub = false; break;
        case OFFB_FS:   name = "FS";    sub = false; break;
        case OFFB_GS:   name = "GS";    sub = false; break;
        case OFFB_SS:   name = "SS";    sub = false; break;

        default:
            throw "Unrecognized register offset";
    }

    Exp *value = NULL;

    if ( sub )
    {
        Temp *reg = mk_reg(name, REG_32);

        value = new Cast(reg, REG_16, CAST_LOW);
    }
    else
    {
        Temp *reg = mk_reg(name, REG_16);

        value = reg;
    }

    return value;
}

static Exp *translate_get_reg_32( int offset )
{
    assert(offset >= 0);
    
    Temp *reg = mk_reg(reg_offset_to_name(offset), REG_32);
    
    return reg;
}

Exp *i386_translate_get( IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout )
{
    assert(expr);
    assert(irbb);
    assert(irout);

    Exp *result = NULL;

    int offset;
    IRType type;

    type = typeOfIRExpr(irbb->tyenv, expr);
    offset = expr->Iex.Get.offset;

    if ( type == Ity_I8 )
    {
        result = translate_get_reg_8(offset);
    }

    else if ( type == Ity_I16 )
    {
        result = translate_get_reg_16(offset);
    }

    else if ( type == Ity_I32 )
    {
        result = translate_get_reg_32(offset);
    }

    else if ( type == Ity_I64 )
    {
      panic("Unhandled register type (I64)");
    }
    else if ( type == Ity_F32 )
    {
      panic("Unhandled register type (F32)");
    }
    else if ( type == Ity_F64 )
    {
      panic("Unhandled register type (F64)");
    }

    else
    {
        panic("Unrecognized register type");
    }

    return result;
}

Exp *i386_translate_ccall( IRExpr *expr, IRSB *irbb, vector<Stmt *> *irout )
{
    assert(expr);
    assert(irbb);
    assert(irout);

    Exp *result = NULL;

    string func = string(expr->Iex.CCall.cee->name);

    Constant    c_CF_POS(REG_32,CF_POS), c_CF_MASK(REG_32,CF_MASK),
      c_ZF_POS(REG_32,ZF_POS),
      c_PF_POS(REG_32,PF_POS),
      c_AF_POS(REG_32,AF_POS),
      c_SF_POS(REG_32,SF_POS),
      c_OF_POS(REG_32,OF_POS),
      c_1(REG_32,1);

    Temp EFLAGS(REG_32,"EFLAGS");

    // use condition codes directly
//     Exp *CF = _ex_shr(&EFLAGS, &c_CF_POS); Exp *ZF =
//     _ex_shr(&EFLAGS, &c_ZF_POS); Exp *PF = _ex_shr(&EFLAGS,
//     &c_PF_POS); Exp *SF = _ex_shr(&EFLAGS, &c_SF_POS); Exp *OF =
//     _ex_shr(&EFLAGS, &c_OF_POS);
    
    Temp *CF = mk_reg("CF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *PF = mk_reg("PF", REG_1);
    Temp *SF = mk_reg("SF", REG_1);
    Temp *OF = mk_reg("OF", REG_1);
    
    if ( func == "x86g_calculate_condition" )
    {
        int arg = expr->Iex.CCall.args[0]->Iex.Const.con->Ico.U32;

        if (use_eflags_thunks) {
          // call eflags thunk
          vector<Exp*> params;
          irout->push_back(new Call(NULL, "x86g_calculate_eflags_all",params));
        }

        switch ( arg )
        {
            case X86CondO:  result =         ecl(OF);  break;
            case X86CondNO: result = ex_not(OF); break;
            case X86CondB:  result =         ecl(CF);  break;
            case X86CondNB: result = ex_not(CF); break;
            case X86CondZ:  result =         ecl(ZF);  break;
            case X86CondNZ: result = ex_not(ZF); break;
            case X86CondS:  result =         ecl(SF);  break;
            case X86CondNS: result = ex_not(SF); break;
            case X86CondP:  result =         ecl(PF);  break;
            case X86CondNP: result = ex_not(PF); break;
            case X86CondBE: result =         ex_or(CF, ZF);  break;
            case X86CondNBE:result = _ex_not(ex_or(CF, ZF)); break;
            case X86CondL:  result =         ex_xor(SF, OF); break;
            case X86CondNL: result = _ex_not(ex_xor(SF, OF));break;
            case X86CondLE: result =         _ex_or(ex_xor(SF, OF), ecl(ZF)); break;
            case X86CondNLE:result = _ex_not(_ex_or(ex_xor(SF, OF), ecl(ZF)));break;
            default:
                panic("Unrecognized condition for x86g_calculate_condition");
        }
	result = _ex_u_cast(result, REG_32);
    }
    else if ( func == "x86g_calculate_eflags_c" )
    {
        if (use_eflags_thunks) {
          // call eflags thunk
          vector<Exp*> params;
          irout->push_back(new Call(NULL, "x86g_calculate_eflags_c",params));
        }

        result = ex_u_cast(CF, REG_32);
    }
    else if ( func == "x86g_calculate_eflags_all" )
    {
        if (use_eflags_thunks) {
          // call eflags thunk
          vector<Exp*> params;
          irout->push_back(new Call(NULL, "x86g_calculate_eflags_all",params));
        }

        result = new Temp(EFLAGS);
    }
    else if ( func == "x86g_use_seg_selector" ) 
    {
        // refer to valgrind/VEX/priv/guest-x86/ghelpers.c
	// x86g_use_seg_selector(ldt, gdt, seg_selector, virtual_addr)

	// data structure defined in valgrind/VEX/pub/libvex_ir.h

	irout->push_back(new Comment("x86g_use_seg_selector"));

        if (use_simple_segments) {

          /*
           * This code replaces segment memory calcuations with a fake
           * register, XS_BASE.  For instance, %fs:addr would be at
           * FS_BASE + addr.
           * 
           * Here's what happens when you have a segment.
           *    
           *    t6 = GET:I16(286)
           *    t5 = 16Uto32(t6)
           *    t2 = GET:I32(292)
           *    t3 = GET:I32(296)
           *    t7 =
           *    x86g_use_seg_selector{0x80c13a0}(t2,t3,t5,0x18:I32):I64
           *
           *    So, we are passed a temporary five.  We need to look
           *    at t5, find t6, and that will tell us the segment
           *    selector used in the statement.
           */ 
          
          IRExpr* tempexp = expr->Iex.CCall.args[2];
          IRExpr* uexp;
          IRExpr* tempexp2;
          IRExpr* segexp;
          IRStmt* wrtmp = NULL;
          IRTemp tempnum;
          Exp* segbaseexp;
          Exp* finaladdr;
          Exp *virtual_addr = translate_expr(expr->Iex.CCall.args[3], irbb, irout);
          
          
          /* temp is a reference to t5 */
          
          if (tempexp->tag != Iex_RdTmp) {
            cerr << tempexp->tag << endl;
            panic("Expected unop.");
          }

          tempnum = tempexp->Iex.RdTmp.tmp;

          /* Now, loop through the block statements and find the
           * statement that moves to our temporary. */
          for (int i = 0; i < irbb->stmts_size; i++) {
            IRStmt *s = irbb->stmts[i];

            if (s->tag == Ist_WrTmp) {
              /* Ahah, we found a write to a temporary. Is it our temporary? */
              if (s->Ist.WrTmp.tmp == tempnum) {
                /* Got it */
                wrtmp = s;
                break;
              }              
            }
          }

          if (wrtmp == NULL) {
            panic("Unable to find write to temporary");
          }

          /* Now we should have a uniop expression */
          uexp = wrtmp->Ist.WrTmp.data;

          if (uexp->tag != Iex_Unop) {
            panic("Expected unop");
          }

          /* This unop should reference another temp */
          tempexp = uexp->Iex.Unop.arg;

          if (tempexp->tag != Iex_RdTmp) {
            panic("Expected read temp");
          }

          tempnum = tempexp->Iex.RdTmp.tmp;
          wrtmp = NULL;
          
          /* Ok, again we need to look for an assignment to this
           * temp. Then we finally have the segment selector. */
          for (int i = 0; i < irbb->stmts_size; i++) {
            IRStmt *s = irbb->stmts[i];

            if (s->tag == Ist_WrTmp) {
              /* Ahah, we found a write to a temporary. Is it our temporary? */
              if (s->Ist.WrTmp.tmp == tempnum) {
                /* Got it */
                wrtmp = s;
                break;
              }              
            }
          }

          if (wrtmp == NULL) {
            panic("Unable to find write to temporary");
          }

          /* Finally, we should have the segment selector */
          segexp = wrtmp->Ist.WrTmp.data;

          if (segexp->tag != Iex_Get) {
            panic("Expected get");
          }

          segbaseexp = translate_get_segreg_base(segexp->Iex.Get.offset);

          finaladdr = _ex_add(segbaseexp, virtual_addr);
          finaladdr = _ex_u_cast(finaladdr, REG_64);
          
        result = finaladdr;
          
        } else {

        Exp *ldt = translate_expr((IRExpr*)expr->Iex.CCall.args[0], 
                                  irbb, irout); 
	Exp *gdt = translate_expr((IRExpr*)expr->Iex.CCall.args[1], 
                                  irbb, irout); 
	Exp *seg_selector = translate_expr((IRExpr*)expr->Iex.CCall.args[2],
                                           irbb, irout); 
        Exp *virtual_addr = translate_expr(expr->Iex.CCall.args[3], irbb, irout);
        //	long virtual_addr = expr->Iex.CCall.args[3]->Iex.Const.con->Ico.U32;

// 	if (seg_selector & ~0xFFFF)
// 	    goto done; 

// 	seg_selector &= 0x0000FFFF; 
	Exp *u_seg_selector = _ex_and(seg_selector, ex_const(0xFFFF)); 
	
// // 	if ((seg_selector & 3) != 3)
// // 	    goto done; 
	
// 	tibit = (seg_selector >> 2) & 1; 
	Exp *tibit = _ex_and(u_seg_selector, ex_const(4)); 

// 	seg_selector >>= 3; 
	Exp *seg_index = ex_shr(u_seg_selector, 3); 
	
// 	if (0 == tibit) {
// 	    if (0 == gdt)
// 		goto done; 
// 	    if (seg_selector >= VEX_GUEST_X86_GDT_NENT)
// 		goto done; 
	    
// 	    // now gdt point to the segment descriptor table
// 	    descriptor = gdt+seg_selector*64; 
// 	} else {
// 	    if (0 == ldt)
// 		goto done; 
	    
// 	    if (seg_selector >= VEX_GUEST_X86_GDT_NENT)
// 		goto done; 
	    
// 	    // now ldt point to the segment descriptor table
// 	    descriptor = ldt+seg_selector*64; 
// 	}
	Exp *seg_offset = _ex_mul(seg_index, ex_const(8)); 
	
	Exp *ldt_ptr = _ex_add(ldt, seg_offset); 
	Exp *gdt_ptr = _ex_add(gdt, seg_offset->clone()); 
	
	Exp *cond = _ex_eq(tibit, ex_const(0)); 
	Label *b1 = mk_label(); 
	Label *b2 = mk_label(); 
	Label *b3 = mk_label();
	Temp *desc = mk_temp(REG_32, irout);
	irout->push_back(new CJmp(cond, ex_name(b1->label), 
				  ex_name(b2->label))); 
	irout->push_back(b1);
	irout->push_back(new Move(new Temp(*desc), gdt_ptr)); 
	irout->push_back(new Jmp(new Name(b3->label))); 
	irout->push_back(b2);
	irout->push_back(new Move(new Temp(*desc), ldt_ptr));
	irout->push_back(b3); 
	
	// get base for segment selector
	// see Intel manual 3A: page 3-12 for descriptor format
	// descriptor+2, descriptor+3, descriptor+4, descriptor+7

	Exp *byte0 = new Cast(new Mem(_ex_add(desc, ex_const(2)), REG_8), REG_32, CAST_UNSIGNED); 
	Exp *byte1 = new Cast(new Mem(_ex_add(desc->clone(), ex_const(3)), REG_8), REG_32, CAST_UNSIGNED); 
	Exp *byte2 = new Cast(new Mem(_ex_add(desc->clone(), ex_const(4)), REG_8), REG_32, CAST_UNSIGNED); 
	Exp *byte3 = new Cast(new Mem(_ex_add(desc->clone(), ex_const(7)), REG_8), REG_32, CAST_UNSIGNED); 

	Exp *addr = 
	    _ex_add(byte0, 
		    _ex_add(_ex_mul(byte1, ex_const(256)),
			    _ex_add(_ex_mul(byte2, ex_const(256*256)),
				_ex_add(_ex_mul(byte3, ex_const(256*256*256)), 
					virtual_addr))));
			    
//	Constant *desc = new Constant(REG_32, descriptor); 
	
	// return an address, high 32 bits are zero, indicating success
	
	result = _ex_u_cast(addr, REG_64); 
        }

    }
    else if ( func == "x86g_calculate_RCL" ) 
    {
      Exp *arg = translate_expr(expr->Iex.CCall.args[0], irbb, irout);
      Exp *rot_amt = translate_expr(expr->Iex.CCall.args[1], irbb, irout);
      Exp *eflags_in = translate_expr(expr->Iex.CCall.args[2], irbb, irout);

      assert(expr->Iex.CCall.args[3]->tag == Iex_Const);
      assert(expr->Iex.CCall.args[3]->Iex.Const.con->tag == Ico_U32);
      int sz = expr->Iex.CCall.args[3]->Iex.Const.con->Ico.U32;

      irout->push_back(new Comment("x86g_calculate_RCL"));

      // normalize rot_amt
      {
        Exp *old_rot_amt = rot_amt;
        rot_amt = mk_temp(REG_32, irout);
        irout->push_back(new Move(rot_amt->clone(),
                                  new BinOp(MOD, 
                                            old_rot_amt, 
                                            ex_const(sz*8+1))));
      }

      Temp *answer = mk_temp(REG_32, irout);
      Temp *new_eflags = mk_temp(REG_32, irout);
      
      // check for rot by zero
      Label *non_zero = mk_label();
      Label *zero = mk_label();
      Label *out = mk_label();
      irout->push_back(new CJmp(_ex_eq(rot_amt->clone(), ex_const(REG_32, 0)),
                                ex_name(zero->label),
                                ex_name(non_zero->label)));

      // normal handling
      {
        irout->push_back(non_zero);

        // arg >> (sz*8+1 - rot_amt)
        Exp *new_right_part = 
          _ex_shr(arg->clone(),
                  _ex_sub(ex_const(sz*8+1),
                          rot_amt->clone()));

        // (arg << rot_amt) | (cf << (rot_amt - 1)) | new_right_part
        Exp *cf = new Cast(mk_reg("CF", REG_1), REG_32, CAST_UNSIGNED);
        irout->push_back
          (new Move(answer->clone(),
                _ex_or(ex_shl(arg, rot_amt),
                       _ex_or(_ex_shl(cf, 
                                      _ex_sub(rot_amt->clone(), 
                                              ex_const(1))),
                              new_right_part 
                              )
                       )));
        new_right_part = NULL;
        cf = NULL;

        // calculate new eflags
        // arg >> (sz*8-rot_amt)
        Exp *new_cf = new Cast(_ex_shr(arg->clone(), 
                                       _ex_sub(ex_const(sz*8), 
                                               rot_amt->clone())), 
                               REG_1, CAST_LOW);
        // (msb(answer) ^ cf) & 1;
        Exp *of = _ex_xor(new Cast(_ex_shr(answer->clone(), 
                                           ex_const(sz*8-1)),
                                   REG_1,
                                   CAST_LOW),
                          new_cf->clone());
        irout->push_back
          (new Move(new_eflags->clone(),
                    _ex_or(_ex_and(eflags_in->clone(), 
                                   ex_const(~(CF_MASK | OF_MASK))),
                           _ex_or(_ex_shl(new Cast(of, 
                                                   REG_32, 
                                                   CAST_UNSIGNED), 
                                          ex_const(OF_POS)),
                                  _ex_shl(new Cast(new_cf, 
                                                   REG_32, 
                                                   CAST_UNSIGNED),
                                          ex_const(CF_POS))))));
        of = NULL;
        new_cf = NULL;

        irout->push_back(new Jmp(ex_name(out->label)));
      }
      // rot by zero
      {
        irout->push_back(zero);
        irout->push_back(new Move(answer->clone(), arg->clone()));
        irout->push_back(new Move(new_eflags->clone(), eflags_in->clone()));
      }

      // put result together
      irout->push_back(out);
      result = mk_temp(REG_64, irout);
      irout->push_back
        (new Move(result->clone(),
                  _ex_or(_ex_shl(new Cast(new_eflags, REG_64, CAST_UNSIGNED), 
                                 ex_const(REG_64, 32)),
                         new Cast(answer, REG_64, CAST_UNSIGNED))));

      // clean up
      Exp::destroy(arg);
      Exp::destroy(eflags_in);
      Exp::destroy(rot_amt);
    }
    else
    {
      result = new Unknown("CCall: " + func, regt_of_irexpr(irbb, expr));
    }


    delete CF;
    delete ZF;
    delete PF;
    delete SF;
    delete OF;

    return result;
}


static Stmt *translate_put_reg_8( int offset, Exp *data, IRSB *irbb )
{
    assert(data);

    bool low;
    string name;
    Temp *reg;

    // Determine which 32 bit register this 8 bit sub
    // register is a part of
    switch ( offset )
    {
        case OFFB_AL:   name = "EAX";    low = true;    break;
        case OFFB_AH:   name = "EAX";    low = false;   break;
        case OFFB_BL:   name = "EBX";    low = true;    break;
        case OFFB_BH:   name = "EBX";    low = false;   break;
        case OFFB_CL:   name = "ECX";    low = true;    break;
        case OFFB_CH:   name = "ECX";    low = false;   break;
        case OFFB_DL:   name = "EDX";    low = true;    break;
        case OFFB_DH:   name = "EDX";    low = false;   break;

          // Special case for EFLAGS thunk
    case OFFB_CC_DEP1:
      reg = mk_reg("CC_DEP1", REG_32);
      return new Move( reg, _ex_u_cast(data, REG_32) );

        default:
            throw "Unrecognized 8-bit register";
    }   

    // Create the corresponding named register
    reg = mk_reg(name, REG_32);

    Exp *masked = NULL;
    Exp *value = NULL;

    // Assignment to 8 bit sub registers use a combination of bit
    // shifting and masking on the corresponding 32 bit registers
    // to achieve the effect. 
    if ( low )
    {
        masked = new BinOp(BITAND, reg, ex_const(0xffffff00));
        value = new Cast(data, REG_32, CAST_UNSIGNED);
    }
    else
    {
        masked = new BinOp(BITAND, reg, ex_const(0xffff00ff));
        value = new BinOp(LSHIFT, new Cast(data, REG_32, CAST_UNSIGNED), ex_const(8));
    }

    value = new BinOp(BITOR, masked, value); 

    return new Move( new Temp(*reg), value );
}

static Stmt *translate_put_reg_16( int offset, Exp *data, IRSB *irbb )
{
    assert(data);

    string name;
    bool sub;
    Temp *reg;

    switch ( offset )
    {
        //
        // These are 16 bit sub registers
        //
        case OFFB_AX:   name = "EAX";   sub = true; break;
        case OFFB_BX:   name = "EBX";   sub = true; break;
        case OFFB_CX:   name = "ECX";   sub = true; break;
        case OFFB_DX:   name = "EDX";   sub = true; break;
        case OFFB_DI:   name = "EDI";   sub = true; break;
        case OFFB_SI:   name = "ESI";   sub = true; break;
        case OFFB_BP:   name = "EBP";   sub = true; break;
        case OFFB_SP:   name = "ESP";   sub = true; break;

        // 
        // These are regular 16 bit registers
        //
        case OFFB_CS:   name = "CS";    sub = false; break;
        case OFFB_DS:   name = "DS";    sub = false; break;
        case OFFB_ES:   name = "ES";    sub = false; break;
        case OFFB_FS:   name = "FS";    sub = false; break;
        case OFFB_GS:   name = "GS";    sub = false; break;
        case OFFB_SS:   name = "SS";    sub = false; break;

          // Special case for EFLAGS thunk
    case OFFB_CC_DEP1:
      reg = mk_reg("CC_DEP1", REG_32);
      return new Move( reg, _ex_u_cast(data, REG_32) );

        default:
            throw "Unrecognized register offset";
    }

    Exp *masked;
    Exp *value;
    
    if ( sub )
    {
        reg = mk_reg(name, REG_32);

        masked = new BinOp(BITAND, new Temp(*reg), ex_const(0xffff0000));
        value = new Cast(data, REG_32, CAST_UNSIGNED);

        value = new BinOp(BITOR, masked, value);
    }
    else
    {
        reg = mk_reg(name, REG_16);

        value = data;
    }

    return new Move( reg, value );
}

static Stmt *translate_put_reg_32( int offset, Exp *data, IRSB *irbb )
{
    assert(data);
    
    Temp *reg = mk_reg(reg_offset_to_name(offset), REG_32);
    
    return new Move( reg, data );
}

Stmt *i386_translate_put( IRStmt *stmt, IRSB *irbb, vector<Stmt *> *irout )
{
    assert(stmt);
    assert(irbb);
    assert(irout);

    Stmt *result = NULL;

    int offset;
    IRType type;
    Exp *data;

    offset = stmt->Ist.Put.offset;
    type = typeOfIRExpr(irbb->tyenv, stmt->Ist.Put.data);
    data = translate_expr(stmt->Ist.Put.data, irbb, irout);

    // 
    // 8 bit sub registers
    //
    if ( type == Ity_I8 )
    {
        result = translate_put_reg_8(offset, data, irbb);
    }

    //
    // 16 bit registers
    //
    else if ( type == Ity_I16 )
    {
        result = translate_put_reg_16(offset, data, irbb);
    }

    //
    // Regular 32 bit registers
    //
    else if ( type == Ity_I32 )
    {
        result = translate_put_reg_32(offset, data, irbb);
    }

    else
    {
        panic("Unrecognized register type");
    }

    return result;
}





/* FIXME: These are arch specific
//----------------------------------------------------------------------
// Determines if a given instruction is a "special" instruction, 
// basically ones that VEX does not handle
//----------------------------------------------------------------------
bool is_special( Instruction *inst )
{
    assert(inst);

    bool result = false;

    // opcode extension
    char reg = (inst->modrm >> 3) & 7;

    switch ( inst->opcode[0] )
    {
        //
        // HLT
        //
        case 0xF4:  
            result = true;
            break;

        //
        // STMXCSR
        // 
        case 0xAE:
            if ( inst->opcode[1] == 0xF )
                result = true;
            break;

        //
        // CPUID
        //
        case 0xA2:
            if ( inst->opcode[1] == 0x0F )
                result = true;
            break;

        // long indirect jmp
        case 0xFF:
          if (reg == 5)
            result = true;
          break;
    }

    return result;
}

//----------------------------------------------------------------------
// Translate special instructions straight from asm to Vine IR
//----------------------------------------------------------------------
vector<Stmt *> *translate_special( Instruction *inst )
{
    assert(inst);

    vector<Stmt *> *irout = new vector<Stmt *>();    
    assert(irout);

    // opcode extension
    char reg = (inst->modrm >> 3) & 7;

    Stmt *st = NULL;

    switch ( inst->opcode[0] )
    {
        //
        // HLT
        //
        case 0xF4:
            st = new Special("hlt");
            break;

        //
        // STMXCSR
        //
        case 0xAE:
            if ( inst->opcode[1] == 0xF )
                st = new Special("stmxcsr");
            break;

        //
        // CPUID
        //
        case 0xA2:
            if ( inst->opcode[1] == 0x0F )
                st = new Special("cpuid");
            break;

        // long indirect jmp
        case 0xFF:
          if (reg == 5)
            st = new Special("ljmpi");
          break;

        default:
            panic("Why would you call translate_special on something that isn't?");
    }
    irout->push_back(mk_dest_label(inst->address));
    irout->push_back(st);

    return irout;
}
*/


/*
static void add_special_returns(bap_block_t *block)
{
  if(block->inst == NULL) return;
  // If this is a return statement, make note of it
  if(block->inst->opcode[0] == 0xC2 ||
     block->inst->opcode[0] == 0xC3 ||
     block->inst->opcode[0] == 0xCA ||
     block->inst->opcode[0] == 0xCB){
    block->bap_ir->push_back(new Special("ret"));
    block->bap_ir->push_back(mk_label());
  }

}
*/



//======================================================================
//
// Code that deals with the setting of EFLAGS 
//
//======================================================================

//----------------------------------------------------------------------
// 
// Helpers
//
//----------------------------------------------------------------------

void del_get_thunk( asm_program_t *prog, bap_block_t *block )
{
    assert(block);

    vector<Stmt *> rv;
    vector<Stmt *> *ir = block->bap_ir;
    string op = get_op_str(prog, block->inst);

    if (i386_op_is_very_broken(op)) {

      // cerr << "Not deleting get thunks" << endl;
      
      return;
    }

    assert(ir);
    
    for (vector<Stmt*>::iterator
           i = ir->begin(); i != ir->end(); i++)
    {
      Stmt *stmt = (*i);
      rv.push_back(stmt);
        if ( stmt->stmt_type == MOVE )
        {
            Move *move = (Move *)stmt;
            if ( move->rhs->exp_type == TEMP )
            {
                Temp *temp = (Temp *)(move->rhs);
                if (    temp->name.find("CC_OP") != string::npos 
                     || temp->name.find("CC_DEP1") != string::npos 
                     || temp->name.find("CC_DEP2") != string::npos 
                     || temp->name.find("CC_NDEP") != string::npos )
                {
                  // remove and Free the Stmt
                  Stmt::destroy(rv.back());
                  rv.pop_back();
                }
            }
        }
    }
    ir->clear();
    ir->insert(ir->begin(), rv.begin(), rv.end());
}


void get_thunk_index(vector<Stmt *> *ir,
		     int *op,
		     int *dep1,
		     int *dep2,
		     int *ndep,
		     int *mux0x )
{
    assert(ir);

    unsigned int i;

    *op = -1;

    for ( i = 0; i < ir->size(); i++ )
    {
        Stmt *stmt = ir->at(i);
        if ( stmt->stmt_type != MOVE
	     || ((Move*)stmt)->lhs->exp_type != TEMP )
	  continue;

	Temp *temp = (Temp *)((Move*)stmt)->lhs;
	if ( temp->name.find("CC_OP") != string::npos ) {
	  *op = i;
	  if (match_mux0x(ir, (i-MUX_SUB), NULL, NULL, NULL, NULL) >= 0)
	    *mux0x = (i-MUX_SUB);
	}
	else if ( temp->name.find("CC_DEP1") != string::npos )
	  *dep1 = i;
	else if ( temp->name.find("CC_DEP2") != string::npos )
	  *dep2 = i;
	else if ( temp->name.find("CC_NDEP") != string::npos )
	  *ndep = i;
    }
}

//
// Set the bits of EFLAGS using the given flag registers. The flag args are COPIED, not consumed.
//
void set_eflags_bits( vector<Stmt *> *irout, Exp *CF, Exp *PF, Exp *AF, Exp *ZF, Exp *SF, Exp *OF )
{
    CF = new Cast(CF, REG_32, CAST_UNSIGNED );
    PF = new Cast(PF, REG_32, CAST_UNSIGNED );
    AF = new Cast(AF, REG_32, CAST_UNSIGNED );
    ZF = new Cast(ZF, REG_32, CAST_UNSIGNED );
    SF = new Cast(SF, REG_32, CAST_UNSIGNED );
    OF = new Cast(OF, REG_32, CAST_UNSIGNED );

    // All the static constants we'll ever need
    Constant    c_N_CF_MASK(REG_32,~CF_MASK), c_CF_POS(REG_32,CF_POS),
      c_N_PF_MASK(REG_32,~PF_MASK), c_PF_POS(REG_32,PF_POS),
      c_N_AF_MASK(REG_32,~AF_MASK), c_AF_POS(REG_32,AF_POS),
      c_N_ZF_MASK(REG_32,~ZF_MASK), c_ZF_POS(REG_32,ZF_POS),
      c_N_SF_MASK(REG_32,~SF_MASK), c_SF_POS(REG_32,SF_POS),
      c_N_OF_MASK(REG_32,~OF_MASK), c_OF_POS(REG_32,OF_POS);

    // Merge the individual flags together into eflags
    Temp EFLAGS(REG_32,"EFLAGS");

    // Clear all the flag bits
    irout->push_back( new Move( new Temp(EFLAGS), ex_and( &EFLAGS,  &c_N_CF_MASK,
                                                                    &c_N_PF_MASK, 
                                                                    &c_N_AF_MASK, 
                                                                    &c_N_ZF_MASK,
                                                                    &c_N_SF_MASK,
                                                                    &c_N_OF_MASK ) ) );
    // Set all the flag bits
    irout->push_back( new Move( new Temp(EFLAGS), _ex_or( ecl(&EFLAGS), ex_shl(CF, &c_CF_POS),
                                                                        ex_shl(PF, &c_PF_POS),
                                                                        ex_shl(AF, &c_AF_POS),
                                                                        ex_shl(ZF, &c_ZF_POS),
                                                                        ex_shl(SF, &c_SF_POS),
                                                                        ex_shl(OF, &c_OF_POS) )) );

}

void get_eflags_bits(vector<Stmt *> *irout)
{
  Exp *CF = mk_reg("CF", REG_1);
  Exp *PF = mk_reg("PF", REG_1);
  Exp *AF = mk_reg("AF", REG_1);
  Exp *ZF = mk_reg("ZF", REG_1);
  Exp *SF = mk_reg("SF", REG_1);
  Exp *OF = mk_reg("OF", REG_1);

  Temp EFLAGS(REG_32,"EFLAGS");

  irout->push_back(new Move(CF, _ex_l_cast(_ex_shr(_ex_and(ecl(&EFLAGS),
							   ex_const(CF_MASK)),
						   ex_const(CF_POS)),
					   REG_1)));

  irout->push_back(new Move(PF, _ex_l_cast(_ex_shr(_ex_and(ecl(&EFLAGS),
							   ex_const(PF_MASK)),
						   ex_const(PF_POS)),
					   REG_1)));

  irout->push_back(new Move(AF, _ex_l_cast(_ex_shr(_ex_and(ecl(&EFLAGS),
							   ex_const(AF_MASK)),
						   ex_const(AF_POS)),
					   REG_1)));

  irout->push_back(new Move(ZF, _ex_l_cast(_ex_shr(_ex_and(ecl(&EFLAGS),
							   ex_const(ZF_MASK)),
						   ex_const(ZF_POS)),
					   REG_1)));

  irout->push_back(new Move(SF, _ex_l_cast(_ex_shr(_ex_and(ecl(&EFLAGS),
							   ex_const(SF_MASK)),
						   ex_const(SF_POS)),
					   REG_1)));

  irout->push_back(new Move(OF, _ex_l_cast(_ex_shr(_ex_and(ecl(&EFLAGS),
							   ex_const(OF_MASK)),
						   ex_const(OF_POS)),
					   REG_1)));
}

//
// This function CONSUMES the cond and flag expressions passed in, i.e.
// they are not cloned before use.
//
void set_flag( vector<Stmt *> *irout, reg_t type, Temp *flag, Exp *cond )
{
  // set flag directly to condition
  //    irout->push_back( new Move(flag, emit_mux0x(irout, type, cond, ex_const(0), ex_const(1))) );
  irout->push_back( new Move(flag, cond) );
}

//----------------------------------------------------------------------
// 
// Functions that generate IR to modify EFLAGS. These functions all 
// have the form mod_eflags_* where * is the name of an instruction 
// type. They're all similar, yet different enough that they couldn't
// be combined into one function. Their implementation, specifically, 
// the way that the flags are set is based off of the ACTIONS_* macros
// found in VEX/priv/guest-x86/ghelpers.c
//
// The arguments are all COPIED before being used in expression trees.
//
//----------------------------------------------------------------------


/* From the Intel Architecture Software Developer's manual:
   PF (bit 2) Parity flag.
   Set if the least-significant byte of the result contains an even number of
   1 bits; cleared otherwise.
 */

Constant c8_0(REG_8,0), 
  c8_1(REG_8,1), 
  c8_2(REG_8,2), 
  c8_3(REG_8,3), 
  c8_4(REG_8,4), 
  c8_5(REG_8,5), 
  c8_6(REG_8,6), 
  c8_7(REG_8,7);


#define CALC_COND_PF(PF8) (_ex_not(_ex_l_cast(_ex_xor(  ex_shr(PF8, &c8_7),\
						       ex_shr(PF8, &c8_6),\
						       ex_shr(PF8, &c8_5),\
						       ex_shr(PF8, &c8_4),\
						       ex_shr(PF8, &c8_3),\
						       ex_shr(PF8, &c8_2),\
						       ex_shr(PF8, &c8_1),\
						       ecl(PF8) ),\
					     REG_1)))

// used in eflags helpers to get rid of overflowed bits.
// VEX passes REG_32s even if original operands were smaller.
// e should always be of type REG_32.
Exp* mask_overflow(Exp *e, reg_t to) {
  if (REG_32 == to) 
    return e;

  const_val_t mask;
  switch (to) {
  case REG_8:
    mask = 0xff;
    break;
  case REG_16:
    mask = 0xffff;
    break;

    // should have returned if REG_32.
    // should never call with greater than REG_32
  default:
    assert(0);
  }

  return _ex_and(e, ex_const(REG_32, mask));
}

vector<Stmt *> mod_eflags_copy( reg_t type, Exp *arg1, Exp *arg2 )
{
    vector<Stmt *> irout;

    // All the constants we'll ever need
    Constant    c_CF_MASK(REG_32,CF_MASK), 
      c_PF_MASK(REG_32,PF_MASK), 
      c_AF_MASK(REG_32,AF_MASK), 
      c_ZF_MASK(REG_32,ZF_MASK), 
      c_SF_MASK(REG_32,SF_MASK), 
      c_OF_MASK(REG_32,OF_MASK); 

    Temp EFLAGS(REG_32,"EFLAGS");

    irout.push_back( new Move( new Temp(EFLAGS), _ex_and(ecl(arg1), ex_or(  &c_CF_MASK,
                                                                            &c_PF_MASK,
                                                                            &c_AF_MASK,
                                                                            &c_ZF_MASK,
                                                                            &c_SF_MASK,
                                                                            &c_OF_MASK )) ) );

    // set status flags from EFLAGS
    get_eflags_bits(&irout);

    return irout;
}

vector<Stmt *> mod_eflags_add( reg_t type, Exp *arg1, Exp *arg2 )
{
    vector<Stmt *> irout;

    Temp *res = mk_temp(REG_32,&irout);

    // The operation itself
    irout.push_back(new Move(res, mask_overflow(ex_add(arg1, arg2), type)));

    // All the static constants we'll ever need
    Constant    c_0(REG_32,0), 
      c_1(REG_32,1), 
      c_2(REG_32,2), 
      c_3(REG_32,3), 
      c_4(REG_32,4), 
      c_5(REG_32,5), 
      c_6(REG_32,6), c_N1(REG_32,((address_t)-1)), c_TYPE_SIZE_LESS_1(REG_32,get_type_size(type) - 1),
      c_7(REG_32,7), c_0x10(REG_32,0x10);

    // Calculate flags


    Temp *CF = mk_reg("CF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *PF = mk_reg("PF", REG_1);
    Temp *SF = mk_reg("SF", REG_1);
    Temp *OF = mk_reg("OF", REG_1);
    Temp *AF = mk_reg("AF", REG_1);


    Exp *condCF = ex_lt(res, arg1);
    set_flag(&irout, type, CF, condCF);
    
    Temp *PF8 = mk_temp(Ity_I8,&irout);
    Move *m =  new Move(PF8, ex_l_cast(res, REG_8) );
    irout.push_back(m );
    /*
    Exp *condPF = _ex_eq( ecl(&c_0), _ex_and( ecl(&c_1), _ex_xor(  ex_shr(PF8, &c_7),
    Exp *condPF = _ex_eq( ecl(&c_1), _ex_and( ecl(&c_1), _ex_xor(  ex_shr(PF8, &c_7),
                                                                   ex_shr(PF8, &c_6),
                                                                   ex_shr(PF8, &c_5),
                                                                   ex_shr(PF8, &c_4),
                                                                   ex_shr(PF8, &c_3),
                                                                   ex_shr(PF8, &c_2),
                                                                   ex_shr(PF8, &c_1),
                                                                      ecl(PF8) ) ) );
    */
    Exp *condPF = CALC_COND_PF(PF8);

    set_flag(&irout, type, PF, condPF);

    Exp *condAF = _ex_eq( ecl(&c_1), _ex_and( ecl(&c_0x10), ex_xor( res, arg1, arg2) ));
    set_flag(&irout, type, AF, condAF);

    Exp *condZF = ex_eq( res, &c_0 );
    set_flag(&irout, type, ZF, condZF);
    
    Exp *condSF = _ex_eq( ecl(&c_1), _ex_and( ecl(&c_1), ex_shr( res, &c_TYPE_SIZE_LESS_1)) );
    set_flag(&irout, type, SF, condSF);

    Exp *condOF = _ex_eq( ecl(&c_1), _ex_and( ecl(&c_1), 
                    _ex_shr( _ex_and( ex_xor(arg1, arg2, &c_N1), ex_xor(arg1, res) ), ecl(&c_TYPE_SIZE_LESS_1) )) );
    set_flag(&irout, type, OF, condOF);

    // We construct EFLAGS in total only in case that
    // instructions directly need EFLAGS as a complete register such as pushf.
    //set_eflags_bits(&irout, CF, PF, AF, ZF, SF, OF);

    return irout;
}

vector<Stmt *> mod_eflags_sub( reg_t type, Exp *arg1, Exp *arg2 )
{
    vector<Stmt *> irout;

    Temp *res = mk_temp(REG_32,&irout);

    // The operation itself
    irout.push_back( new Move(res, mask_overflow(ex_sub(arg1, arg2), type)));

    // All the static constants we'll ever need
    Constant    c_0(REG_32,0), 
      c_1(REG_32,1), 
      c_2(REG_32,2), 
      c_3(REG_32,3), 
      c_4(REG_32,4), 
      c_5(REG_32,5), 
      c_6(REG_32,6), c_TYPE_SIZE_LESS_1(REG_32,get_type_size(type) - 1),
      c_7(REG_32,7), c_0x10(REG_32,0x10);
    Constant    c_N1(REG_32,((address_t) -1));

    // Calculate flags

    Temp *CF = mk_reg("CF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *PF = mk_reg("PF", REG_1);
    Temp *SF = mk_reg("SF", REG_1);
    Temp *OF = mk_reg("OF", REG_1);
    Temp *AF = mk_reg("AF", REG_1);
    Exp *condCF = ex_lt(arg1, arg2);
    set_flag(&irout, type, CF, condCF);
    
    Temp *PF8 = mk_temp(Ity_I8,&irout);
    irout.push_back( new Move(PF8, ex_l_cast(res, REG_8) ) );
    Exp *condPF = CALC_COND_PF(PF8);
    set_flag(&irout, type, PF, condPF);

    /* FIXME: (1 == ( 0x10 & foo)) is always false */
    Exp *condAF = _ex_eq( ecl(&c_1), _ex_and( ecl(&c_0x10), ex_xor( res, arg1, arg2) ));
    set_flag(&irout, type, AF, condAF);

    Exp *condZF = ex_eq( res, &c_0 );
    set_flag(&irout, type, ZF, condZF);
    
    Exp *condSF = _ex_eq( ecl(&c_1), _ex_and( ecl(&c_1), ex_shr( res, &c_TYPE_SIZE_LESS_1)) );
    set_flag(&irout, type, SF, condSF);

    Exp *condOF = _ex_eq( ecl(&c_1), _ex_and( ecl(&c_1), 
                    _ex_shr( _ex_and( ex_xor(arg1, arg2), ex_xor(arg1, res) ), ecl(&c_TYPE_SIZE_LESS_1) )) );
    set_flag(&irout, type, OF, condOF);

    // We construct EFLAGS in total only in case that
    // instructions directly need EFLAGS as a complete register such as pushf.
    //set_eflags_bits(&irout, CF, PF, AF, ZF, SF, OF);

    return irout;
}

vector<Stmt *> mod_eflags_adc( reg_t type, Exp *arg1, Exp *arg2, Exp *arg3 )
{
    vector<Stmt *> irout;

    Constant c_CF_MASK(REG_32,CF_MASK);

    Temp *res = mk_temp(REG_32,&irout);

    // (from VEX's gdefs.h) 
    // arg1 = first arg          
    // arg2 = (second arg) XOR old_carry
    // arg3 = old_carry

    // Recover the args
    arg3 = _ex_and( arg3, &c_CF_MASK );
    arg2 = _ex_xor( arg2, arg3 );

    // The operation itself
    irout.push_back(new Move(res, mask_overflow(_ex_add(ex_add(arg1, arg2), 
                                                        ecl(arg3)), type)));

    // All the static constants we'll ever need
    Constant    c_0(REG_32,0), 
      c_1(REG_32,1), 
      c_2(REG_32,2), 
      c_3(REG_32,3), 
      c_4(REG_32,4), 
      c_5(REG_32,5), 
      c_6(REG_32,6), c_N1(REG_32,((address_t) -1)), 
      c_TYPE_SIZE_LESS_1(REG_32,get_type_size(type) - 1),
      c_7(REG_32,7), c_0x10(REG_32,0x10);

    // Calculate flags

//     Temp *CF = mk_temp("CF", Ity_I1);
//     Exp *condOldC = ex_eq(arg3, &c_0);
//     Exp *condCF = ex_le(res, arg1);
//     set_flag(&irout, type, CF, condCF);
//     Label *doneCF = mk_label();
//     Label *resetCF = mk_label();
//     irout.push_back( new CJmp(condOldC, new Name(doneCF->label), new Name(resetCF->label)) );
//     irout.push_back( resetCF );
//     condCF = ex_lt(res, arg1);
//     set_flag(&irout, type, CF, condCF);
//     irout.push_back( doneCF );

    Temp *CF = mk_reg("CF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *PF = mk_reg("PF", REG_1);
    Temp *SF = mk_reg("SF", REG_1);
    Temp *OF = mk_reg("OF", REG_1);
    Temp *AF = mk_reg("AF", REG_1);


    // (arg3==1) & (res <= arg1) | (arg3==0) & (res<arg1)
    Exp *condCF = _ex_or(_ex_and(ex_eq(arg3,&c_1), 
				 ex_le(res,arg1)), 
			 _ex_and(ex_eq(arg3,&c_0), 
				 ex_lt(res,arg1)));
    set_flag(&irout, type, CF, condCF);


    
    Temp *PF8 = mk_temp(Ity_I8,&irout);
    irout.push_back( new Move(PF8, ex_l_cast(res, REG_8) ) );
    Exp *condPF = CALC_COND_PF(PF8);
    set_flag(&irout, type, PF, condPF);

    Exp *condAF = _ex_eq( ecl(&c_1), _ex_and( ecl(&c_0x10), ex_xor( res, arg1, arg2) ));
    set_flag(&irout, type, AF, condAF);

    Exp *condZF = ex_eq( res, &c_0 );
    set_flag(&irout, type, ZF, condZF);
    
    Exp *condSF = _ex_eq( ecl(&c_1), _ex_and( ecl(&c_1), ex_shr( res, &c_TYPE_SIZE_LESS_1)) );
    set_flag(&irout, type, SF, condSF);

    Exp *condOF = _ex_eq( ecl(&c_1), _ex_and( ecl(&c_1), 
                    _ex_shr( _ex_and( ex_xor(arg1, arg2, &c_N1), ex_xor(arg1, res) ), ecl(&c_TYPE_SIZE_LESS_1) )) );
    set_flag(&irout, type, OF, condOF);

    // We construct EFLAGS in total only in case that
    // instructions directly need EFLAGS as a complete register such as pushf.
    //set_eflags_bits(&irout, CF, PF, AF, ZF, SF, OF);

    delete arg2;
    delete arg3;

    return irout;
}

vector<Stmt *> mod_eflags_sbb( reg_t type, Exp *arg1, Exp *arg2, Exp *arg3 )
{
    vector<Stmt *> irout;

    Constant c_CF_MASK(REG_32,CF_MASK);

    Temp *res = mk_temp(REG_32,&irout);

    // Recover the args
    arg3 = _ex_and( arg3, &c_CF_MASK );
    arg2 = _ex_xor( arg2, arg3 );

    // The operation itself
    irout.push_back( new Move(res, mask_overflow(_ex_sub(ex_sub(arg1, arg2), 
                                                         ecl(arg3)), type)));

    // All the static constants we'll ever need
    Constant    c_0(REG_32,0), 
      c_1(REG_32,1), 
      c_2(REG_32,2), 
      c_3(REG_32,3), 
      c_4(REG_32,4), 
      c_5(REG_32,5), 
      c_6(REG_32,6), 
      c_N1(REG_32,((address_t)-1)), 
      c_TYPE_SIZE_LESS_1(REG_32,get_type_size(type) - 1),
      c_7(REG_32,7), c_0x10(REG_32,0x10);

    // Calculate flags

//     Temp *CF = mk_temp("CF", Ity_I1);
//     Exp *condOldC = ex_eq(arg3, &c_0);
//     Exp *condCF = ex_le(arg1, arg2);
//     set_flag(&irout, type, CF, condCF);
//     Label *doneCF = mk_label();
//     Label *resetCF = mk_label();
//     irout.push_back( new CJmp(condOldC, new Name(doneCF->label), new Name(resetCF->label)) );
//     irout.push_back( resetCF );
//     condCF = ex_lt(arg1, arg2);
//     set_flag(&irout, type, CF, condCF);
//     irout.push_back( doneCF );
    Temp *CF = mk_reg("CF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *PF = mk_reg("PF", REG_1);
    Temp *SF = mk_reg("SF", REG_1);
    Temp *OF = mk_reg("OF", REG_1);
    Temp *AF = mk_reg("AF", REG_1);


    // (arg3==0) & (arg1 <= arg2) | (not(arg3==0)) & (arg1<arg2)
    Exp *condCF = _ex_or(_ex_and(ex_eq(arg3,&c_0), 
				 ex_le(arg1,arg2)), 
			 _ex_and(_ex_not(ex_eq(arg3,&c_0)), 
				 ex_lt(arg1,arg2)));
    set_flag(&irout, type, CF, condCF);

    
    Temp *PF8 = mk_temp(Ity_I8,&irout);
    irout.push_back( new Move(PF8, ex_l_cast(res, REG_8) ) );
    Exp *condPF = CALC_COND_PF(PF8);
    set_flag(&irout, type, PF, condPF);

    Exp *condAF = _ex_eq( ecl(&c_1), _ex_and( ecl(&c_0x10), ex_xor( res, arg1, arg2) ));
    set_flag(&irout, type, AF, condAF);

    Exp *condZF = ex_eq( res, &c_0 );
    set_flag(&irout, type, ZF, condZF);
    
    Exp *condSF = _ex_eq( ecl(&c_1), _ex_and( ecl(&c_1), ex_shr( res, &c_TYPE_SIZE_LESS_1)) );
    set_flag(&irout, type, SF, condSF);

    Exp *condOF = _ex_eq( ecl(&c_1), _ex_and( ecl(&c_1), 
                    _ex_shr( _ex_and( ex_xor(arg1, arg2), ex_xor(arg1, res) ), ecl(&c_TYPE_SIZE_LESS_1) )) );
    set_flag(&irout, type, OF, condOF);

    // We construct EFLAGS in total only in case that
    // instructions directly need EFLAGS as a complete register such as pushf.
    //set_eflags_bits(&irout, CF, PF, AF, ZF, SF, OF);

    delete arg2;
    delete arg3;

    return irout;
}

vector<Stmt *> mod_eflags_logic( reg_t type, Exp *arg1, Exp *arg2 )
{
    vector<Stmt *> irout;

    Exp *res = arg1;

    // All the static constants we'll ever need
    Constant    c_0(REG_32,0), 
      c_1(REG_32,1), 
      c_2(REG_32,2), 
      c_3(REG_32,3), 
      c_4(REG_32,4), 
      c_5(REG_32,5), 
      c_6(REG_32,6), c_N1(REG_32,((address_t) -1)), 
      c_TYPE_SIZE_LESS_1(REG_32,get_type_size(type) - 1),
      c_7(REG_32,7), c_0x10(REG_32,0x10);

    // Calculate flags

    Temp *CF = mk_reg("CF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *PF = mk_reg("PF", REG_1);
    Temp *SF = mk_reg("SF", REG_1);
    Temp *OF = mk_reg("OF", REG_1);
    Temp *AF = mk_reg("AF", REG_1);

    irout.push_back(new Move(CF, Constant::f.clone()));
    
    Temp *PF8 = mk_temp(Ity_I8,&irout);
    irout.push_back( new Move(PF8, ex_l_cast(res, REG_8) ) );
    Exp *condPF = CALC_COND_PF(PF8);
    set_flag(&irout, type, PF, condPF);

    irout.push_back( new Move(AF, Constant::f.clone()) );    

    Exp *condZF = ex_eq( res, &c_0 );
    set_flag(&irout, type, ZF, condZF);
    
    Exp *condSF = _ex_eq( ecl(&c_1), _ex_and( ecl(&c_1), ex_shr( res, &c_TYPE_SIZE_LESS_1)) );
    set_flag(&irout, type, SF, condSF);

    irout.push_back( new Move(OF, Constant::f.clone()) );    

    // We construct EFLAGS in total only in case that
    // instructions directly need EFLAGS as a complete register such as pushf.
    //set_eflags_bits(&irout, CF, PF, AF, ZF, SF, OF);

    return irout;
}

vector<Stmt *> mod_eflags_inc( reg_t type, Exp *arg1, Exp *arg2, Exp *arg3 )
{
    vector<Stmt *> irout;

    int type_size = get_type_size(type);
    Exp *res = arg1;

    // All the static constants we'll ever need
    Constant    
      c_0(REG_32,0), 
      //      c_CF_POS(REG_32,CF_POS),
      c_1(REG_32,1), 
      c_2(REG_32,2), 
      c_3(REG_32,3), 
      c_4(REG_32,4), 
      c_5(REG_32,5), 
      c_6(REG_32,6), 
      c_7(REG_32,7), 
      c_TYPE_SIZE_LESS_1(REG_32,type_size - 1),
      c_0x10(REG_32,0x10);

    Constant    c_SIGN_MASK = 
      Constant(REG_32, 1 << (type_size - 1) );
    Constant    c_DATA_MASK = 
      Constant(REG_32, 
	       type_size == 8 
	       ? 0xff 
	       : (type_size == 16 
		  ? 0xffff 
		  : 0xffffffff) );

    Exp *argL = _ex_sub(res, &c_1);
    Exp *argR = &c_1;

    // Calculate flags

    // no-op
    //    irout.push_back( new Move(CF, _ex_and(ex_shr(arg3, &c_CF_POS), ecl(&c_1))));
    //    irout.push_back( new Move(CF, 
    //			      new Cast(ex_shr(arg3, &c_CF_POS), 
    //				       I1,
    //				       CAST_LOW)));

    Temp *CF = mk_reg("CF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *PF = mk_reg("PF", REG_1);
    Temp *SF = mk_reg("SF", REG_1);
    Temp *OF = mk_reg("OF", REG_1);
    Temp *AF = mk_reg("AF", REG_1);

    Temp *PF8 = mk_temp(Ity_I8,&irout);
    irout.push_back( new Move(PF8, ex_l_cast(res, REG_8) ) );
    Exp *condPF = CALC_COND_PF(PF8);
    set_flag(&irout, type, PF, condPF);

    Exp *condAF = _ex_eq( ecl(&c_1), _ex_and( ecl(&c_0x10), ex_xor( res, argL, argR) ));
    set_flag(&irout, type, AF, condAF);

    Exp *condZF = ex_eq( res, &c_0 );
    set_flag(&irout, type, ZF, condZF);
    
    Exp *condSF = _ex_eq( ecl(&c_1), _ex_and( ecl(&c_1), ex_shr( res, &c_TYPE_SIZE_LESS_1)) );
    set_flag(&irout, type, SF, condSF);

    Exp *condOF = _ex_eq( ex_and(res, &c_DATA_MASK), ecl(&c_SIGN_MASK) );
    set_flag(&irout, type, OF, condOF);

    // We construct EFLAGS in total only in case that
    // instructions directly need EFLAGS as a complete register such as pushf.
    //set_eflags_bits(&irout, CF, PF, AF, ZF, SF, OF);

    delete argL;

    return irout;
}

vector<Stmt *> mod_eflags_dec( reg_t type, Exp *arg1, Exp *arg2, Exp *arg3 )
{
    vector<Stmt *> irout;

    int type_size = get_type_size(type);
    Exp *res = arg1;

    // All the static constants we'll ever need
    Constant    c_0(REG_32,0), c_CF_POS(REG_32,CF_POS),
      c_1(REG_32,1), 
      c_2(REG_32,2), 
      c_3(REG_32,3), 
      c_4(REG_32,4), 
      c_5(REG_32,5), 
      c_6(REG_32,6), c_N1(REG_32,((address_t)-1)), 
      c_TYPE_SIZE_LESS_1(REG_32,type_size - 1),
      c_7(REG_32,7), c_0x10(REG_32,0x10);

    Constant    c_SIGN_MASK = Constant(REG_32, 1 << (type_size - 1) );
    Constant    c_DATA_MASK = Constant(REG_32, type_size == 8 ? 0xff : (type_size == 16 ? 0xffff : 0xffffffff) );

    //    res = _ex_u_cast(res, REG_32);
    Exp *argL = _ex_add(res, &c_1);
    Exp *argR = &c_1;

    // Calculate flags

    Temp *CF = mk_reg("CF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *PF = mk_reg("PF", REG_1);
    Temp *SF = mk_reg("SF", REG_1);
    Temp *OF = mk_reg("OF", REG_1);
    Temp *AF = mk_reg("AF", REG_1);

    irout.push_back( new Move(CF, _ex_and(_ex_l_cast(ex_shr(arg3, &c_CF_POS), REG_1), Constant::t.clone()) ) );    
    
    Temp *PF8 = mk_temp(Ity_I8,&irout);
    irout.push_back( new Move(PF8, ex_l_cast(res, REG_8) ) );
    Exp *condPF = CALC_COND_PF(PF8);
    set_flag(&irout, type, PF, condPF);

    Exp *condAF = _ex_eq( ecl(&c_1), _ex_and( ecl(&c_0x10), ex_xor( res, argL, argR) ));
    set_flag(&irout, type, AF, condAF);

    Exp *condZF = ex_eq( res, &c_0 );
    set_flag(&irout, type, ZF, condZF);
    
    Exp *condSF = _ex_eq( ecl(&c_1), _ex_and( ecl(&c_1), ex_shr( res, &c_TYPE_SIZE_LESS_1)) );
    set_flag(&irout, type, SF, condSF);

    Exp *condOF = _ex_eq( ex_and(res, &c_DATA_MASK), ex_sub(&c_SIGN_MASK, &c_1) );
    set_flag(&irout, type, OF, condOF);

    // We construct EFLAGS in total only in case that
    // instructions directly need EFLAGS as a complete register such as pushf.
    //set_eflags_bits(&irout, CF, PF, AF, ZF, SF, OF);

    delete argL;

    return irout;
}

vector<Stmt *> mod_eflags_shl( reg_t type, Exp *arg1, Exp *arg2 )
{
    vector<Stmt *> irout;

    int type_size = get_type_size(type);
    Exp *res = arg1;
    Exp *count_expr  = arg2->clone();

    // All the static constants we'll ever need
    Constant    c_0(REG_32,0), c_CF_MASK(REG_32,CF_MASK),
      c_1(REG_32,1), 
      c_2(REG_32,2), 
      c_3(REG_32,3), 
      c_4(REG_32,4), 
      c_5(REG_32,5), 
      c_6(REG_32,6), c_N1(REG_32,((address_t)-1)), c_TYPE_SIZE_LESS_1(REG_32,type_size - 1),
                c_7(REG_32,7), c_0x10(REG_32,0x10);

    // Calculate flags
    Temp *CF = mk_reg("CF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *PF = mk_reg("PF", REG_1);
    Temp *SF = mk_reg("SF", REG_1);
    Temp *OF = mk_reg("OF", REG_1);
    Temp *AF = mk_reg("AF", REG_1);

    Label *ifcount0 = mk_label(); 
    Label *ifcountn0 = mk_label(); 
    
    if (!use_eflags_thunks) {
	if (count_opnd) {
	    
	    Constant c0 (REG_8, 0);
	    Exp *cond = ex_eq (count_opnd, &c0); 
	    irout.push_back(new CJmp(cond, ex_name(ifcountn0->label), 
				     ex_name(ifcount0->label))); 
	    irout.push_back (ifcount0);
	} else 
	    assert (0);
    }
    
    irout.push_back( new Move(CF, _ex_and(_ex_l_cast(ex_shr(arg2, &c_TYPE_SIZE_LESS_1), REG_1), Constant::t.clone()) ) );    
    
    Temp *PF8 = mk_temp(Ity_I8,&irout);
    irout.push_back( new Move(PF8, ex_l_cast(res, REG_8) ) );
    Exp *condPF = CALC_COND_PF(PF8);
    set_flag(&irout, type, PF, condPF);

    irout.push_back( new Move(AF, Constant::f.clone()) );    

    Exp *condZF = ex_eq( res, &c_0 );
    set_flag(&irout, type, ZF, condZF);
    
    Exp *condSF = _ex_eq( ecl(&c_1), _ex_and( ecl(&c_1), ex_shr( res, &c_TYPE_SIZE_LESS_1)) );
    set_flag(&irout, type, SF, condSF);

    Exp *condOF = _ex_and(_ex_l_cast(_ex_shr(ex_xor(arg1, arg2), ecl(&c_TYPE_SIZE_LESS_1)), REG_1), Constant::t.clone());
    set_flag(&irout, type, OF, condOF);

    // We construct EFLAGS in total only in case that
    // instructions directly need EFLAGS as a complete register such as pushf.
    //set_eflags_bits(&irout, CF, PF, AF, ZF, SF, OF);

    if (!use_eflags_thunks)
	irout.push_back (ifcountn0);

    return irout;
}

vector<Stmt *> mod_eflags_shr( reg_t type, Exp *arg1, Exp *arg2 )
{
    vector<Stmt *> irout;

    int type_size = get_type_size(type);
    Exp *res = arg1;

    // All the static constants we'll ever need
    Constant    c_0(REG_32,0), c_CF_MASK(REG_32,CF_MASK),
                c_1(REG_32,1), 
                c_2(REG_32,2), 
                c_3(REG_32,3), 
                c_4(REG_32,4), 
                c_5(REG_32,5), 
                c_6(REG_32,6), c_N1(REG_32,((address_t)-1)), c_TYPE_SIZE_LESS_1(REG_32,type_size - 1),
                c_7(REG_32,7), c_0x10(REG_32,0x10);


    // Calculate flags

    Temp *CF = mk_reg("CF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *PF = mk_reg("PF", REG_1);
    Temp *SF = mk_reg("SF", REG_1);
    Temp *OF = mk_reg("OF", REG_1);
    Temp *AF = mk_reg("AF", REG_1);

    Label *ifcount0 = mk_label(); 
    Label *ifcountn0 = mk_label(); 

    if (!use_eflags_thunks) {
	if (count_opnd) {
	    Constant c0 (REG_8, 0);
	    Exp *cond = ex_eq (count_opnd, &c0);
	    irout.push_back(new CJmp(cond, ex_name(ifcount0->label), 
				     ex_name(ifcountn0->label))); 
	    irout.push_back (ifcountn0);
	} else {
          /* If the count is an immediate that is 0 modulo 32, then
             VEX never performs a binop, and thus never sets
             count_opnd.  I am assuming that if this happens the count
             is 0. */
          irout.push_back(new Jmp(ex_name(ifcount0->label)));
        }
    }


    irout.push_back( new Move(CF, ex_l_cast(arg2, REG_1) ) );    
    
    Temp *PF8 = mk_temp(Ity_I8,&irout);
    irout.push_back( new Move(PF8, ex_l_cast(res, REG_8) ) );
    Exp *condPF = CALC_COND_PF(PF8);
    set_flag(&irout, type, PF, condPF);

    irout.push_back( new Move(AF, Constant::f.clone()) );    

    Exp *condZF = ex_eq( res, &c_0 );
    set_flag(&irout, type, ZF, condZF);
    
    Exp *condSF = _ex_eq( ecl(&c_1), _ex_and( ecl(&c_1), ex_shr( res, &c_TYPE_SIZE_LESS_1)) );
    set_flag(&irout, type, SF, condSF);

    // FIXME: this is wrong for sar. should set OF to 0.
    Exp *condOF = _ex_and(_ex_l_cast(_ex_shr(ex_xor(arg1, arg2), ecl(&c_TYPE_SIZE_LESS_1)), REG_1), Constant::t.clone());
    set_flag(&irout, type, OF, condOF);

    
    
    // We construct EFLAGS in total only in case that
    // instructions directly need EFLAGS as a complete register such as pushf.
    //set_eflags_bits(&irout, CF, PF, AF, ZF, SF, OF);

    if (!use_eflags_thunks) 
	irout.push_back (ifcount0);
    return irout;
}

// FIXME: should not modify flags when shifting by zero
vector<Stmt *> mod_eflags_rol( reg_t type, Exp *arg1, Exp *arg2, Exp *arg3 )
{
    vector<Stmt *> irout;

    int type_size = get_type_size(type);

    // All the static constants we'll ever need
    Constant    c_0(REG_32,0), c_CF_MASK(REG_32,CF_MASK),
                c_1(REG_32,1), c_OF_MASK(REG_32,OF_MASK),
                c_11(REG_32,11),
                c_OF_MAGIC_NUMBER(REG_32,11 - (type_size - 1));

    // Calculate flags

    Exp *oldFlags = _ex_and( ecl(arg3), _ex_not( ex_or(&c_OF_MASK, &c_CF_MASK) ) );

    Temp *CF = mk_reg("CF", REG_1);
    Temp *ZF = mk_reg("ZF", REG_1);
    Temp *PF = mk_reg("PF", REG_1);
    Temp *SF = mk_reg("SF", REG_1);
    Temp *OF = mk_reg("OF", REG_1);
    Temp *AF = mk_reg("AF", REG_1);

    // lsb of result
    set_flag(&irout, type, CF, _ex_l_cast(ecl(arg1),
    					  REG_1));

    //    Exp *OF = _ex_and( ecl(&c_OF_MASK), _ex_xor( ex_shl(arg1, &c_OF_MAGIC_NUMBER), ex_shl(arg1, &c_11) ) );
    // new OF flag xor msb of result
    set_flag(&irout, type, OF, _ex_xor(ecl(CF),
				       _ex_l_cast(ex_shr(arg1, 
							 ex_const(type_size-1)),
						  REG_1)));
    //    set_flag(&irout, type, OF, _ex_l_cast(_ex_and( ecl(&c_OF_MASK), 
    //						   _ex_xor( ex_shl(arg1, 
    //								   &c_OF_MAGIC_NUMBER), 
    //							    ex_shl(arg1, &c_11))),
    //					  REG_1));

    // We construct EFLAGS in total only in case that
    // instructions directly need EFLAGS as a complete register such as pushf.
//    // set only CF and OF (which were cleared above)
//    Temp EFLAGS(REG_32,"EFLAGS");
//    irout.push_back( new Move( new Temp(EFLAGS),
//			       ex_or( oldFlags, 
//                                      // don't shift CF becaus it's pos 1
//                                      _ex_or(_ex_u_cast(CF, REG_32),
//                                             _ex_shl(_ex_u_cast(OF, REG_32),
//                                                     ex_const(REG_32, OF_POS))))));

//    destroy(oldFlags);
//    destroy(CF);
//    destroy(OF);

    return irout;
}

// FIXME: should not modify flags when shifting by zero
vector<Stmt *> mod_eflags_ror( reg_t type, Exp *arg1, Exp *arg2, Exp *arg3 )
{
    vector<Stmt *> irout;

    int type_size = get_type_size(type);

    // All the static constants we'll ever need
    Constant    c_0(REG_32,0), c_CF_MASK(REG_32,CF_MASK),
                c_1(REG_32,1), c_OF_MASK(REG_32,OF_MASK),
                c_11(REG_32,11),
                c_TYPE_SIZE_LESS_1(REG_32,type_size - 1),
                c_OF_MAGIC_NUMBER(REG_32,11 - (type_size - 1)),
                c_OF_MAGIC_NUMBER_2(REG_32,11 - (type_size - 1) + 1);

    // Calculate flags

    Exp *oldFlags = _ex_and( ecl(arg3), _ex_not( ex_or(&c_OF_MASK, &c_CF_MASK) ) );

    // CF is set to msb of result
    Temp *CF = mk_reg("CF", REG_1);
    set_flag(&irout, type, CF, _ex_l_cast(ex_shr(arg1, &c_TYPE_SIZE_LESS_1),
					  REG_1));

    // OF is set to xor of two most significant bits of result
    //    Exp *OF = _ex_and( ecl(&c_OF_MASK), _ex_xor( ex_shl(arg1, &c_OF_MAGIC_NUMBER), ex_shl(arg1, &c_OF_MAGIC_NUMBER_2) ) );
    Temp *OF = mk_reg("OF", REG_1);
    //    set_flag(&irout, type, OF, _ex_and( ecl(&c_OF_MASK), _ex_xor( ex_shl(arg1, &c_OF_MAGIC_NUMBER), ex_shl(arg1, &c_OF_MAGIC_NUMBER_2) ) ));
    set_flag(&irout, type, OF, _ex_xor(ecl(CF),
				       _ex_l_cast(_ex_shr(ecl(arg1),
							  ex_const(type_size-2)
							  ),
						  REG_1)
				       ));

    // We construct EFLAGS in total only in case that
    // instructions directly need EFLAGS as a complete register such as pushf.
    // set only CF and OF (which were cleared above)
//    Temp EFLAGS(REG_32,"EFLAGS");
//    irout.push_back( new Move( new Temp(EFLAGS),
//			       ex_or( oldFlags, 
//                                      // don't shift CF becaus it's pos 1
//                                      _ex_or(_ex_u_cast(CF, REG_32),
//                                             _ex_shl(_ex_u_cast(OF, REG_32),
//                                                     ex_const(REG_32, OF_POS))))));


//    destroy(oldFlags);
//    destroy(CF);
//    destroy(OF);

    return irout;
}

vector<Stmt *> mod_eflags_umul( reg_t type, Exp *arg1, Exp *arg2 )
{
    vector<Stmt *> irout;

    int type_size = get_type_size(type);

    // All the static constants we'll ever need
    Constant    c_0(REG_32,0), 
                c_1(REG_32,1), 
                c_2(REG_32,2), 
                c_3(REG_32,3), 
                c_4(REG_32,4), 
                c_5(REG_32,5), 
                c_6(REG_32,6), c_TYPE_SIZE_LESS_1(type,type_size - 1),
                c_7(REG_32,7);

    // Calculate flags
    Temp *res = NULL;
    Temp *lo = NULL;
    Temp *hi = NULL;

    reg_t res_type;

    // Figure out what types to use
    if ( type == REG_8 )
    {
      res_type = REG_16;
    }
    else if ( type == REG_16 )
    {
      res_type = REG_32;
    }
    else if ( type == REG_32 )
    {
      res_type = REG_64;
    }
    res = mk_temp(res_type,&irout);
    lo = mk_temp(type,&irout);
    hi = mk_temp(type,&irout);

    irout.push_back( new Move(res, _ex_mul(_ex_u_cast(ex_l_cast(arg1, type), 
                                                      res->typ),
                                           _ex_u_cast(ex_l_cast(arg2, type), 
                                                      res->typ) )));
    irout.push_back( new Move(lo, ex_l_cast(res, lo->typ)) );
    irout.push_back( new Move(hi, ex_h_cast(res, hi->typ)) );

    Temp *CF = mk_reg("CF", REG_1);
    Exp *condCF = _ex_neq( ecl(hi), ex_const(type, 0));
    set_flag(&irout, type, CF, condCF);
    
    Temp *PF = mk_reg("PF", REG_1);
    Temp *PF8 = mk_temp(Ity_I8,&irout);
    irout.push_back( new Move(PF8, ex_l_cast(res, REG_8) ) );
    Exp *condPF = CALC_COND_PF(PF8);
    set_flag(&irout, type, PF, condPF);

    Temp *AF = mk_reg("AF", REG_1);
    irout.push_back( new Move(AF, Constant::f.clone()) );    

    Temp *ZF = mk_reg("ZF", REG_1);
    Exp *condZF = _ex_eq( ecl(lo), ex_const(type, 0));
    set_flag(&irout, type, ZF, condZF);
    
    Temp *SF = mk_reg("SF", REG_1);
    Exp *condSF = _ex_l_cast(ex_shr( lo, &c_TYPE_SIZE_LESS_1), REG_1);
    set_flag(&irout, type, SF, condSF);

    Temp *OF = mk_reg("OF", REG_1);
    irout.push_back( new Move(OF, new Temp(*CF)) );

    // We construct EFLAGS in total only in case that
    // instructions directly need EFLAGS as a complete register such as pushf.
    //set_eflags_bits(&irout, CF, PF, AF, ZF, SF, OF);

    return irout;
}

vector<Stmt *> mod_eflags_smul( reg_t type, Exp *arg1, Exp *arg2 )
{
    vector<Stmt *> irout;

    int type_size = get_type_size(type);

    // All the static constants we'll ever need
    Constant    c_0(REG_32,0), 
                c_1(REG_32,1), 
                c_2(REG_32,2), 
                c_3(REG_32,3), 
                c_4(REG_32,4), 
                c_5(REG_32,5), 
                c_6(REG_32,6), c_TYPE_SIZE_LESS_1(type,type_size - 1),
                c_7(REG_32,7);

    // Calculate flags
    Temp *res = NULL;
    Temp *lo = NULL;
    Temp *hi = NULL;

    // Figure out what types to use
    if ( type == REG_8 )
    {
      res = mk_temp(REG_16, &irout);
      lo = mk_temp(REG_8,&irout);
      hi = mk_temp(REG_8,&irout);
    }
    else if ( type == REG_16 )
    {
      res = mk_temp(REG_32,&irout);
      lo = mk_temp(REG_16,&irout);
      hi = mk_temp(REG_16,&irout);
    }
    else if ( type == REG_32 )
    {
      res = mk_temp(REG_64,&irout);
      lo = mk_temp(REG_32,&irout);
      hi = mk_temp(REG_32,&irout);
    }

    irout.push_back( new Move(res, _ex_mul(_ex_s_cast(ex_l_cast(arg1, type), 
                                                     res->typ),
                                           _ex_s_cast(ex_l_cast(arg2, type), 
                                                     res->typ) )));

    irout.push_back( new Move(lo, ex_l_cast(res, lo->typ)) );
    irout.push_back( new Move(hi, ex_h_cast(res, hi->typ)) );

    Temp *CF = mk_reg("CF", REG_1);
    Exp *condCF = _ex_neq( ecl(hi), ex_sar(lo, &c_TYPE_SIZE_LESS_1) );
    set_flag(&irout, type, CF, condCF);
    
    Temp *PF = mk_reg("PF", REG_1);
    Temp *PF8 = mk_temp(Ity_I8,&irout);
    irout.push_back( new Move(PF8, ex_l_cast(res, REG_8) ) );
    Exp *condPF = CALC_COND_PF(PF8);
    set_flag(&irout, type, PF, condPF);

    Temp *AF = mk_reg("AF", REG_1);
    irout.push_back( new Move(AF, Constant::f.clone()) );    

    Temp *ZF = mk_reg("ZF", REG_1);
    Exp *condZF = _ex_eq( ecl(lo), ex_const(lo->typ, 0));
    set_flag(&irout, type, ZF, condZF);
    
    Temp *SF = mk_reg("SF", REG_1);
    Exp *condSF = _ex_l_cast(ex_shr( lo, &c_TYPE_SIZE_LESS_1), REG_1);
    set_flag(&irout, type, SF, condSF);

    Temp *OF = mk_reg("OF", REG_1);
    irout.push_back( new Move(OF, new Temp(*CF)) );

    // We construct EFLAGS in total only in case that
    // instructions directly need EFLAGS as a complete register such as pushf.
    //set_eflags_bits(&irout, CF, PF, AF, ZF, SF, OF);

    return irout;
}

int del_put_thunk(vector<Stmt*> *ir,
                  string op_string, int opi, int dep1, int dep2, int ndep, int mux0x)
{

  assert(ir);
  assert(opi >= 0 && dep1 >= 0 && dep2 >= 0 && ndep >= 0);
  
  
  vector<Stmt *> rv;
  
  // int end = -1;
  int len = 0;
  int j = 0;
  string op;
    
  // cout << "opi " << opi << " dep1 " << dep1 << " dep2 " << dep2 << " ndep " << ndep << " mux0x " << mux0x << endl;
  
  // assert(ndep >= dep2 && dep2 >= dep1 && dep1 >= opi && opi >= mux0x);
  
  // Delete statements assigning to flag thunk temps
  for (vector<Stmt*>::iterator
         i = ir->begin(); i != ir->end(); i++, j++)
  {
    Stmt *stmt = (*i);
    rv.push_back(stmt);
    
    len++;
    
    // cout << "debug " << j << " " << stmt->tostring() << endl;
    
    if (i386_op_is_very_broken(op_string)) {
      // cerr << "Warning: Broken op detected.  Not removing thunks.  Please fix this part of libasmir eventually." << endl;
    
    
    } else {

      // /* NEW deletion code */
      // if ((j >= mux0x) && (j <= ndep)) {
      //   cout << "here we are..." << j << stmt->tostring() << endl;
      //   Stmt::destroy(rv.back());
      //   rv.pop_back();
      //   len--;
      //   // end = len;
      // } 

      // /* NEW deletion code */
      // if (((j >= mux0x) && (j < (mux0x + MUX_LENGTH + MUX_OFFSET)))
      //     || j == opi
      //     || j == dep1
      //     || j == dep2
      //     || j == ndep) {
      //   cout << "here we are..." << j << stmt->tostring() << endl;
      //     Stmt::destroy(rv.back());
      //     rv.pop_back();
      //     len--;
      //     // end = len;
      // }
    
      /* OLD deletion code */
      if ( stmt->stmt_type == MOVE )
      {
        Move *move = (Move *)stmt;
        if ( move->lhs->exp_type == TEMP )
        {
          Temp *temp = (Temp *)(move->lhs);
          if (    temp->name.find("CC_OP") != string::npos 
                  || temp->name.find("CC_DEP1") != string::npos 
                  || temp->name.find("CC_DEP2") != string::npos 
                  || temp->name.find("CC_NDEP") != string::npos )
          {
            //// XXX: don't delete for now.
            //// remove and Free the Stmt
            // CC_OP, CC_DEP1, CC_DEP2, CC_NDEP are never set unless use_eflags_thunks is true.
            if(!use_eflags_thunks) {
              // cout << "old code would delete " << j << stmt->tostring() << endl;
              Stmt::destroy(rv.back());
              rv.pop_back();
              len--;
            }
            //end = len;
          }
        }
      }
    }        
  }
  
  assert(len >= 0);
  ir->clear();
  ir->insert(ir->begin(), rv.begin(), rv.end());
  return len;
}


//
// These typedefs are specifically for casting mod_eflags_func to the 
// function that accepts the right number of arguments
//
typedef vector<Stmt *> Mod_Func_0 (void);
typedef vector<Stmt *> Mod_Func_2 (reg_t, Exp *, Exp *);
typedef vector<Stmt *> Mod_Func_3 (reg_t, Exp *, Exp *, Exp *);

static void modify_eflags_helper( string op, reg_t type, vector<Stmt *> *ir, int argnum, Mod_Func_0 *mod_eflags_func )
{

    assert(ir);
    assert(argnum == 2 || argnum == 3);
    assert(mod_eflags_func);

    // Look for occurrence of CC_OP assignment
    // These will have the indices of the CC_OP stmts
    int opi, dep1, dep2, ndep, mux0x;
    opi = dep1 = dep2 = ndep = mux0x = -1;
    get_thunk_index(ir, &opi, &dep1, &dep2, &ndep, &mux0x);

    if ( opi >= 0 )
    {
        vector<Stmt *> mods;

        if ( argnum == 2 )
        {
            // Get the arguments we need from these Stmt's
            Exp *arg1 = ((Move *)(ir->at(dep1)))->rhs;
            Exp *arg2 = ((Move *)(ir->at(dep2)))->rhs;

            // Do the translation
            // To figure out the type, we assume the rhs of the 
            // assignment to CC_DEP is always either a Constant or a Temp
            // Otherwise, we have no way of figuring out the expression type
            Mod_Func_2 *mod_func = (Mod_Func_2 *)mod_eflags_func;
            mods = mod_func(type, arg1, arg2);
        }
        else // argnum == 3
        {
            Exp *arg1 = ((Move *)(ir->at(dep1)))->rhs;
            Exp *arg2 = ((Move *)(ir->at(dep2)))->rhs;
            Exp *arg3 = ((Move *)(ir->at(ndep)))->rhs;

            Mod_Func_3 *mod_func = (Mod_Func_3 *)mod_eflags_func;
            mods = mod_func(type, arg1, arg2, arg3);
        }

        // I don't think this is needed anymore --ed
	// if (mux0x != -1) {
	//   Exp *cond, *exp0, *expx, *res;
	//   match_mux0x(ir, mux0x, &cond, &exp0, &expx, &res);
	//   Label *mod = mk_label();
	//   Label *nomod = mk_label();
	//   mods.insert(mods.begin(), mod);
	//   mods.insert(mods.begin(),
	// 	      new CJmp(ecl(cond),
	// 		       new Name(mod->label), new Name(nomod->label)) );
	//   mods.push_back(nomod);
	// }

        // Delete the thunk
        int pos = del_put_thunk(ir, op, opi, dep1, dep2, ndep, mux0x);
        // Insert the eflags mods in this position
        ir->insert( ir->begin()+pos, mods.begin(), mods.end() );
        ir->insert( ir->begin()+pos, new Comment("eflags thunk: "+op));
    }
    else {
      cerr << "Warning! No EFLAGS thunk was found for \"" 
           << op << "\"!" << endl;
      for(vector<Stmt*>::iterator
            i=ir->begin(); i!=ir->end(); i++) {
        cerr << (*i)->tostring() << endl;
      }
      //      panic("No EFLAGS thunk was found for \"" + op + "\"!");
    }
}

/* List of operations we should not delete thunks for.
 *
 * Or, put another way, a list of operations for which the eflags code
 * is COMPLETELY BROKEN.
 */
bool i386_op_is_very_broken(string op_string) {
  if (op_string.find("shr",0) == 0
      || op_string.find("sar",0) == 0) {
    return true;
  }
  else {
    return false;
  }
}

void i386_modify_flags( asm_program_t *prog, bap_block_t *block )
{
    assert(block);

    vector<Stmt *> *ir = block->bap_ir;    

    // Look for occurrence of CC_OP assignment
    // These will have the indices of the CC_OP stmts
    // FIXME: cut-and-paste from modify_eflags_helper, which 
    //        will do this again
    int opi, dep1, dep2, ndep, mux0x;
    opi = dep1 = dep2 = ndep = mux0x = -1;
    get_thunk_index(ir, &opi, &dep1, &dep2, &ndep, &mux0x);

    if (opi == -1)
      // doesn't set flags
      return;

    Stmt *op_stmt = ir->at(opi);

    bool got_op;
    int op;
    if(!(op_stmt->stmt_type == MOVE)) {
      got_op = false;
    } else {
      Move *op_mov = (Move*)op_stmt;
      if(!(op_mov->rhs->exp_type == CONSTANT)) {
        got_op = false;
      } else {
        Constant *op_const = (Constant*)op_mov->rhs;
        op = op_const->val;
        got_op = true;
      }
    }

    if (got_op) {
      reg_t type;
      switch(op) {
      case X86G_CC_OP_ADDB:
      case X86G_CC_OP_ADCB:
      case X86G_CC_OP_SUBB:
      case X86G_CC_OP_SBBB:
      case X86G_CC_OP_LOGICB:
      case X86G_CC_OP_INCB:   
      case X86G_CC_OP_DECB:   
      case X86G_CC_OP_SHLB:   
      case X86G_CC_OP_SHRB:   
      case X86G_CC_OP_ROLB:   
      case X86G_CC_OP_RORB:   
      case X86G_CC_OP_UMULB:  
      case X86G_CC_OP_SMULB:
        type = REG_8;
        break;

      case X86G_CC_OP_ADDW:
      case X86G_CC_OP_ADCW:
      case X86G_CC_OP_SUBW:
      case X86G_CC_OP_SBBW:
      case X86G_CC_OP_LOGICW:
      case X86G_CC_OP_INCW:   
      case X86G_CC_OP_DECW:   
      case X86G_CC_OP_SHLW:   
      case X86G_CC_OP_SHRW:   
      case X86G_CC_OP_ROLW:   
      case X86G_CC_OP_RORW:   
      case X86G_CC_OP_UMULW:  
      case X86G_CC_OP_SMULW:
        type = REG_16;
        break;

      case X86G_CC_OP_ADDL:
      case X86G_CC_OP_ADCL:
      case X86G_CC_OP_SUBL:
      case X86G_CC_OP_SBBL:
      case X86G_CC_OP_LOGICL:
      case X86G_CC_OP_INCL:   
      case X86G_CC_OP_DECL:   
      case X86G_CC_OP_SHLL:   
      case X86G_CC_OP_SHRL:   
      case X86G_CC_OP_ROLL:   
      case X86G_CC_OP_RORL:   
      case X86G_CC_OP_UMULL:  
      case X86G_CC_OP_SMULL:
        type = REG_32;
        break;

      case X86G_CC_OP_COPY:
        type = REG_32;
        break;

      default:
        assert(0);
      }

      string op_s;
      Mod_Func_0 *cb;
      int num_params;
      
      switch (op) {
      case X86G_CC_OP_COPY:
        op_s = "copy";
        num_params = 2;
        cb = (Mod_Func_0*) mod_eflags_copy;
        break;

      case X86G_CC_OP_ADDB:
      case X86G_CC_OP_ADDW:
      case X86G_CC_OP_ADDL:
        op_s = "add";
        num_params = 2;
        cb = (Mod_Func_0*) mod_eflags_add;
        break;

      case X86G_CC_OP_ADCB:
      case X86G_CC_OP_ADCW:
      case X86G_CC_OP_ADCL:
        op_s = "adc";
        num_params = 3;
        cb = (Mod_Func_0*) mod_eflags_adc;
        break;

      case X86G_CC_OP_SUBB:
      case X86G_CC_OP_SUBW:
      case X86G_CC_OP_SUBL:
        op_s = "sub";
        num_params = 2;
        cb = (Mod_Func_0*) mod_eflags_sub;
        break;

      case X86G_CC_OP_SBBB:
      case X86G_CC_OP_SBBW:
      case X86G_CC_OP_SBBL:
        op_s = "sbb";
        num_params = 3;
        cb = (Mod_Func_0*) mod_eflags_sbb;
        break;

      case X86G_CC_OP_LOGICB:
      case X86G_CC_OP_LOGICW: 
      case X86G_CC_OP_LOGICL: 
        op_s = "logic";
        num_params = 2;
        cb = (Mod_Func_0 *)mod_eflags_logic;
        break;

      case X86G_CC_OP_INCB:   
      case X86G_CC_OP_INCW:   
      case X86G_CC_OP_INCL:   
        op_s = "inc";
        num_params = 3;
        cb = (Mod_Func_0*) mod_eflags_inc;
        break;

      case X86G_CC_OP_DECB:   
      case X86G_CC_OP_DECW:   
      case X86G_CC_OP_DECL:
        op_s = "dec";
        num_params = 3;
        cb = (Mod_Func_0 *)mod_eflags_dec;
        break;

      case X86G_CC_OP_SHLB:   
      case X86G_CC_OP_SHLW:   
      case X86G_CC_OP_SHLL:   
        op_s = "shl";
        num_params = 2;
        cb = (Mod_Func_0 *)mod_eflags_shl;
        break;

      case X86G_CC_OP_SHRB:   
      case X86G_CC_OP_SHRW:   
      case X86G_CC_OP_SHRL:   
        op_s = "shr";
        num_params = 2;
        cb = (Mod_Func_0 *)mod_eflags_shr;
        break;

      case X86G_CC_OP_ROLB:   
      case X86G_CC_OP_ROLW:   
      case X86G_CC_OP_ROLL:   
        op_s = "rol";
        num_params = 3;
        cb = (Mod_Func_0 *)mod_eflags_rol;
        break;

      case X86G_CC_OP_RORB:   
      case X86G_CC_OP_RORW:   
      case X86G_CC_OP_RORL:   
        op_s = "ror";
        num_params = 3;
        cb = (Mod_Func_0 *)mod_eflags_ror;
        break;

      case X86G_CC_OP_UMULB:  
      case X86G_CC_OP_UMULW:  
      case X86G_CC_OP_UMULL:  
        op_s = "umul";
        num_params = 2;
        cb = (Mod_Func_0 *)mod_eflags_umul;
        break;

      case X86G_CC_OP_SMULB:
      case X86G_CC_OP_SMULW:
      case X86G_CC_OP_SMULL:
        op_s = "smul";
        num_params = 2;
        cb = (Mod_Func_0 *)mod_eflags_smul;
        break;

      default:
        panic("unhandled cc_op!");
      }
      modify_eflags_helper(op_s, type, ir, num_params, cb);
      return;

    } else {
      string op = get_op_str(prog, block->inst);
      //      cerr << "Warning: non-constant cc_op for " << op 
      //           << ". falling back on old eflag code." << endl;

      // DEBUG
      //ostream_i386_insn(block->inst, cout);


      // FIXME: how to figure out types?
      if ( op.find("rol",0) == 0)
        modify_eflags_helper(op, REG_32, ir, 3, (Mod_Func_0 *)mod_eflags_rol);
      else if ( op.find("ror",0) == 0)
        modify_eflags_helper(op, REG_32, ir, 3, (Mod_Func_0 *)mod_eflags_ror);
      else if ( op.find("shr",0) == 0
                || op.find("sar",0) == 0)
        modify_eflags_helper(op, REG_32, ir, 2, (Mod_Func_0 *)mod_eflags_shr);
      else if ( op.find("shl",0) == 0 && op.find("shld") == string::npos )
        modify_eflags_helper(op, REG_32, ir, 2, (Mod_Func_0 *)mod_eflags_shl);
      else {
        cerr << "Warning! Flags not handled for " << op 
             << hex 
             << " at " << block->inst
             << endl;
      }


//       if ( op == "sahf" || op == "bsr" || op == "bsf" || op == "popf" ||
//            op == "popfl" || 
//            inst->opcode[0] == 0xc7 && inst->opcode[1] == 0x0f || // cmpxchg8b TMP
//            op == "cmpxchg8b") // VEX calcs flags inline and copies
//         {
//           modify_eflags_helper(op, ir, 2, (Mod_Func_0 *)mod_eflags_copy);
//         }
//       else if ( op == "add" || op == "addb" || op == "addw" || op == "addl" ||
//                 op == "xadd")
//         {
//           modify_eflags_helper(op, ir, 2, (Mod_Func_0 *)mod_eflags_add);
//         }
//       else if ( op == "sub" || op == "subb" || op == "subw" || op == "subl" || 
//                 op == "cmp" || op == "cmpb" || op == "cmpw" || op == "cmpl" ||
//                 op == "cmpxchg" ||
//                 op == "neg" || op == "negl" || 
//                 op.find("scas",0) != string::npos || // scas and variants
//                 op.find("cmps",0) != string::npos  // cmps and variants
//                 )
//         {
//           modify_eflags_helper(op, ir, 2, (Mod_Func_0 *)mod_eflags_sub);
//         }
//       else if ( op == "adc" || op == "adcl" )
//         {
//           modify_eflags_helper(op, ir, 3, (Mod_Func_0 *)mod_eflags_adc);
//         }
//       else if ( op == "sbb" || op == "sbbl" )
//         {
//           modify_eflags_helper(op, ir, 3, (Mod_Func_0 *)mod_eflags_sbb);
//         }   
//       else if ( op == "and"  || op == "or"  || op == "xor"  || op == "test"  ||
//                 op == "andl" || op == "orl" || op == "xorl" || op == "testl" ||
//                 op == "andw" || op == "orw" || op == "xorw" || op == "testw" ||
//                 op == "andb" || op == "orb" || op == "xorb" || op == "testb" )
//         {
//           modify_eflags_helper(op, ir, 2, (Mod_Func_0 *)mod_eflags_logic);
//         }   
//       else if ( op == "inc" || op == "incl" || op == "incb" || op == "incw" )
//         {
//           modify_eflags_helper(op, ir, 3, (Mod_Func_0 *)mod_eflags_inc);
//         }   
//       else if ( op == "dec" || op == "decl" || op == "decb" || op == "decw" )
//         {
//           modify_eflags_helper(op, ir, 3, (Mod_Func_0 *)mod_eflags_dec);
//         }   
//       else if ( op == "shl" || op == "shll" || op == "shlb")
//         {
//           modify_eflags_helper(op, ir, 2, (Mod_Func_0 *)mod_eflags_shl);
//         }   
//       else if ( op == "shr" || op == "shrl" || op == "sar" || op == "sarl" ||
//                 op == "shrd" || op == "sarw")
//         {
//           modify_eflags_helper(op, ir, 2, (Mod_Func_0 *)mod_eflags_shr);
//         }   
//       else if ( op == "rol" )
//         {
//           modify_eflags_helper(op, ir, 3, (Mod_Func_0 *)mod_eflags_rol);
//         }   
//       else if ( op == "ror" )
//         {
//           modify_eflags_helper(op, ir, 3, (Mod_Func_0 *)mod_eflags_ror);
//         }   
//       else if ( op == "mul" || op == "mull" )
//         {
//           modify_eflags_helper(op, ir, 2, (Mod_Func_0 *)mod_eflags_umul);
//         }
//       else if ( op == "imul" || op == "imull" )
//         {
//           modify_eflags_helper(op, ir, 2, (Mod_Func_0 *)mod_eflags_smul);
//         }
//       else if ( op == "call" ||
//                 op.find("div",0) == 0 || // div + variants
//                 op == "int" ||
//                 op[0] == 'j' || // jmp and all conditional jumps)
//                 op.find(" j",0) != string::npos || // prefixed jmps
//                 op == "lea" ||
//                 op == "leave" ||
//                 op.find("mov") != string::npos || // all movs
//                 op == "nop" ||
//                 op == "not" ||
//                 op == "pop" || op == "popa" || op == "popad" ||
//                 op == "push" || op == "pusha" || op == "pushad" ||
//                 op == "pushl" ||
//                 op == "pushf" || op == "pushfl" ||
//                 op == "rdtsc" || 
//                 op == "ret" ||
//                 op.find("set",0) == 0 || // setCC
//                 op == "std" || // dflag is assigned explicitly in trans.
//                 op.find("stos",0) != string::npos || // stos variants & prefixes
//                 op == "xchg" )
//         {
//           // flags unaffected
//         }
//       else
//         {
//           //panic("No flag handler for \"" + op + "\"!");
//           cerr << "Warning! Flags not handled for " << op 
//                << hex 
//                << " " << (int)inst->opcode[0]
//                << " " << (int)inst->opcode[1]
//                << " " << (int)inst->opcode[2]
//                << endl;
//         }
    }
}



