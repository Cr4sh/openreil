#ifndef _READTRACE_H_
#define _READTRACE_H_

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <assert.h>
#include "trace.h"
#include "trace_v40.h"
#include "trace_v41.h"
#include "trace_v50.h"
#include "irtoir.h"
#include "pin_trace.h"


extern "C" {

  trace_frames_t* read_frames_from_file(const string &filename,
                                        uint64_t offset,
                                        uint64_t numisns);

  bap_blocks_t * read_trace_from_file(const string &filename,
                                      uint64_t offset,
                                      uint64_t numisns,
                                      bool print,
                                      bool atts,
                                      bool pintrace);

  void destroy_trace_frames(trace_frames_t *v);
}
  
#endif
