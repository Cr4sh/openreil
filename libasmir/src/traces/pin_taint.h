// -*- c++ -*-

/**** TODO: Add verbosity ****/

#pragma once

#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <vector>
#include <string.h>
#include "pin_frame.h"
#include "trace.container.hpp"

// Size of temporary buffers
#define BUFSIZE 128

// TODO: we need a type for the mapping to variables/registers
typedef uint32_t var;
// We only consider 32-bit memory addresses
// #define address uint32_t
// And enable 2^32 different taint tags
// special values: 
//  * 0        -> untainted
//  * ffffffff -> mixed taint
//  * other n  -> nth input byte
#define MIXED_TAINT 0xFFFFFFFF
#define NOTAINT 0
typedef uint32_t t;

#define MASK1 0x000000FF
#define MASK2 0x0000FF00
#define MASK3 0x00FF0000
#define MASK4 0xFF000000

typedef std::map<var,t> context;

// Some bit masks
#define LOW8   0xff
#define HIGH8  0xff000000
#define LOW16  0xffff
#define HIGH16 0xffff0000

/*********** IDs for taint sources **********/

#define ARG_ID 2
#define ENV_ID 2

/*************  Operand Usage  **************/

#define RD 0x01
#define WR 0x10
#define RW 0x11

/********************************************/

struct ValSpecRec {
  pintrace::RegMem_t type;               // Type of value specifier.
  uint32_t loc;                // Location of this value.
  pintrace::PIN_REGISTER value;// Actual value.
  uint32_t usage;              // Operand usage (R, RW, W, etc)
  uint32_t taint;              // Taint status of the value
};

/** Like [frame option] type in ML.  If b is true, then f is a valid,
 * meaningful frame.  If b is false, some failure returned and no
 * frame is present. */
struct FrameOption_t {
  frame f;
  bool b;

  FrameOption_t(bool b) {
    assert (b == false);
    this->b = b;
  }

  FrameOption_t(bool b, frame &tf) {
    this->f = f;
  }

  FrameOption_t() {

  }

};

/* globals */

extern int g_skipTaints;

/* functions */

bool defaultPolicy(uint32_t addr, uint32_t length, const char *msg);

namespace pintrace { // We will use namespace to avoid collision

  typedef bool(*TAINT_POLICY_FUN)(uint32_t addr, uint32_t length, const char *msg);

  struct fdInfo_t {
    fdInfo_t() {
      name = string("Uninitialized"); offset=-1;
    }
    fdInfo_t(string name_in, uint64_t offset_in) {
      name = name_in; offset = offset_in;
    }
    string name;
    uint64_t offset;
  };  

  typedef std::pair<string, uint64_t> resource_t;
  
   // Tracking the taint during program flow
   class TaintTracker {

   public:

     TaintTracker(ValSpecRec *env);

     /** A function to introduce taint in the contexts. Writes
	 information to state; this state must be passed to
	 taintIntro */
     bool taintPreSC(uint32_t callno, const uint64_t * args, uint32_t &state);

     FrameOption_t taintPostSC(const uint32_t bytes,
                               const uint64_t * args,
                               uint32_t &addr,
                               uint32_t &length,
                               const uint32_t state);

#ifdef _WIN32
     std::vector<frame> taintArgs(char *cmdA, wchar_t *cmdW);
#else
     std::vector<frame> taintArgs(int args, char **argv);
#endif

#ifdef _WIN32
     std::vector<frame> taintEnv(char *env, wchar_t *wenv);
#else
     std::vector<frame> taintEnv(char **env);
#endif

     // A function to propagate taint
     void taintPropagation(context &delta);

     // A function to apply taint policies
     bool taintChecking();

     void setTaintContext(context &delta);

     void resetTaint(context &delta);
     
     void setCount(uint32_t cnt);

     void setTaintArgs(bool taint);

     void setTaintEnv(string env_var);
      
     void trackFile(string file);

     void setTaintStdin();
      
     void setTaintNetwork();

     // Helpers
     // A function to check whether the instruction arguments are tainted
     uint32_t getReadTaint(context &delta);

     bool hasTaint(context &delta);

     //bool propagatedTaint(bool branch);
      
     void printMem();

     void printRegs(context &delta);

     void postSysCall(context &delta);

     void acceptHelper(uint32_t fd);

     FrameOption_t recvHelper(uint32_t fd, void *ptr, size_t len);

     uint32_t getRegTaint(context &delta, uint32_t reg_int);

     uint32_t getMemTaint(uint32_t addr, RegMem_t type);

     void untaintMem(uint32_t addr);
       
     static uint32_t getSize(RegMem_t type);

     static bool isValid(RegMem_t type);

     static bool isReg(RegMem_t type);

     static bool isMem(RegMem_t type);
    
     
   private:

     // The taint number (producing taint tags)
     uint32_t taintnum;

     // a context defining a map from registers to taint
     // this is maintainted externally now
     //context delta;

     // We can use a byte-centric approach, each byte maps to taint
     // a context defining a map from memory locations to taint
     context memory;

     // The table containing the values of the current instruction
     ValSpecRec *values;

     // How many values are being used
     uint32_t count;
     
     /********** Syscall-specific vars ***********/
     std::set<string> taint_files;
     std::map<resource_t, uint32_t> taint_mappings;
     std::map<uint32_t, fdInfo_t> fds;
     std::map<uint32_t,uint32_t> sections;
     bool taint_net;
     bool taint_args;
     std::set<string> taint_env;

     /********************************************/

     // The taint policy function
     TAINT_POLICY_FUN pf;

#ifdef _WIN32
     std::map<unsigned int, unsigned int> syscall_map;
#endif

     void addTaintToWritten(context &delta, uint32_t tag);
      
     uint32_t combineTaint(uint32_t oldtag, uint32_t newtag);

     uint32_t exists(context &ctx, uint32_t elem);

     uint32_t getTaint(context &ctx, uint32_t elem);

     FrameOption_t introMemTaint(uint32_t addr, uint32_t length, const char *source, int64_t offset);

     FrameOption_t introMemTaintFromFd(uint32_t fd, uint32_t addr, uint32_t length);
     
     void setTaint(context &ctx, uint32_t key, uint32_t tag);

   };

}; // End of namespace
