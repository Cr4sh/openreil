
#ifndef _TRACE_VXX_H_
#define _TRACE_VXX_H_

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <assert.h>
#include <sstream>

#include "trace.h"
#include "reg_mapping.h"

class Trace
{

public:
	Trace(ifstream * tracefile);
	virtual void read_entry_header(EntryHeader * eh);
	virtual void read_taint_record(taint_record_t * rec);
	virtual void read_records(taint_record_t records[MAX_OPERAND_LEN], uint32_t length, uint64_t tainted);
	virtual void read_operand(OperandVal * operand);
	virtual void consume_header(TraceHeader * hdr);
    conc_map_vec * operand_status(EntryHeader * eh);
private:
    cval_type_t opsize_to_type(int size);
protected:
	ifstream * trace;
	void read_taint_byte_record(TaintByteRecord * byte_rec);
	void read_operands(OperandVal operand[MAX_NUM_OPERANDS], uint8_t num);
	void read_module();
	virtual void read_process();
	void read_procs(TraceHeader * hdr);

} ;

#endif
