#include <string>
#include <iostream>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

using namespace std;

#include "common.h"

#ifdef LOG_TO_STDERR

uint32_t log_stderr_mask = LOG_MSG;

#endif

//----------------------------------------------------------------------
// Logging functions
//----------------------------------------------------------------------
uint32_t log_stderr(uint32_t mask)
{

#ifdef LOG_TO_STDERR

    int ret = log_stderr_mask;

    log_stderr_mask = mask;

    return ret;

#else

    return 0;

#endif

}

int log_init(uint32_t mask, const char *path)
{
    // ...

    return EINVAL;
}

int log_close(void)
{
    // ...

    return EINVAL;
}

void log_write(uint32_t level, const char *msg, ...)
{
    va_list mylist;
    va_start(mylist, msg);    

    char *buff = NULL;
    int len = vsnprintf(NULL, 0, msg, mylist); 
    if (len > 0 && (buff = (char *)malloc(len + 1)))
    {
        if (vsnprintf(buff, len + 1, msg, mylist) < 0)
        {
            free(buff);
            buff = NULL;
        }
    }
    
    va_end(mylist);    

    if (buff)
    {

#ifdef LOG_TO_STDERR

        if (level & log_stderr_mask)
        {
            cerr << buff << endl;
        }        

#endif

#ifdef LOG_TO_FILE

        // ...
#endif

        free(buff);
    }
}

//----------------------------------------------------------------------
// A panic function that prints a msg and terminates the program
//----------------------------------------------------------------------
void panic(string msg)
{
    string str = "panic(): " + msg;

    throw BapException(str);
}

void _panic(const char *msg)
{
    panic(string(msg));
}
