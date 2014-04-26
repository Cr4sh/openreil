/*
 Owned and copyright David Brumley, 2007. All rights reserved.
 Do not copy, disclose, or distribute without explicit written
 permission.
*/
#ifndef _IR_PRINTER_H
#define _IR_PRINTER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <set>

#include "asm_program.h"
//#include "disasm-pp.h"

#include "ir_printer.h"

extern "C" 
{
#include "libvex.h"
}

#include "irtoir.h"
//#include "typecheck_ir.h"
//#include "irdeendianizer.h"
//#include "ir2xml.h"

using namespace std;

// Uncomment to enable typechecking.
//#define NOTYPECHECKING
void print_globals();
void print_bap_ir(asm_program_t *prog, vector<bap_block_t *> vblocks );
#endif
