/*
 Owned and copyright BitBlaze, 2007. All rights reserved.
 Do not copy, disclose, or distribute without explicit written
 permission.
*/
//======================================================================
//
// This file contains a test of the VEX IR translation interface.
//
//======================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <set>
#include <iostream>

#include "asm_program.h"
#include "ir_printer.h"

extern "C" 
{
#include "libvex.h"
}

#include "irtoir.h"

using namespace std;

#define MAX_INST_LEN 30

//======================================================================
//
// Main
//
//======================================================================

int main(int argc, char *argv[])
{
    unsigned char inst[MAX_INST_LEN];
    memset(inst, 0, sizeof(inst));
    memcpy(inst, "\x01\xC8", 2);

    translate_init();

    // allocate a fake bfd instance
    asm_program_t *prog = asmir_new_asmp_for_arch(bfd_arch_i386);
    if (prog)
    {        
        if (prog->segs = (section_t *)bfd_alloc(prog->abfd, sizeof(section_t)))
        {
            // create segment for our instruction buffer
            prog->segs->data = inst;
            prog->segs->datasize = MAX_INST_LEN;            
            prog->segs->start_addr = 0;
            prog->segs->end_addr = MAX_INST_LEN;
            prog->segs->section = NULL;
            prog->segs->is_code = true;        
            
            // translate to VEX
            bap_block_t *block = generate_vex_ir(prog, 0);
            if (block)
            {
                // tarnslate to BAP
                generate_bap_ir_block(prog, block);

                for (int i = 0; i < block->bap_ir->size(); i++)
                {
                    Stmt *s = block->bap_ir->at(i);
                    printf("%s\n", s->tostring().c_str());
                }
            }
        }        
    }

    vx_FreeAll();

    return 0;
}

