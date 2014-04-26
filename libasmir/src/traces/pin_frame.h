// -*- c++ -*-

#pragma once

#include <iostream>
#include <stdint.h>
#include <vector>
#include "pin_common.h"

/*
 * It turns out that if you try to include the get operands code in
 * the experimental pin tool, the BAP backend and pin.H both define
 * BOOL.  There did not seem to be an easy way of solving this, and so
 * I am adding a define to optionally pull in the BAP code needed for
 * get operands.  Previously this was done using multiple versions of
 * pin_frame and pin_trace, which was not fun to maintain.
 *
 * -ejs
 */

#ifdef GET_OPERANDS
#include "trace_vXX.h"
#endif

namespace pintrace { // Use namespace to avoid conflict

/* Register definition: copied from Pin's types_vampi.TLH */

  //#ifdef GET_OPERANDS

const uint32_t MAX_BYTES_PER_PIN_REG = 32;
const uint32_t MAX_WORDS_PER_PIN_REG = (MAX_BYTES_PER_PIN_REG/2);
const uint32_t MAX_DWORDS_PER_PIN_REG = (MAX_WORDS_PER_PIN_REG/2);
const uint32_t MAX_QWORDS_PER_PIN_REG = (MAX_DWORDS_PER_PIN_REG/2);
const uint32_t MAX_FLOATS_PER_PIN_REG = (MAX_BYTES_PER_PIN_REG/sizeof(float));
const uint32_t MAX_DOUBLES_PER_PIN_REG = (MAX_BYTES_PER_PIN_REG/sizeof(double));


union PIN_REGISTER
{
  uint8_t  byte[MAX_BYTES_PER_PIN_REG];
  uint16_t word[MAX_WORDS_PER_PIN_REG];
  uint32_t dword[MAX_DWORDS_PER_PIN_REG];
  uint64_t qword[MAX_QWORDS_PER_PIN_REG];
  
  int8_t   s_byte[MAX_BYTES_PER_PIN_REG];
  int16_t  s_word[MAX_WORDS_PER_PIN_REG];
  int32_t  s_dword[MAX_DWORDS_PER_PIN_REG];
  int64_t  s_qword[MAX_QWORDS_PER_PIN_REG];

  float  flt[MAX_FLOATS_PER_PIN_REG];
  double  dbl[MAX_DOUBLES_PER_PIN_REG];

};

  //#endif

/***************** Syscalls ***************/
// FIXME: use the ones from /usr/include/asm/unistd.h
     
#define __NR_read		  3
#define __NR_open		  5
#define __NR_close		  6
#define __NR_execve		 11
#define __NR_mmap		 90
#define __NR_socketcall	102
#define __NR_mmap2		192

#define REG_BASE 0x1
#define MEM_BASE 0x50
#define REGTYPE_LAST (MEM_BASE-1)
#define MEMTYPE_LAST (MEM_BASE+0x50)
// Value specifier type.
#define VT_NONE     0x0
#define VT_REG8     (REG_BASE+0x0)
#define VT_REG16    (REG_BASE+0x1)
#define VT_REG32    (REG_BASE+0x2)
#define VT_REG64    (REG_BASE+0x3)
#define VT_REG128   (REG_BASE+0x4)
#define VT_MEM8     (MEM_BASE+0x0)
#define VT_MEM16    (MEM_BASE+0x1)
#define VT_MEM32    (MEM_BASE+0x2)
#define VT_MEM64    (MEM_BASE+0x3)
#define VT_MEM128   (MEM_BASE+0x4)
#define VT_MEM256   (MEM_BASE+0x5)

    typedef enum {
        NONE = 0,
        REGISTER = 1,
        MEM = 2,
    } RegMemEnum_t;

    
    typedef struct RegMem_s {
        RegMemEnum_t type;  
        uint32_t size;   // In bits
    } RegMem_t;


    static const RegMem_t INVALIDREGMEM = { NONE, 8 };

    extern bool valid_regmem_type(RegMem_t rm);
    
enum FrameType {

   // Invalid frame type.
   FRM_NONE = 0,

   // Keyframe. Supplies some register values
   FRM_KEY = 1,

   // "Regular" frame. Sufficient to handle majority of x86 instructions.
   FRM_STD = 2,

   // Frame indicating a module was loaded.
   FRM_LOADMOD = 3,

   // Frame containing information about a system call.
   FRM_SYSCALL = 4,

   // Frame taint information
   FRM_TAINT = 5,

   FRM_STD2 = 6,

   // Exception frame
   FRM_EXCEPT = 7,

   // Better key frame. Can supply all register values,
   // memory mappings, or both
   FRM_KEY_GENERAL = 8,
};


  // Counter for the taint source

  extern int source;
  
   /**
    * Frame: Base struct for all frame objects.
    */
   struct Frame {

      FrameType type;

      Frame(FrameType ty) : type(ty) {}

      //
      // Serializes the frame and outputs to the ostream. 'sz' is the size
      // of the "child" frame, i.e. the size of all data that comes after
      // this frame's data.
      //
      virtual std::ostream &serialize(std::ostream &out, uint16_t sz = 0) = 0;

      virtual std::istream &unserializePart(std::istream &in) = 0;

      //
      // The main unserialization function. Returns a new Frame object.
      // If 'noskip' is false, then the serialized data representing the
      // frame will merely be skipped and not constructed in an object;
      // NULL will be returned instead. This can be used to quickly skip
      // to the next frame.
      //
      static Frame *unserialize(std::istream &in, bool noskip = true);

   };

   /*
    * StdFrame: Standard frame, used to log majority of instructions.
    *
    * Packed format:
    *   4 bytes -> Address (addr)
    *   4 bytes -> Thread ID (tid)
    *   1 byte  -> Lengths (packed values_count and insn_length)
    *   N bytes -> Raw bytes (rawbytes) where N == insn_length * sizeof(char)
    *   N bytes -> Cache mask, where N = ceil(values_count / 8.0)
    *   N bytes -> Values where N = values_count * 4
    *
    *
    * Additional notes:
    *   Although StdFrame objects have an insn_length field, this field
    *   will not always reflect the actual length of the instruction. If
    *   the value of this field is set to 0, then the instruction length
    *   should be inferred by parsing the raw instruction bytes as an x86
    *   instruction, i.e. the length of the instruction is implicit in its
    *   x86 encoding. The rawbytes field will always be padded with extra
    *   garbage bytes after the instruction bytes to make up
    *   MAX_INSN_BYTES bytes.
    *
    * RULES FOR STORING VALUES:
    *   The values array can hold up to MAX_VALUES_COUNT "values". Each
    *   value corresponds to the concrete value of a particular register
    *   or memory location read by the instruction. Note that we only need
    *   to store values that were read, and not those that were written.
    *   The order of the values is as follows:
    *
    *     For each operand (both explicit and implicit), ordered based on
    *     Pin's operand ordering scheme:
    *        If the operand is a register to be read:
    *           If a segment register is specified:
    *              Store the value of the segment register.
    *           EndIf
    *           Store the value of the register to be read.
    *        Else if the operand is a memory location:
    *           If a segment register was specified:
    *              Store the value of the segment register.
    *           EndIf
    *           If a base register is specified:
    *              Store the value of the base register.
    *           EndIf
    *           If an index register is specified:
    *              Store the value of the index register.
    *           EndIf
    *        EndIf
    *     EndFor
    *     If this instruction is a memory read:
    *        Store the value of the memory location being read.
    *     EndIf
    *     If this instruction has any subsequent memory reads:
    *        Store the values of the memory locations being read.
    *     EndIf
    *
    *   i.e. the values array will start with the values of all registers
    *   that will be read by the processor, whether to use the value in an
    *   operation or to use the value to calculate an effective
    *   address. The registers are stored in the order of the operands
    *   they occur in. Finally, if the instruction reads from memory, the
    *   value of the memory location will be stored as well.
    *
    * VALUE CACHING:
    *   The packed StdFrame representation uses a data cache to reduce the
    *   amount of values that need to be stored in the trace. In order to
    *   denote which values can be found in the cache and which are stored
    *   in the current frame, a cache mask is used. The cache mask uses 1
    *   bit per value that needs to be stored in the frame; if the bit is
    *   NOT set, then the value in the cache is invalid and the right
    *   value is found in the frame. Else, the trace reader should
    *   retrieve the value from the cache, i.e. 1 for in cache, 0 for not
    *   in cache.  The cache mask is always long enough such that there
    *   are sufficient bits to represent every needed value in the
    *   trace. It is thus values_count bits long, rounded up to a multiple
    *   of 8 in order to fit in an integral number of bytes. The mapping
    *   between value and cache map bit is as follows: the first value
    *   maps to the least significant bit of the first byte, and so on.
    *
    *
    */
   struct StdFrame : public Frame {

      uint32_t addr;
      uint32_t tid;
      uint8_t insn_length;         // Must be <= MAX_INSN_BYTES
      uint8_t values_count;          // Must be <= MAX_VALUES_COUNT
      char rawbytes[MAX_INSN_BYTES];
      char cachemask[MAX_CACHEMASK_BTYES];
      uint32_t values[MAX_VALUES_COUNT];
      uint32_t types[MAX_VALUES_COUNT];
      uint32_t usages[MAX_VALUES_COUNT];
      uint32_t locs[MAX_VALUES_COUNT];
      uint32_t taint[MAX_VALUES_COUNT];

      StdFrame() : Frame(FRM_STD) {}
      virtual std::ostream &serialize(std::ostream &out, uint16_t sz = 0);
      virtual std::istream &unserializePart(std::istream &in);

      void clearCache();

#ifdef GET_OPERANDS
      conc_map_vec * getOperands();
#endif
     
      // TODO: Removing the bounds checking in the functions below might
      // lead to some speedups. Test and see if the risk is worth it.

      // Note: x & 7 === x % 8
      bool isCached(uint32_t pos)
      {
         if (pos < MAX_VALUES_COUNT)
            return (cachemask[pos >> 3] >> (pos & 7)) & 1;
         else
            return false;
      }

      void setCached(uint32_t pos)
      {
         if (pos < MAX_VALUES_COUNT)
            cachemask[pos >> 3] |= (1 << (pos & 7));
      }

      void unsetCached(uint32_t pos)
      {
         if (pos < MAX_VALUES_COUNT)
            cachemask[pos >> 3] &= ~(1 << (pos & 7));
      }

   };

   /*
    * StdFrame2: Standard frame 2, used to log majority of
    * instructions.  Supports registers larger than 32-bits.
    *
    * Packed format:
    *   4 bytes -> Address (addr)
    *   4 bytes -> Thread ID (tid)
    *   1 byte  -> Lengths (packed values_count and insn_length)
    *   N bytes -> Raw instruction bytes (rawbytes) where N == insn_length * sizeof(char)
    *   N bytes -> Cache mask, where N = ceil(values_count / 8.0)
    *   N bytes -> Types  where N = values_count * 4
    *   N bytes -> Usages where N = values_count * 4
    *   N bytes -> Locs   where N = values_count * 4
    *   N bytes -> Taint  where N = values_count * 4
    *   N bytes -> Values, where the size of the values comes from the types
    *
    *
    * Additional notes:
    *   Although StdFrame objects have an insn_length field, this field
    *   will not always reflect the actual length of the instruction. If
    *   the value of this field is set to 0, then the instruction length
    *   should be inferred by parsing the raw instruction bytes as an x86
    *   instruction, i.e. the length of the instruction is implicit in its
    *   x86 encoding. The rawbytes field will always be padded with extra
    *   garbage bytes after the instruction bytes to make up
    *   MAX_INSN_BYTES bytes.
    *
    * RULES FOR STORING VALUES:
    *   The values array can hold up to MAX_VALUES_COUNT "values". Each
    *   value corresponds to the concrete value of a particular register
    *   or memory location read by the instruction. Note that we only need
    *   to store values that were read, and not those that were written.
    *   The order of the values is as follows:
    *
    *     For each operand (both explicit and implicit), ordered based on
    *     Pin's operand ordering scheme:
    *        If the operand is a register to be read:
    *           If a segment register is specified:
    *              Store the value of the segment register.
    *           EndIf
    *           Store the value of the register to be read.
    *        Else if the operand is a memory location:
    *           If a segment register was specified:
    *              Store the value of the segment register.
    *           EndIf
    *           If a base register is specified:
    *              Store the value of the base register.
    *           EndIf
    *           If an index register is specified:
    *              Store the value of the index register.
    *           EndIf
    *        EndIf
    *     EndFor
    *     If this instruction is a memory read:
    *        Store the value of the memory location being read.
    *     EndIf
    *     If this instruction has any subsequent memory reads:
    *        Store the values of the memory locations being red.
    *     EndIf
    *
    *   i.e. the values array will start with the values of all registers
    *   that will be read by the processor, whether to use the value in an
    *   operation or to use the value to calculate an effective
    *   address. The registers are stored in the order of the operands
    *   they occur in. Finally, if the instruction reads from memory, the
    *   value of the memory location will be stored as well.
    *
    * VALUE CACHING:
    *   The packed StdFrame representation uses a data cache to reduce the
    *   amount of values that need to be stored in the trace. In order to
    *   denote which values can be found in the cache and which are stored
    *   in the current frame, a cache mask is used. The cache mask uses 1
    *   bit per value that needs to be stored in the frame; if the bit is
    *   NOT set, then the value in the cache is invalid and the right
    *   value is found in the frame. Else, the trace reader should
    *   retrieve the value from the cache, i.e. 1 for in cache, 0 for not
    *   in cache.  The cache mask is always long enough such that there
    *   are sufficient bits to represent every needed value in the
    *   trace. It is thus values_count bits long, rounded up to a multiple
    *   of 8 in order to fit in an integral number of bytes. The mapping
    *   between value and cache map bit is as follows: the first value
    *   maps to the least significant bit of the first byte, and so on.
    *
    *
    */
   struct StdFrame2 : public Frame {

      uint32_t addr;
      uint32_t tid;
      uint8_t insn_length;         // Must be <= MAX_INSN_BYTES
      uint8_t values_count;          // Must be <= MAX_VALUES_COUNT
      char rawbytes[MAX_INSN_BYTES];
      char cachemask[MAX_CACHEMASK_BTYES];
      PIN_REGISTER values[MAX_VALUES_COUNT];
      uint32_t types[MAX_VALUES_COUNT];
      uint32_t usages[MAX_VALUES_COUNT];
      uint32_t locs[MAX_VALUES_COUNT];
      uint32_t taint[MAX_VALUES_COUNT];

      StdFrame2() : Frame(FRM_STD2) {}
      virtual std::ostream &serialize(std::ostream &out, uint16_t sz = 0);
      virtual std::istream &unserializePart(std::istream &in);

      void clearCache();

#ifdef GET_OPERANDS
      conc_map_vec * getOperands();
#endif
     
      // TODO: Removing the bounds checking in the functions below might
      // lead to some speedups. Test and see if the risk is worth it.

      // Note: x & 7 === x % 8
      bool isCached(uint32_t pos)
      {
         if (pos < MAX_VALUES_COUNT)
            return (cachemask[pos >> 3] >> (pos & 7)) & 1;
         else
            return false;
      }

      void setCached(uint32_t pos)
      {
         if (pos < MAX_VALUES_COUNT)
            cachemask[pos >> 3] |= (1 << (pos & 7));
      }

      void unsetCached(uint32_t pos)
      {
         if (pos < MAX_VALUES_COUNT)
            cachemask[pos >> 3] &= ~(1 << (pos & 7));
      }

   };
  
   /*
    * KeyFrame: Keyframe, used to resynchronize the execution trace.
    *
    * Packed format:
    *    8 bytes -> Position (pos)
    *   48 bytes -> Values of all registers.
    *
    * Additional notes:
    *   The keyframe should be inserted once every few thousand (or
    *   hundred, or million) frames. It is used to resynchronize the
    *   trace. The keyframe contains the values of all registers at that
    *   current point in the trace, which are used to populate the
    *   register value cache. At the same time, when a keyframe is
    *   encountered, the memory cache should be cleared, i.e. all frames
    *   following the keyframe cannot refer to memory values cached prior
    *   to the keyframe.
    *
    *   The keyframe also contains positional information, so readers will
    *   know where the keyframe is located relative to the stream. The
    *   'pos' attribute holds the count of the number of instruction
    *   frames that are in the trace prior to the keyframe. Note that
    *   special frames such as SyscallFrames and LoadModuleFrames are not
    *   part of the instruction count. 'pos' can thus be used to determine
    *   how many instructions have been executed just before the keyframe
    *   was added.
    *
    */
   struct KeyFrame : public Frame {

      uint64_t pos;

      uint32_t eax;
      uint32_t ebx;
      uint32_t ecx;
      uint32_t edx;
      uint32_t esi;
      uint32_t edi;
      uint32_t esp;
      uint32_t ebp;
      uint32_t eflags;

      uint16_t cs;
      uint16_t ds;
      uint16_t ss;
      uint16_t es;
      uint16_t fs;
      uint16_t gs;

      // XXX: Additional registers?

      KeyFrame() : Frame(FRM_KEY) {}
      virtual ~KeyFrame() { }
      virtual std::ostream &serialize(std::ostream &out, uint16_t sz = 0);
      virtual std::istream &unserializePart(std::istream &in);

      void setAll(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx,
                  uint32_t esi, uint32_t edi, uint32_t esp, uint32_t ebp,
                  uint32_t eflags,
                  uint16_t cs, uint16_t ds, uint16_t ss,
                  uint16_t es, uint16_t fs, uint16_t gs);

   };

   /*
    * KeyFrameGeneral: Keyframe, contains many register or memory locations
    *
    * Packed format:
    *    8 bytes -> Position (pos)
    *    4 bytes -> numRegs
    *    4 bytes -> numMems
    *    The rest is variable.
    */
   struct KeyFrameGeneral : public Frame {

     uint64_t pos;
     uint32_t numRegs; /* Number of registers */
     uint32_t* regIds; /* Array of numRegs register ids */
     uint32_t* regTypes; /* Array of numRegs register types */
     PIN_REGISTER* regValues; /* Array of numRegs values */
     uint32_t numMems; /* Number of memory locations included */
     uint32_t* memAddrs; /* Array of numMems memory addresses */
     uint8_t* memValues; /* Array of numMems memory values */
     

      KeyFrameGeneral() : Frame(FRM_KEY_GENERAL) {
        regIds = NULL;
        regTypes = NULL;
        regValues = NULL;
        memAddrs = NULL;
        memValues = NULL;
      }
     /* Do a deep copy in the copy constructor */
     KeyFrameGeneral(const KeyFrameGeneral &copy) : Frame(FRM_KEY_GENERAL) {
       pos = copy.pos;
       numRegs = copy.numRegs;
       regIds = new uint32_t[numRegs];
       std::copy(copy.regIds, copy.regIds+numRegs, regIds);
       regTypes = new uint32_t[numRegs];
       std::copy(copy.regTypes, copy.regTypes+numRegs, regTypes);
       regValues = new union PIN_REGISTER[numRegs];
       std::copy(copy.regValues, copy.regValues+numRegs, regValues);
       numMems = copy.numMems;
       memAddrs = new uint32_t[numMems];
       std::copy(copy.memAddrs, copy.memAddrs+numMems, memAddrs);
       memValues = new uint8_t[numMems];
       std::copy(copy.memValues, copy.memValues+numMems, memValues);
     }
     ~KeyFrameGeneral() {
       if (regIds) delete[] regIds;
       if (regTypes) delete[] regTypes;
       if (regValues) delete[] regValues;
       if (memAddrs) delete[] memAddrs;
       if (memValues) delete[] memValues;
     }
     virtual std::ostream &serialize(std::ostream &out, uint16_t sz = 0);
     virtual std::istream &unserializePart(std::istream &in);
#ifdef GET_OPERANDS
     conc_map_vec * getOperands();
#endif
   };
  
   /*
    * LoadModuleFrame : Records information about a loaded module.
    *
    * Packed format:
    *  XXX
    *
    * Additional notes:
    *
    */
   struct LoadModuleFrame : public Frame {

      uint32_t low_addr;
      uint32_t high_addr;
      uint32_t start_addr;
      uint32_t load_offset;
      char name[64];
      
      LoadModuleFrame() : Frame(FRM_LOADMOD) {}
      virtual std::ostream &serialize(std::ostream &out, uint16_t sz = 0);
      virtual std::istream &unserializePart(std::istream &in);
      
   };

   /*
    * SyscallFrame : A system call.
    *
    * Packed format:
    *   XXX
    *
    * Additional notes:
    *
    */
   struct SyscallFrame : public Frame {

      uint32_t addr;
      uint32_t tid;
      uint32_t callno;
      uint32_t args[MAX_SYSCALL_ARGS];
      
      SyscallFrame() : Frame(FRM_SYSCALL) {}
      virtual std::ostream &serialize(std::ostream &out, uint16_t sz = 0);
      virtual std::istream &unserializePart(std::istream &in);
#ifdef GET_OPERANDS
     conc_map_vec * getOperands();
#endif     
   };

  /* XXX: TaintFrame really should have an id for each byte! */
  
   struct TaintFrame : public Frame {

      uint32_t id;
      uint32_t length;
      uint32_t addr;
      
      TaintFrame() : Frame(FRM_TAINT) {}
      virtual std::ostream &serialize(std::ostream &out, uint16_t sz = 0);
      virtual std::istream &unserializePart(std::istream &in);
#ifdef GET_OPERANDS
     conc_map_vec * getOperands();
#endif     
   };

   /* ExceptionFrame: Exceptionframe, used to capture state if there is a 
    *                 processor exception.
    *
    * Packed format:
    *  XXX
    *
    * Additional notes:
    * XXX
    *
    */
   struct ExceptionFrame : public Frame {

       uint32_t exception;
       uint32_t tid;
       // XXX Can these be 64 bit?
       uint32_t from_addr;
       uint32_t to_addr;

      ExceptionFrame() : Frame(FRM_EXCEPT) {}
      virtual std::ostream &serialize(std::ostream &out, uint16_t sz = 0);
      virtual std::istream &unserializePart(std::istream &in);
   };


}; // END of namespace

typedef pintrace::Frame trace_frame_t;
typedef std::vector<trace_frame_t*> trace_frames_t;
