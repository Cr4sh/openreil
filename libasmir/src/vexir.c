//======================================================================
//
// This file provides the interface to VEX that allows block by block
// translation from binary to VEX IR. 
//
//======================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "libvex.h"

#include "vexmem.h"


//======================================================================
//
// Globals
//
//======================================================================

// Some info required for translation
static VexArchInfo         vai;
static VexGuestExtents     vge;
static VexTranslateArgs    vta;
static VexTranslateResult  vtr;

// Define a temp buffer to hold the translated bytes
// Not needed with patched VEX
#ifndef AMD64
#define TMPBUF_SIZE 2000
static UChar tmpbuf[TMPBUF_SIZE];
static Int tmpbuf_used;
#endif

// Global for saving the intermediate results of translation from
// within the callback (instrument1)
static IRSB *irbb_current = NULL;

//======================================================================
//
// Functions needed for the VEX translation
//
//======================================================================

__attribute__ ((noreturn))
static void failure_exit( void )
{
  abort();
}

static void log_bytes( HChar* bytes, Int nbytes )
{
    Int i;
    for (i = 0; i < nbytes-3; i += 4)
        printf("%c%c%c%c", bytes[i], bytes[i+1], bytes[i+2], bytes[i+3]);
    for (; i < nbytes; i++)
        printf("%c", bytes[i]);
}

static Bool chase_into_ok( void *closureV, Addr64 addr64 )
{
    return False;
}

static void *dispatch( void )
{
    return NULL;
}

static UInt needs_self_check ( void* opaque, VexGuestExtents* vge ) {
   return 0;
}
//----------------------------------------------------------------------
// This is where we copy out the IRSB
//----------------------------------------------------------------------
static
IRSB *instrument1(  void *callback_opaque, 
                    IRSB *irbb,
                    VexGuestLayout *vgl,
                    VexGuestExtents *vge,
                    IRType gWordTy,
                    IRType hWordTy )
{

    assert(irbb);
    
    irbb_current = vx_dopyIRSB(irbb);

    return irbb;
}



//----------------------------------------------------------------------
// Initializes VEX 
// It must be called before using VEX for translation to Valgrind IR
//----------------------------------------------------------------------
void translate_init()
{
  static int initialized = 0;
  if (initialized)
    return;
  initialized = 1;

    //
    // Initialize VEX
    //
    VexControl vc;
    vc.iropt_verbosity              = 0;
    //vc.iropt_level                  = 0;    // No optimization by default
    vc.iropt_level                  = 2;
    vc.iropt_precise_memory_exns    = False;
    vc.iropt_unroll_thresh          = 0;
    vc.guest_max_insns              = 1;    // By default, we translate 1 instruction at a time
    vc.guest_chase_thresh           = 0;    
    vc.guest_chase_cond             = 0;
    
    LibVEX_Init(&failure_exit, 
                &log_bytes, 
                0,              // Debug level
                False,          // Valgrind support
                &vc );

    LibVEX_default_VexArchInfo(&vai);

    // Enable SSE
    /* vai.hwcaps |= VEX_HWCAPS_X86_SSE1; */
    /* vai.hwcaps |= VEX_HWCAPS_X86_SSE2; */
    /* vai.hwcaps |= VEX_HWCAPS_X86_SSE3; */
    /* vai.hwcaps |= VEX_HWCAPS_X86_LZCNT; */

    // Setup the translation args
    vta.arch_guest          = VexArch_INVALID; // to be assigned later
    //vta.arch_guest          = VexArchARM;
    //vta.arch_guest          = VexArchX86;               // Source arch 
    vta.archinfo_guest      = vai;
    // FIXME: detect this one automatically
#ifdef AMD64
    vta.arch_host           = VexArchAMD64;
#else    
    vta.arch_host           = VexArchX86;               // Target arch
#endif
    vta.archinfo_host       = vai;
    vta.guest_bytes         = NULL;             // Set in translate_insns
    vta.guest_bytes_addr    = 0;                // Set in translate_insns
    vta.callback_opaque     = NULL;             // Used by chase_into_ok, but never actually called
    vta.chase_into_ok       = chase_into_ok;    // Always returns false
    vta.preamble_function   = NULL;
    vta.guest_extents       = &vge;
#ifdef AMD64
    vta.host_bytes          = NULL;           // Buffer for storing the output binary
    vta.host_bytes_size     = 0;
    vta.host_bytes_used     = NULL;
#else
    vta.host_bytes          = tmpbuf;           // Buffer for storing the output binary
    vta.host_bytes_size     = TMPBUF_SIZE;
    vta.host_bytes_used     = &tmpbuf_used;
#endif
    vta.instrument1         = instrument1;      // Callback we defined to help us save the IR
    vta.instrument2         = NULL;
    //    vta.do_self_check       = False;
    vta.traceflags          = 0;                // Debug verbosity
    vta.dispatch_unassisted = dispatch; // Not used
    vta.dispatch_assisted   = dispatch;  // Not used
    vta.needs_self_check    = needs_self_check;  // Not used
}

//----------------------------------------------------------------------
// Translate 1 instruction to VEX IR.
//----------------------------------------------------------------------
IRSB *translate_insn( VexArch guest,
		      unsigned char *insn_start,
		      unsigned int insn_addr )
{
    vta.arch_guest = guest;

    if (guest == VexArchARM) {
      /* We must set the ARM version of VEX aborts */
      vta.archinfo_guest.hwcaps |= 5 /* ARMv5 */;
    }

    vta.guest_bytes         = (UChar *)(insn_start);  // Ptr to actual bytes of start of instruction
    vta.guest_bytes_addr    = (Addr64)(insn_addr);

    irbb_current = NULL;

    // FIXME: check the result
    // Do the actual translation
    vtr = LibVEX_Translate( &vta );

    assert(irbb_current);

    return irbb_current;
}

