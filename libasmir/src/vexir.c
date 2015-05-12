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
#include "jumpbuf.h"
#include "common.h"
#include "disasm.h"

//======================================================================
//
// Globals
//
//======================================================================

// Some info required for translation
static VexArchInfo vai;
static VexGuestExtents vge;
static VexTranslateArgs vta;
static VexTranslateResult vtr;

// Define a temp buffer to hold the translated bytes
// Not needed with patched VEX
#ifndef AMD64

#define TMPBUF_SIZE 2000

static UChar tmpbuf[TMPBUF_SIZE];
static Int tmpbuf_used;

#endif

#define VEX_TRACE_INST (1 << 5)

// Global for saving the intermediate results of translation from
// within the callback (instrument1)
static IRSB *irbb_current = NULL;
static int size_current = 0;

//======================================================================
//
// Functions needed for the VEX translation
//
//======================================================================

__attribute__((noreturn))
static void failure_exit(void)
{
    abort();
}

static void log_bytes(const HChar *bytes, SizeT nbytes)
{
    log_write_bytes(LOG_VEX, bytes, nbytes);
}

static Bool chase_into_ok(void *closureV, Addr addr)
{
    return False;
}

static void *dispatch(void)
{
    return NULL;
}

static UInt needs_self_check(
    void *opaque, 
    VexRegisterUpdates* pxControl, 
    const VexGuestExtents *vge)
{
    return 0;
}

//----------------------------------------------------------------------
// This is where we copy out the IRSB
//----------------------------------------------------------------------
static IRSB *instrument1(void *callback_opaque, 
                         IRSB *irbb,
                         const VexGuestLayout *vgl, 
                         const VexGuestExtents *vge,
                         const VexArchInfo *ainfo,
                         IRType gWordTy, 
                         IRType hWordTy)
{
    assert(irbb);

    irbb_current = vx_dopyIRSB(irbb);
    size_current = vge->len[0];

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
    {
        return;
    }

    initialized = 1;

    // Initialize VEX
    VexControl vc;
    vc.iropt_verbosity                  = 0;
    vc.iropt_level                      = 2;
    vc.iropt_register_updates_default   = VexRegUpdSpAtMemAccess;
    vc.iropt_unroll_thresh              = 0;
    vc.guest_max_insns                  = 1; // By default, we translate 1 instruction at a time
    vc.guest_chase_thresh               = 0;
    vc.guest_chase_cond                 = 0;

    LibVEX_Init(&failure_exit,
                &log_bytes,
                0, // Debug level
                &vc);

    LibVEX_default_VexArchInfo(&vai);

    // FIXME: determinate endianess by specified guest arch
    vai.endness = VexEndnessLE;

    // Setup the translation args
    vta.arch_guest                  = VexArch_INVALID;  // to be assigned later
    vta.archinfo_guest              = vai;

    //
    // FIXME: detect this one automatically
    //
#ifdef AMD64
    
    vta.arch_host                   = VexArchAMD64;

#else
    
    vta.arch_host                   = VexArchX86;       // Target arch

#endif
    
    vta.archinfo_host               = vai;
    vta.guest_bytes                 = NULL;             // Set in translate_insns
    vta.guest_bytes_addr            = 0;                // Set in translate_insns
    vta.callback_opaque             = NULL;             // Used by chase_into_ok, but never actually called
    vta.chase_into_ok               = chase_into_ok;    // Always returns false
    vta.preamble_function           = NULL;
    vta.guest_extents               = &vge;

#ifdef AMD64
    
    vta.host_bytes                  = NULL;             // Buffer for storing the output binary
    vta.host_bytes_size             = 0;
    vta.host_bytes_used             = NULL;

#else
    
    vta.host_bytes                  = tmpbuf;           // Buffer for storing the output binary
    vta.host_bytes_size             = TMPBUF_SIZE;
    vta.host_bytes_used             = &tmpbuf_used;

#endif

    vta.instrument1                 = instrument1;      // Callback we defined to help us save the IR
    vta.instrument2                 = NULL;
    vta.traceflags                  = VEX_TRACE_INST;   // Debug verbosity
    
    vta.disp_cp_chain_me_to_slowEP  = dispatch;         // Not used
    vta.disp_cp_chain_me_to_fastEP  = dispatch;         // Not used
    vta.disp_cp_xindir              = dispatch;         // Not used
    vta.disp_cp_xassisted           = dispatch;         // Not used

    vta.needs_self_check            = needs_self_check; // Not used
}

//----------------------------------------------------------------------
// Translate 1 instruction to VEX IR.
//----------------------------------------------------------------------
IRSB *translate_insn(VexArch guest,
                     unsigned char *insn_start,
                     unsigned int insn_addr,
                     int *insn_size)
{
    // init global variables
    vta.arch_guest = guest;
    vta.archinfo_guest.hwcaps = 0;

    if (guest == VexArchARM)
    {
        // We must set the ARM version of VEX aborts
        vta.archinfo_guest.hwcaps |= 7 /* ARMv7 */;

        if (IS_ARM_THUMB(insn_addr))
        {
            // in thumb mode we also need to increment buffer pointer as VEX wants
            insn_start += 1;
        }
    }
    else if (guest == VexArchX86)
    {

#ifdef USE_SSE

        // Enable SSE
        vta.archinfo_guest.hwcaps |= VEX_HWCAPS_X86_SSE1;
        vta.archinfo_guest.hwcaps |= VEX_HWCAPS_X86_SSE2;
        vta.archinfo_guest.hwcaps |= VEX_HWCAPS_X86_SSE3;
        vta.archinfo_guest.hwcaps |= VEX_HWCAPS_X86_LZCNT;

#endif // USE_SSE

    }

    vta.guest_bytes = insn_start; // Ptr to actual bytes of start of instruction
    vta.guest_bytes_addr = (Addr64)insn_addr;

    irbb_current = NULL;
    size_current = 0;

    if (!setjmp(vex_error)) 
    {
        jmp_buf_set = 1;

        // FIXME: check the result
        // Do the actual translation
        vtr = LibVEX_Translate(&vta);

        assert(irbb_current);

        if (insn_size)
        {
            *insn_size = size_current;
        }
    }
    else
    {
        log_write(LOG_ERR, "Critical VEX error, instruction was not translated");
        
        irbb_current = NULL;
    }

    return irbb_current;
}
