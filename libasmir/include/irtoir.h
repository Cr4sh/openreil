/*
 Owned and copyright BitBlaze, 2007. All rights reserved.
 Do not copy, disclose, or distribute without explicit written
 permission.
*/

#ifndef __IRTOIR_H
#define __IRTOIR_H

typedef struct bap_block_s bap_block_t;


//
// VEX headers (inside Valgrind/VEX/pub)
//

#include "pin_frame.h"
#include "asm_program.h"
#include "vexmem.h"

#ifdef __cplusplus
extern "C"
{
#endif


#include "libvex.h"

#ifdef __cplusplus
}

#include <vector>

#include "stmt.h"

//
// A struct that encapsulates the stages of translation of 1 asm instruction.
// inst is the original instruction.
// vex_ir is inst translated into a block of VEX IR.
// bap_ir is inst translated into a block of Vine IR from its VEX IR translation.
//
struct bap_block_s
{
  bfd_vma         inst;
  IRSB            *vex_ir;
  vector<Stmt *>  *bap_ir;
};




//======================================================================
// 
// Translation functions. These should not be used directly unless you
// understand exactly how they work and have a specific need for them.
// If you just wanna translate a program, see generate_vex_ir and 
// generate_bap_ir at the bottom. They wrap these functions and
// provide an easier interface for translation. 
//
//======================================================================

extern "C" {
//
// Initializes VEX. This function must be called before translate_insn
// can be used. 
// vexir.c
void translate_init();

//
// Translates 1 asm instruction (in byte form) into a block of VEX IR
//
// \param insn_start Pointer to bytes of instruction
// \param insn_addr Address of the instruction in its own address space
// \return An IRSB containing the VEX IR translation of the given instruction
// vexir.c
IRSB *translate_insn( VexArch guest, unsigned char *insn_start, unsigned int insn_addr );
}


//======================================================================
// 
// Utility functions that wrap the raw translation functions.
// These are what should be used to do the actual translation.
// See print-ir.cpp in ../../apps for examples of how to use these.
//
//======================================================================


// Take an instrs and translate it into a VEX IR block
// and store it in a bap block
//bap_block_t* generate_vex_ir(VexArch guest, Instruction *inst);
bap_block_t* generate_vex_ir(asm_program_t *prog, address_t inst);

//
// Take a vector of instrs and translate it into VEX IR blocks
// and store them in a vector of bap blocks
//
// \param instrs the list of instructions to translate
// \return A vector of bap blocks. Only the inst and vex_ir fields in
//         the bap block are valid at this point.
//

//vector<bap_block_t *> generate_vex_ir(asm_program_t *prog, const vector<Instruction *> &instrs);

//
// Take a disassembled function and translate it into VEX IR blocks
// and store them in a vector of bap blocks
//
// \param func A disassembled function
// \return A vector of bap blocks. Only the inst and vex_ir fields in
//         the bap block are valid at this point.
//
//vector<bap_block_t *> generate_vex_ir(asm_program_t *prog, asm_function_t *func);

//
// Take a disassembled program and translate it into VEX IR blocks
// and store them in a vector of bap blocks
//
// \param prog A disassembled program
// \return A vector of bap blocks. Only the inst and vex_ir fields in
//         the bap block are valid at this point.
//
vector<bap_block_t *> generate_vex_ir( asm_program_t *prog );

// Same as generate_vex_ir, but only for an address range
vector<bap_block_t *> generate_vex_ir(asm_program_t *prog,
					address_t start,
					address_t end);

// Take a bap block that has gone through VEX translation and translate it
// to Vine IR.
void generate_bap_ir_block( asm_program_t *prog, bap_block_t *block );

//
// Take a vector of bap blocks that have gone through VEX translation
// and translate them to Vine IR
//
// \param vblocks Vector of bap blocks with valid VEX IR translations
// \return Vector of bap blocks with the bap_ir field filled in
//
vector<bap_block_t *> generate_bap_ir( asm_program_t *prog, vector<bap_block_t *> vblocks );


VexArch vexarch_of_bfdarch(bfd_architecture arch);

// Get global variables declaractions for this arch
//vector<VarDecl *> get_reg_decls(VexArch arch);


extern "C" {
  typedef struct vector<bap_block_t *> bap_blocks_t;

#else
  struct bap_block_s
  {
    bfd_vma         inst;
    IRSB            *vex_ir;
    struct vector_Stmt  *bap_ir;
  };


  typedef struct vector_bap_block_t bap_blocks_t;
#endif  // __cplusplus

  extern void asmir_set_print_warning(bool value);
  extern bool asmir_get_print_warning();

  /*
  /// Enable/disable eflags thunks code. 
  extern void set_use_eflags_thunks(bool value);
  extern bool get_use_eflags_thunks();
  /// If 1, calls/returns translated as Call/Return.
  /// If 0, calls/returns translated as Jmp
  extern void set_call_return_translation(int value);
  extern Stmt** gen_eflags_helpers_c();
  */

  extern void asmir_set_use_simple_segments(bool value);

  /* extern bap_blocks_t * asmir_asmprogram_to_bap(asm_program_t *prog); */
  /* extern bap_blocks_t * asmir_asmprogram_range_to_bap(asm_program_t *prog, address_t start, address_t end); */
  extern asm_program_t* byte_insn_to_asmp(bfd_architecture arch, address_t addr, unsigned char *bb_bytes, unsigned int len);
  extern bap_block_t* asmir_addr_to_bap(asm_program_t *p, address_t addr, address_t *next);

  extern int asmir_bap_blocks_size(bap_blocks_t *bs);
  extern bap_block_t * asmir_bap_blocks_get(bap_blocks_t *bs, int i);
  extern long asmir_bap_blocks_error(bap_blocks_t *bs);
  extern long asmir_bap_block_error(bap_block_t *bs);
  extern void destroy_bap_blocks(bap_blocks_t *bs);
  extern void destroy_bap_block(bap_block_t *bs);
  extern int asmir_bap_block_size(bap_block_t *b);
  extern address_t asmir_bap_block_address(bap_block_t *b);
  extern Stmt * asmir_bap_block_get(bap_block_t *b, int i);
  extern const char* asm_string_from_block(bap_block_t *b);
  extern char* string_blockinsn(asm_program_t *prog, bap_block_t *block);
  extern bap_blocks_t * asmir_bap_from_trace_file(char *filename, uint64_t offset, uint64_t numisns, long atts, long pintrace);
  extern trace_frames_t * asmir_frames_from_trace_file(char *filename, uint64_t offset, uint64_t numisns);
  extern void asmir_frames_destroy(trace_frames_t *tfs);
  extern int asmir_frames_length(trace_frames_t *tfs);
  extern trace_frame_t * asmir_frames_get(trace_frames_t *tfs, int index);
  extern pintrace::FrameType asmir_frame_type(trace_frame_t *tf);
  extern int asmir_frame_tid(trace_frame_t *tf);
  extern uint8_t * asmir_frame_get_insn_bytes(trace_frame_t *tf, uint64_t *addrout, int *len);

  extern const char* asmir_frame_get_loadmod_info(trace_frame_t *tf, uint64_t *lowout, uint64_t *highout);
  extern void asmir_frame_get_syscall_info(trace_frame_t *tf, int *callno, uint64_t *addr, int *tid);
  extern void asmir_frame_get_except_info(trace_frame_t *tf, int *exceptno, int *tid, uint64_t *from_addr, uint64_t *to_addr);
  extern conc_map_vec * asmir_frame_get_operands(trace_frame_t *tf);
  extern void asmir_frame_destroy_operands(conc_map_vec *cv);
  extern int asmir_frame_operands_length(conc_map_vec *cv);
  extern ConcPair* asmir_frame_get_operand(conc_map_vec *cv, int num);
#ifdef __cplusplus
}
#endif

#endif

