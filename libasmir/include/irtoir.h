#ifndef IRTOIR_H
#define IRTOIR_H

typedef struct bap_block_s bap_block_t;
typedef struct bap_context_s bap_context_t;

//
// VEX headers (inside Valgrind/VEX/pub)
//
#include "vexmem.h"
#include "libvex.h"

#ifdef __cplusplus

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
    address_t inst;
    int inst_size;
    
    string str_mnem;
    string str_op;

    IRSB *vex_ir;
    vector<Stmt *> *bap_ir;
}; 


typedef struct flag_thunks_s
{
    // see CC_OP word description for target architecture in VEX documentation
    int op;

} flag_thunks_t; 

//
// Structure that keeps global state of translation process.
//
struct bap_context_s
{
    // current architecture
    VexArch guest;

    // address and size of current instruction
    address_t inst;
    int inst_size;

    // special Exp to record the AST for bit shift instructions
    Exp *count_opnd;

    // to keep current values of VEX flag thunk descriptors
    flag_thunks_t flag_thunks;

    // to keep current value of ITSTATE register
    uint32_t itstate;
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

extern "C" 
{

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
IRSB *translate_insn(VexArch guest, unsigned char *insn_start, unsigned int insn_addr, int *insn_size);

//
// Translate an IRSB into a vector of Stmts in our IR
vector<Stmt *> *translate_irbb(bap_context_t *context, IRSB *irbb);

}


//======================================================================
// 
// Utility functions that wrap the raw translation functions.
// These are what should be used to do the actual translation.
// See print-ir.cpp in ../../apps for examples of how to use these.
//
//======================================================================

// Initialize IR translator context
bap_context_t *init_bap_context(VexArch guest);

// Take an instrs and translate it into a VEX IR block
// and store it in a bap block
bap_block_t* generate_vex_ir(bap_context_t *context, uint8_t *data, address_t inst);

// Same as generate_vex_ir, but only for an address range
vector<bap_block_t *> generate_vex_ir(bap_context_t *context, uint8_t *data, address_t start, address_t end);

// Take a bap block that has gone through VEX translation and translate it
// to Vine IR.
void generate_bap_ir(bap_context_t *context, bap_block_t *block);

//
// Take a vector of bap blocks that have gone through VEX translation
// and translate them to Vine IR
//
// \param vblocks Vector of bap blocks with valid VEX IR translations
// \return Vector of bap blocks with the bap_ir field filled in
//
vector<bap_block_t *> generate_bap_ir(bap_context_t *context, vector<bap_block_t *> vblocks);

// Get registers list for guest's architecture
vector<VarDecl *> get_reg_decls(bap_context_t *context);

extern "C" 
{
typedef struct vector<bap_block_t *> bap_blocks_t;

#endif  // __cplusplus

#ifdef __cplusplus
}
#endif

#endif
