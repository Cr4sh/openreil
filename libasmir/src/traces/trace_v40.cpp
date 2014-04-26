#include "trace_v40.h"


Trace_v40::Trace_v40(ifstream * tracefile) : Trace(tracefile) { }

void Trace_v40::read_taint_record(taint_record_t * rec)
{
	trace->read(BLOCK(*rec), TAINT_RECORD_FIXED_SIZE);
	int i;
	// the number of records is unused in version 40
	for ( i = 0 ; i < 3 ; i ++ )
		read_taint_byte_record(&rec->taintBytes[i]);
}

void Trace_v40::read_operand(OperandVal * operand)
{
	trace->read(BLOCK(*operand), OPERAND_VAL_FIXED_SIZE);
	int i;
        for (i = 0 ; i < 4 ; i ++)
            read_taint_record(&operand->records[i]);
}



