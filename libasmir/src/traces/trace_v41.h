#ifndef _TRACE_V41_H_
#define _TRACE_V41_H_

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <assert.h>
#include "readtrace.h"
#include "trace_vXX.h"

class Trace_v41 : public Trace
{

public:

	Trace_v41(ifstream * tracefile);

} ;

#endif
