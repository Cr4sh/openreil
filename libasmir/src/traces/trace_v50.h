#ifndef _TRACE_V50_H_
#define _TRACE_V50_H_

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <assert.h>
#include "readtrace.h"
#include "trace_vXX.h"

#define ENTRY_HEADER_FIXED_SIZE_v50 24
#define PROC_RECORD_FIXED_SIZE_v50 44

class Trace_v50 : public Trace
{

public:

         Trace_v50(ifstream * tracefile);
    void read_process();
    void read_entry_header(EntryHeader * eh);
    void read_taint_record(taint_record_t * rec);
    void read_records(taint_record_t records[MAX_OPERAND_LEN], uint32_t length, uint64_t tainted);
    void read_operand(OperandVal * operand);
    void consume_header(TraceHeader * hdr);

} ;

#endif
