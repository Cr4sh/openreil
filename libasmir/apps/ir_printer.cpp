/*
 Owned and copyright BitBlaze, 2007. All rights reserved.
 Do not copy, disclose, or distribute without explicit written
 permission.
*/
#include "ir_printer.h"

#include <iostream>

void print_bap_ir(asm_program_t *prog, vector<bap_block_t *> vblocks)
{
    unsigned int i, j;
    
    for (i = 0; i < vblocks.size(); i++)
    {
        bap_block_t *block = vblocks.at(i);
        vector<Stmt *> *inner = block->bap_ir;

        assert(block);        

        cout << "  {" << endl;
	    cout << "   // " << asmir_string_of_insn(prog, block->inst) << endl;

        for (j = 0; j < inner->size(); j++)
        {
            Stmt *s = inner->at(j);
            cout << "     " << s->tostring();
            cout << endl;
        }

        cout << "  }" << endl;
    }
}
