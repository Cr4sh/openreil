
#include "trace_v50.h"

Trace_v50::Trace_v50(ifstream * tracefile) : Trace(tracefile){}

void Trace_v50::read_taint_record(taint_record_t * rec)
{
        rec->taint_propag = -1 ;
        rec->numRecords = 0 ;
	trace->read(BLOCK(rec->numRecords), CHAR);
	int i;
	if (rec->numRecords > MAX_NUM_TAINTBYTE_RECORDS) {
		cerr << "num: " << rec->numRecords << endl ; 
		throw "taintbyte records number exceeds maximum";
	}
	assert(rec->numRecords <= 3);
	for ( i = 0 ; i < rec->numRecords ; i ++ )
		read_taint_byte_record(&rec->taintBytes[i]);

}

void Trace_v50::read_records(taint_record_t records[MAX_OPERAND_LEN], uint32_t length, uint64_t tainted)
{
	int i;
	if (length > MAX_OPERAND_LEN) {
		cerr << length << endl; 
		throw "operand length exceeds maximum";
	}
	assert(length <= 8);
	for ( i = 0 ; i < length ; i ++ )
		if (tainted & (1L<<i))
			read_taint_record(&records[i]);

}

void Trace_v50::read_entry_header(EntryHeader * eh)
{
	// reading in the static part of entry header
	trace->read(BLOCK(*eh), ENTRY_HEADER_FIXED_SIZE_v50);
	trace->read(BLOCK(eh->rawbytes), eh->inst_size);
	read_operands(eh->operand, eh->num_operands);
}

void Trace_v50::read_process()
{

	ProcRecord proc;
	trace->read(BLOCK(proc), PROC_RECORD_FIXED_SIZE_v50); 
	int i;
	for (i = 0 ; i < proc.n_mods ; i ++)
		read_module();

}

void Trace_v50::consume_header(TraceHeader * hdr)
{
        trace->read(BLOCK(hdr->gdt_base), INT32); 
        trace->read(BLOCK(hdr->idt_base), INT32); 
        read_procs(hdr);
}

void Trace_v50::read_operand(OperandVal * operand)
{
        operand->length = 0 ;
        operand->tainted = 0 ;
        trace->read(BLOCK(operand->acc), CHAR);
        trace->read(BLOCK(operand->length), CHAR);
        trace->read(BLOCK(operand->tainted), INT16);
        trace->read(BLOCK(operand->addr), INT32);
        trace->read(BLOCK(operand->value), INT32);
        trace->read(BLOCK(operand->type), CHAR);
        trace->read(BLOCK(operand->usage), CHAR);
        read_records(operand->records, operand->length, operand->tainted);
}

