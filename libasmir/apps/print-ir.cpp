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
//#include "disasm-pp.h"

#include "ir_printer.h"

extern "C" 
{
#include "libvex.h"
}

#include "irtoir.h"

using namespace std;

//======================================================================
//
// Miscellaneous functions
//
//======================================================================

void usage( char *progname )
{
    printf("Usage: %s filename [option]\n", progname);
    printf("Options:\n");
    printf("\t0 - Print only assembly\n");
    printf("\t1 - Print only VEX IR\n");
    printf("\t2 - Print only Vine IR [Default]\n");
}



//----------------------------------------------------------------------
// Print all the instructions of this program, separating them by
// function and BB
//----------------------------------------------------------------------
void print_prog_insns( asm_program_t *prog )
{
  for (section_t *sec = prog->segs; sec; sec = sec->next) {
    if (!sec->is_code)
      continue;

    disassembler_ftype disas = disassembler(prog->abfd);
    prog->disasm_info.fprintf_func = (fprintf_ftype)fprintf;
    prog->disasm_info.stream = stdout;

    int len;
    for (address_t addr = sec->start_addr; addr < sec->end_addr; addr += len) {
      len = disas(addr, &prog->disasm_info);
    }
  }
}


void print_vex_ir(asm_program_t *prog, vector<bap_block_t *> vblocks )
{

    unsigned int i;

    for ( i = 0; i < vblocks.size(); i++ )
    {
        bap_block_t *block = vblocks.at(i);
        assert(block);

        cout << endl << "VEX Block " << i << endl;

        cout << "   ";
        cout << asmir_string_of_insn(prog, block->inst);
        cout << endl;

        if ( block->vex_ir != NULL )
            ppIRSB(block->vex_ir);
        else
            cout << "   Unhandled by VEX" << endl;

    }
}



void print_prog_ir(asm_program_t *prog)
{
  vector<bap_block_t *> bap_blocks = generate_vex_ir(prog);    
  bap_blocks = generate_bap_ir(prog, bap_blocks);

  //print_globals();
  print_bap_ir(prog, bap_blocks);
}

//======================================================================
//
// Main
//
//======================================================================

int main(int argc, char *argv[])
{
    //
    // Check args
    //
    if ( argc < 2 )
    {
        usage(argv[0]);
        return -1;
    }

    char print_option = '2';
    if ( argc > 2 )
    {
        print_option = *(argv[2]);
    }

    vector<bap_block_t *> bap_blocks;

    //
    // Disassemble the program 
    //
    cerr << "Disassembling binary." << endl;
    asm_program_t *prog = asmir_open_file(argv[1], -1);
    assert(prog);


    if(print_option == '2'){
      print_prog_ir(prog);
      return 0;
    }

    //
    // Translate asm to VEX IR
    //
    cerr << "Translating asm to VEX IR." << endl;
    if ( print_option > '0' )
    {
        bap_blocks = generate_vex_ir(prog);
    }

    //
    // Translate VEX IR to Vine IR
    //
    cerr << "Translating VEX IR to Vine IR." << endl;
    if ( print_option > '1' )
    {
      bap_blocks = generate_bap_ir(prog, bap_blocks);
    }

    cerr << "Printing output:" << endl;
    // 
    // Print all the instructions in the disassembled program
    //
    if ( print_option == '0' )
        print_prog_insns(prog);

    //
    // Print the VEX IR output
    //
    else if ( print_option == '1' )
      print_vex_ir(prog, bap_blocks);

    //
    // Print the Vine IR output
    //
    else if ( print_option == '2' )
      print_bap_ir(prog, bap_blocks);


    //
    // Free the translated block after we're done using it, otherwise,
    // we run the risk of running out of memory for subsequent blocks
    // since memory for translations is static and have a fixed limit.
    //
//    vx_FreeAll();   // Frees mem allocated inside generate_vex_ir()


    return 0;
}


