
#ifndef _LIBOPENREIL_H_
#define _LIBOPENREIL_H_

// IR format definitions
#include "reil_ir.h"

//#define DBG_BAP

#define MAX_INST_LEN 30
#define REIL_ERROR -1

typedef void * reil_t;
typedef enum _reil_arch_t { REIL_X86 } reil_arch_t;
typedef int (* reil_inst_handler_t)(reil_inst_t *inst, void *context);

void reil_inst_print(reil_inst_t *inst);

reil_t reil_init(reil_arch_t arch, reil_inst_handler_t handler, void *context);
void reil_close(reil_t reil);

int reil_translate(reil_t reil, reil_addr_t addr, unsigned char *buff, int len);
int reil_translate_insn(reil_t reil, reil_addr_t addr, unsigned char *buff, int len);

#endif
