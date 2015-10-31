
#ifndef LIBOPENREIL_H
#define LIBOPENREIL_H

// IR format definitions
#include "reil_ir.h"


// max. size of one machine instruction (all arhitectures)
#define MAX_INST_LEN 24

// return value that indicates initialization/translation error
#define REIL_ERROR -1

// enable under construction features
#define TESTING


/*
    In ARCH_ARM mode least significant bit of instruction address
    indicates Thumb mode enabled.
*/
#define REIL_ARM_THUMB(_addr_) ((_addr_) | 1)

/*
    Debug information type constants for mask argument of reil_log_init()
*/
#define REIL_LOG_INFO   0x00000001  // regular message
#define REIL_LOG_WARN   0x00000002  // error
#define REIL_LOG_ERR    0x00000004  // warning
#define REIL_LOG_BIN    0x00000008  // instruction bytes
#define REIL_LOG_ASM    0x00000010  // instruction assembly code
#define REIL_LOG_VEX    0x00000020  // instruction VEX code
#define REIL_LOG_BIL    0x00000040  // instruction BAP IL code

// all log messages
#define REIL_LOG_ALL 0x7FFFFFFF

// disable log messages
#define REIL_LOG_NONE 0

// by default we prints warnings, errors and info to stderr
#define REIL_LOG_DEFAULT (LOG_INFO | LOG_WARN | LOG_ERR)


typedef void * reil_t;

typedef enum _reil_arch_t 
{ 
    ARCH_X86, 
    ARCH_ARM 

} reil_arch_t;


#ifdef __cplusplus
extern "C" {
#endif

/*
    Prints REIL instruction to console.
*/
void reil_inst_print(reil_inst_t *inst);


typedef int (* reil_inst_handler_t)(reil_inst_t *inst, void *context);

/*
    Initialize REIL translator.
*/
reil_t reil_init(reil_arch_t arch, reil_inst_handler_t handler, void *context);

/*
    Close translator.
*/
void reil_close(reil_t reil);

/*
    Initialize debug log. 
    * Individual bits of mask argument tells what kind of information
    we need to include into the log (see REIL_LOG_* constants).
    * Log file path can be specified in path argument, NULL value 
    indicates that we need to print debug information only to stderr.
*/
int reil_log_init(uint32_t mask, const char *path);

/*
    Closes currently opened log file.
*/
void reil_log_close(void);

int reil_translate(reil_t reil, reil_addr_t addr, unsigned char *buff, int len);
int reil_translate_insn(reil_t reil, reil_addr_t addr, unsigned char *buff, int len);

#ifdef __cplusplus
}
#endif

#endif // LIBOPENREIL_H
