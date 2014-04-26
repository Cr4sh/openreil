#ifndef _TRACE_V40_H_
#define _TRACE_V40_H_

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <assert.h>
#include "readtrace.h"
#include "trace_vXX.h"

class Trace_v40 : public Trace
{

public:

	Trace_v40(ifstream * tracefile);
        void read_taint_record(taint_record_t * rec);
        void read_operand(OperandVal * operand);


} ;

#endif
