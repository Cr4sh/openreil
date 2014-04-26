/*
 Owned and copyright BitBlaze, 2007. All rights reserved.
 Do not copy, disclose, or distribute without explicit written
 permission.
*/
#include "ir_printer.h"

#include <iostream>

/*
void print_globals(){

  vector<VarDecl *> globals = get_reg_decls();
  for(vector<VarDecl *>::const_iterator it = globals.begin();
	it != globals.end(); it++){
	VarDecl *s = *it;
	cout << s->tostring() << endl;
  }

  cout << "var mem:reg8_t[reg32_t];" << endl;

  vector<Stmt *> helpers = gen_eflags_helpers();
  for(vector<Stmt *>::const_iterator it = helpers.begin();
      it != helpers.end(); it++){
    Stmt *s = *it;
    cout << s->tostring() << endl;
  }
}
*/

void print_bap_ir(asm_program_t *prog, vector<bap_block_t *> vblocks )
{
    unsigned int i, j;
    
    for ( i = 0; i < vblocks.size(); i++ )
    {
        bap_block_t *block = vblocks.at(i);
        assert(block);
        
        vector<Stmt *> *inner = block->bap_ir;

	//        cout << "Vine Block " << i << endl;
        cout << "  {" << endl;
// 	declvis vis;
// 	vis.compute(inner);
// 	print_decls(vis.decls);
	//        cout << "    ";
	cout << "   // " << asmir_string_of_insn(prog, block->inst) << endl;


        for ( j = 0; j < inner->size(); j++ )
        {
	  Stmt *s = inner->at(j);
	  cout << "     " << s->tostring();
// 	  if(s->stmt_type == LABEL)
// 	    cout << endl;
// 	  else
// 	    cout << ";" << endl;
	  cout << endl;

        }
        cout << "  }" << endl;
    }
    
}
