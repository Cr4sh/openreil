#include "config.h"
#include <string.h>
#include <bfd.h>
// maybe all the header files should be moved to 
// libasmir/include? - ethan
#include "./traces/pin_frame.h"
#include "./traces/readtrace.h"
#include "irtoir.h"
#include "asm_program.h"
#include "irtoir-internal.h"

#include "jumpbuf.h"

/* These are no longer used with BAP's native lifting code */

// bap_blocks_t * asmir_asmprogram_to_bap(asm_program_t *prog) {
//   vector<bap_block_t *> *res = new vector<bap_block_t *>();
//   // eww, references

//   // Set longjmp to handle possible VEX errors
//   jmp_buf_set = 1;
//   if (setjmp(vex_error) != 0) {
//     /* There was an exception */
//     jmp_buf_set = 0;
//     cerr << "There was an exception" << endl;
//     return NULL;
//   }

//   *res = generate_vex_ir(prog);
//   generate_bap_ir(prog, *res);
//   jmp_buf_set = 0;
//   return res;
// }


// bap_blocks_t *asmir_asmprogram_range_to_bap(asm_program_t *prog, 
// 					address_t start,
// 					address_t end)
// {
//   vector<bap_block_t *> *res = new vector<bap_block_t *>();
//   // eww, references

//   // Set longjmp to handle possible VEX errors
//   jmp_buf_set = 1;
//   if (setjmp(vex_error) != 0) {
//     /* There was an exception */
//     jmp_buf_set = 0;
//     cerr << "There was an exception" << endl;
//     return NULL;
//   }

//   *res = generate_vex_ir(prog, start, end);
//   generate_bap_ir(prog, *res);
//   jmp_buf_set = 0;
//   return res;
// }

int asmir_bap_blocks_size(bap_blocks_t *bs) {
  if (bs) {
    return bs->size();
  } else {
    return -1;
  }
}

bap_block_t * asmir_bap_blocks_get(bap_blocks_t *bs, int i) {
  return bs->at(i);
}

void destroy_bap_block(bap_block_t *b) {
  // FIXME: inst seems to be a pointer into the asm_program_t,
  // so we don't need to free it?
  // FIXME: stuff in vex_ir seems to be allocated in VEX's own heap?
  // If so, should provide a way to free that memory too?
  //free(b->vex_ir);

  if (b == NULL) {
    cerr << "WARNING: attempt to destroy NULL bap block" << endl;
  } else {

    for (vector<Stmt*>::iterator j = b->bap_ir->begin();
         j != b->bap_ir->end(); j++) {
      Stmt::destroy(*j);
    }
    delete b->bap_ir;
    delete b;
  }
}

void destroy_bap_blocks(bap_blocks_t *bs) {
  for (vector<bap_block_t *>::iterator i = bs->begin(); i != bs->end(); i++) {
    destroy_bap_block(*i);
  }
  delete bs;
}

/* Check for an error block sequence.  So ML code can figure out if
 * an error occured, since it can't inspect pointers directly. */
long asmir_bap_blocks_error(bap_blocks_t *bs) {

  if (bs == NULL) {
    return TRUE;
  }
  else {
    return FALSE;
  }
}

/* Check for an error block. */
long asmir_bap_block_error(bap_block_t *b) {
  return asmir_bap_blocks_error((bap_blocks_t*) b);
}

address_t asmir_bap_block_address(bap_block_t *b)
{
  return b->inst;
}

int asmir_bap_block_size(bap_block_t *b) {
  if (b == NULL) {
    return -1;
  } else {
  return b->bap_ir->size();
  }
}

Stmt * asmir_bap_block_get(bap_block_t *b, int i) {
  return b->bap_ir->at(i);
}

const char* asm_string_from_block(bap_block_t *b) {
  return b->bap_ir->front()->assembly.c_str();
}

// hmm, this isn't in bfd.h, but is in the documentation...
extern "C" {
  extern void *bfd_alloc (bfd *, bfd_size_type);
}

asm_program_t*
byte_insn_to_asmp(bfd_architecture arch, address_t addr, unsigned char *bb_bytes, unsigned int len)
{
  asm_program_t *prog = asmir_new_asmp_for_arch(arch);
  unsigned char *bytes = (unsigned char*)bfd_alloc(prog->abfd, len);
  assert(bytes);
  // copy the string, because the old one is freed when we return
  memcpy(bytes, bb_bytes, len);
  section_t *sec = (section_t*)bfd_alloc(prog->abfd, sizeof(section_t));
  assert(sec);
  
  sec->start_addr = addr;
  sec->datasize = len;
  sec->end_addr = addr+len;
  sec->data = bytes;
  sec->is_code = 1;
  sec->next = NULL;
  prog->segs = sec;

  struct disassemble_info *disasm_info = &prog->disasm_info;
  disasm_info->buffer = bytes;
  disasm_info->buffer_vma = addr;
  disasm_info->buffer_length = len;
  disasm_info->section = NULL;

  return prog;
}


// moved from ir_program.cpp
bap_block_t* asmir_addr_to_bap(asm_program_t *p, address_t addr, address_t *next)
{

  // Set longjmp to handle possible VEX errors
  jmp_buf_set = 1;
  if (setjmp(vex_error) != 0) {
    /* There was an exception */
    jmp_buf_set = 0;
    cerr << "There was an exception in asmir_addr_to_bap" << endl;
    return NULL;
  }
  
  translate_init();
  bap_block_t * bap_block = generate_vex_ir(p, addr);
  generate_bap_ir_block(p, bap_block);
  if (next)
      *next = addr + asmir_get_instr_length(p, addr);
  return bap_block;
}

bap_blocks_t * asmir_bap_from_trace_file(char * filename, 
                                         uint64_t offset,
                                         uint64_t numisns,
                                         long atts,
					 long pintrace)
{
  bap_blocks_t * b = read_trace_from_file(string(filename), 
					  offset,
                                          numisns,
					  false,
					  atts, 
					  pintrace);
  return b;
}

trace_frames_t * asmir_frames_from_trace_file(char *filename,
                                              uint64_t offset,
                                              uint64_t numisns) {
  return read_frames_from_file(string(filename),
                               offset,
                               numisns);
}

void asmir_frames_destroy(trace_frames_t *tfs) {
  if (tfs)
    destroy_trace_frames(tfs);
}

int asmir_frames_length(trace_frames_t *tfs) {
  if (tfs == NULL) return -1;

  return tfs->size();
}

trace_frame_t * asmir_frames_get(trace_frames_t *tfs, int index) {
  assert(tfs);

  return (*tfs)[index];
}

pintrace::FrameType asmir_frame_type(trace_frame_t *tf) {
  assert(tf);

  return tf->type;
}

int asmir_frame_tid(trace_frame_t *tf) {
  assert(tf);

  switch (tf->type) {
      case pintrace::FRM_STD2:
        pintrace::StdFrame2 *sf = dynamic_cast<pintrace::StdFrame2*> (tf);
        return sf->tid;
  }

  return -1;
}

uint8_t * asmir_frame_get_insn_bytes(trace_frame_t *tf, uint64_t *addrout, int *len) {
  assert(tf);

  /* Don't return anything for non-instruction frames */
  if (tf->type != pintrace::FRM_STD2) {
    *len = 0;
    *addrout = 0;
    return NULL;
  } else {
    /* Okay, let's get to business. */
    pintrace::StdFrame2 * sf2 = dynamic_cast<pintrace::StdFrame2*> (tf);

    *len = sf2->insn_length;
    *addrout = sf2->addr;
    /* Return a pointer to the data in the frame. Make sure we're done
       with it before freeing the frame! */
    return (uint8_t*) (sf2->rawbytes);
  }

  assert(false);
  return NULL;
}

const char* asmir_frame_get_loadmod_info(trace_frame_t *tf, uint64_t *lowout, uint64_t *highout) {
  assert(tf);
  assert(tf->type = pintrace::FRM_LOADMOD);

  pintrace::LoadModuleFrame *lf = dynamic_cast<pintrace::LoadModuleFrame*> (tf);
  
  *lowout = lf->low_addr;
  *highout = lf->high_addr;
  return lf->name;
}

void asmir_frame_get_syscall_info(trace_frame_t *tf, int *callno,
                                  uint64_t *addr, int *tid) {
  assert(tf);
  assert(tf->type = pintrace::FRM_SYSCALL);

  pintrace::SyscallFrame *sf = dynamic_cast<pintrace::SyscallFrame*> (tf);
  
  *addr = sf->addr;
  *tid = sf->tid;
  *callno = sf->callno;    
}

void asmir_frame_get_except_info(trace_frame_t *tf, int *exceptno,
                                 int *tid, uint64_t *from_addr,
                                 uint64_t *to_addr) {
  assert(tf);
  assert(tf->type = pintrace::FRM_EXCEPT);

  pintrace::ExceptionFrame *ef = dynamic_cast<pintrace::ExceptionFrame*> (tf);
  
  *from_addr = ef->from_addr;
  *to_addr = ef->to_addr;
  *tid = ef->tid;
  *exceptno = ef->exception;
}


conc_map_vec * asmir_frame_get_operands(trace_frame_t *tf) {
  assert(tf);  
  
  switch (tf->type) {
      case pintrace::FRM_STD2:
      {
        pintrace::StdFrame2 *f = dynamic_cast <pintrace::StdFrame2*> (tf);
        return f->getOperands();
        break;
      }
        
      case pintrace::FRM_SYSCALL:
      {
        pintrace::SyscallFrame *f = dynamic_cast <pintrace::SyscallFrame*> (tf);
        return f->getOperands();
        break;
      }
      
      case pintrace::FRM_TAINT:
      { 
        pintrace::TaintFrame *f = dynamic_cast <pintrace::TaintFrame*> (tf);
        return f->getOperands();
        break;
      }

      case pintrace::FRM_KEY_GENERAL:
      {
        pintrace::KeyFrameGeneral *f = dynamic_cast <pintrace::KeyFrameGeneral*> (tf);
        return f->getOperands();
        break;
      }
      
      default:
        return NULL;
        break;        
  }
}

void asmir_frame_destroy_operands(conc_map_vec *cv) {
  assert(cv);
  conc_map_vec::iterator i;
  
  for (i = cv->begin(); i != cv->end(); i++) {
    delete *i;
  }
  
  delete cv;
}

int asmir_frame_operands_length(conc_map_vec *cv) {
  if (cv) {
   return cv->size();
  } else {
    return -1;
  }
}

ConcPair* asmir_frame_get_operand(conc_map_vec *cv, int num) {
  assert(cv);
  assert(num >= 0);
  return (*cv)[num];
}
