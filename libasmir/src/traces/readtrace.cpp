#include <cassert>
#include <cstdlib>
#include <string>
#include <iostream>
#include "readtrace.h"
#include "asm_program.h"

using namespace std;

/* Read frames from trace to be pulled into ML.  This allows ML code to lift. */
trace_frames_t * read_frames_from_file(const string &filename,
                                       uint64_t offset,
                                       uint64_t numisns)
{
  // a vector to store our frames in
  trace_frames_t * result = new trace_frames_t;
  
  uint64_t counter = 0LL;
  pintrace::TraceReader tr;

  //cerr << "reading from offset " << offset << endl;

    try {
      tr.open(filename.c_str());
    }
    catch (pintrace::TraceExn& e) {
      cerr << "Trace exception: " << e.msg << endl;
      throw e;
    }
    
    // Initializations
    translate_init();

    //    cerr << "plz seek to " << offset << " of " << tr.count() << endl;

    if (!tr.seek(offset)) {
      /* Couldn't seek there! */
      delete result;
      return NULL;
    }
    
    while(
      (tr.pos() < tr.count()) && // Don't go past the end of the trace
      ((numisns == 0) ? true : counter < numisns) // Count numisns
          ) {

      // Reading each instruction
      pintrace::Frame *f = tr.next();
      assert(f);
      counter += 1;

      result->push_back(f);

      /* XXX: Make sure we delete frames once we're done with them. */
      
    }
      
    return result;
}

void destroy_trace_frames(trace_frames_t *v) {
  trace_frames_t::iterator i;

  for (i = v->begin(); i != v->end(); i++) {
    // Free the frame
    delete *i;
  }

  // Free the vector
  delete v;
}

bap_blocks_t * read_trace_from_file(const string &filename,
				    uint64_t offset,
                                    uint64_t numisns,
				    bool print, 
				    bool atts, 
				    bool pintrace)
{
  // a block to accumulate the lifted traces
  bap_blocks_t * result = new bap_blocks_t;
  uint64_t counter = 0LL;
  pintrace::TraceReader tr;

  //cerr << "reading from offset " << offset << endl;

  if(pintrace) {

    try {
      tr.open(filename.c_str());
    }
    catch (pintrace::TraceExn& e) {
      cerr << "Trace exception: " << e.msg << endl;
      throw e;
    }

    // FIXME: Currently only x86
    VexArch arch = VexArchX86;
    bfd_architecture bfd_arch = bfd_arch_i386;
    asm_program_t * prog = asmir_trace_asmp_for_arch(bfd_arch);
    assert(prog);
    assert(prog->abfd);
    
    // Initializations
    translate_init();

    //    cerr << "plz seek to " << offset << " of " << tr.count() << endl;

    if (!tr.seek(offset)) {
      /* Couldn't seek there! */
      return NULL;
    }
    
    while(
      (tr.pos() < tr.count()) && // Don't go past the end of the trace
      ((numisns == 0) ? true : counter < numisns) // Count numisns
          ) {

      // Reading each instruction
      pintrace::Frame *f = tr.next();
      assert(f);
      counter += 1;

      //      cerr << "pos " << tr.pos() << " of " << tr.count() << endl;
      
      switch(f->type) {
          
      case pintrace::FRM_STD: // TODO: We should consider key frame
          {
              assert(false);
              break;
          }
          
      case pintrace::FRM_STD2: // TODO: We should consider key frame
          {
              //static int num = 0;
              //cerr << "Raising frame " << num++ << endl;
              pintrace::StdFrame2 *cur_frm = (pintrace::StdFrame2 *) f;

              /* Set up the trace_read_memory function so the
               * disassembler is happy. */
              set_trace_bytes(cur_frm->rawbytes, cur_frm->insn_length,
                              cur_frm->addr);
          
              bap_block_t *bblock = new bap_block_t;
              bblock->bap_ir = new vector<Stmt *>();
              bblock->inst = cur_frm->addr;
              bblock->vex_ir =
                  translate_insn(arch,
                                 (unsigned char *)cur_frm->rawbytes,
                                 cur_frm->addr);
              // prog = byte_insn_to_asmp(bfd_arch, 
              //       		   cur_frm->addr,
              //       		   (unsigned char *)cur_frm->rawbytes,
              //       		   MAX_INSN_BYTES);
              generate_bap_ir_block(prog, bblock);

              // free internal VEX memory...
              // XXX Not necessary(?) as asmir_close will do this too
              vx_FreeAll();

              string assembly(asmir_string_of_insn(prog,
                                                   (bfd_vma)(cur_frm->addr)));
              ////string assembly("ed has disabled this; fix it");
              bblock->bap_ir->front()->assembly = assembly;
              if (atts)
                  bblock->bap_ir->front()->attributes.cv =
                      cur_frm->getOperands() ;
              bblock->bap_ir->front()->attributes.tid = cur_frm->tid;
          
              result->push_back(bblock);
              //for (int i = 0 ; i < bblock->bap_ir->size() ; i ++)
              //    cout << bblock->bap_ir->at(i)->tostring() << endl ;
              break;
          }
          
      case pintrace::FRM_SYSCALL: 
          {
              break;
          }
          
      case pintrace::FRM_LOADMOD:
          {
              std::stringstream ss;
              pintrace::LoadModuleFrame *lf = (pintrace::LoadModuleFrame*) f;
              bap_block_t *bblock = new bap_block_t;
              bblock->bap_ir = new vector<Stmt *>();
              bblock->inst = 0;
            
              /* Build string */
              ss.flags ( ios::hex );
              ss << "Loaded module " << lf->name << " at " << lf->low_addr
                 << " to " << lf->high_addr;
            
              Special *special = new Special(ss.str());
              bblock->bap_ir->push_back(special);
              bblock->vex_ir = NULL;
              result->push_back(bblock);
              break;
          }
          
      case pintrace::FRM_TAINT:
          {
              pintrace::TaintFrame * tf = (pintrace::TaintFrame *) f;
              bap_block_t *bblock = new bap_block_t;
              bblock->bap_ir = new vector<Stmt *>();
              bblock->inst = tf->id;
              //generate_bap_ir_block(prog, bblock);
              Label * label = new Label("ReadSyscall");
              if (atts)
                  label->attributes.cv = tf->getOperands() ;
              bblock->bap_ir->push_back(label);
              bblock->vex_ir = NULL;
              result->push_back(bblock);
          
              //cerr << sf->callno << " " << sf->args[0] << " " << sf->args[4] << endl;
              //for (int i = 0 ; i < bblock->bap_ir->size() ; i ++)
              //    cout << bblock->bap_ir->at(i)->tostring() << endl ;
              break;
	  
          }
          
      case pintrace::FRM_EXCEPT:   // TODO: Fill this in
          {
              break;
          }

      default:
          break;

      }

      /* After all this, delete the frame */
      delete f;
      /* Clean up prog, prog->abfd, and vex memory */
      asmir_close(prog);
    }
    
  }
  else {
    ifstream trace ;
    trace.open(filename.c_str());
    
    if (!trace.is_open()) {
      cerr << "Couldn't open " << filename << endl;
      exit(1);
    }
    
    Trace * trc ;
    EntryHeader eh;
    TraceHeader hdr;
    
    try {
      trace.read(BLOCK(hdr), TRACE_HEADER_FIXED_SIZE);
      
      assert(hdr.magicnumber == MAGIC_NUMBER) ;
      switch (hdr.version)
	{
	case 40: trc = new Trace_v40(&trace); break;
	case 41: trc = new Trace_v41(&trace); break;
	case 50: trc = new Trace_v50(&trace); break;
	default: throw "unsupported trace version";
	}
      trc->consume_header(&hdr) ;
      
      /* Since the traces do not contain any architecture information *
       * we only support x86 for now - ethan                          */
      VexArch arch = VexArchX86;
      /* XXX Suspected memory leak! (Is prog freed?) */
      asm_program_t * prog = asmir_new_asmp_for_arch(bfd_arch_i386);
      
      // Initializations
      translate_init();
      while(!trace.eof()) {
	// Reading each entry header
	trc->read_entry_header(&eh);
	
	counter ++ ;
	if (counter > offset) {

	  bap_block_t *bblock = new bap_block_t;
	  bblock->bap_ir = new vector<Stmt *>();
	  bblock->inst = eh.address;
	  // lift the instruction to VEX IL
	  bblock->vex_ir = translate_insn(arch, eh.rawbytes, eh.address);
	  // and then to BAP IL
	  generate_bap_ir_block(prog, bblock);
	  if (atts)
	    bblock->bap_ir->front()->attributes.cv = trc->operand_status(&eh) ;
	  // append to result
	  result->push_back(bblock);
	  int i;
	  if (print)
	    for ( i = 0 ; i < bblock->bap_ir->size() ; i ++)
	      cout << bblock->bap_ir->at(i)->tostring() << endl ;
	}
      }
    }
    catch (const char * s)
      {
	cout << s << endl ;
      }
  }
  return result;
}


