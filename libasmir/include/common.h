#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include "config.h"


#define LIBASMIR_VERSION "0.1"

typedef uint64_t address_t;


//
// Logging events
//
#define LOG_INFO    0x00000001  // regular message
#define LOG_WARN    0x00000002  // error
#define LOG_ERR     0x00000004  // warning
#define LOG_BIN     0x00000008  // instruction bytes
#define LOG_ASM     0x00000010  // instruction assembly code
#define LOG_VEX     0x00000020  // instruction VEX code
#define LOG_BIL     0x00000040  // instruction BAP IL code

// mask for generic purpose log messages
#define LOG_MSG (LOG_INFO | LOG_WARN | LOG_ERR)

// mask for machine code log messages
#define LOG_INSN (LOG_BIN | LOG_ASM)

// mask for IR code log messagess
#define LOG_IR (LOG_VEX | LOG_BIL)

// mask for all log messages
#define LOG_ALL (LOG_MSG | LOG_INSN | LOG_IR)

// mask to disable all log messages
#define LOG_NONE 0


#define LOG_STDERR_DEFAULT LOG_MSG

#define LOG_TO_STDERR
#define LOG_TO_FILE


#ifdef __cplusplus
extern "C" {
#endif

void _panic(const char *msg);


#ifdef __cplusplus

#include <string>

class BapException
{
public:
    
    BapException(string s) : reason(s) {};
    string reason;
};

void panic(string msg);

#endif


uint32_t log_stderr(uint32_t mask);

int log_init(uint32_t mask, const char *path);
void log_close(void);

void log_write(uint32_t level, const char *msg, ...);
size_t log_write_bytes(uint32_t level, const char *msg, size_t len);

#ifdef __cplusplus
}
#endif

#endif
