
#ifndef LIBOPENREIL_H
#define LIBOPENREIL_H

// IR format definitions
#include "reil_ir.h"

#define MAX_INST_LEN 30

#define REIL_ERROR -1

typedef void * reil_t;
typedef enum _reil_arch_t { ARCH_X86 } reil_arch_t;
typedef int (* reil_inst_handler_t)(reil_inst_t *inst, void *context);

#ifdef __cplusplus
extern "C" {
#endif

void reil_inst_print(reil_inst_t *inst);

reil_t reil_init(reil_arch_t arch, reil_inst_handler_t handler, void *context);
void reil_close(reil_t reil);

int reil_translate(reil_t reil, reil_addr_t addr, unsigned char *buff, int len);
int reil_translate_insn(reil_t reil, reil_addr_t addr, unsigned char *buff, int len);

#ifdef __cplusplus
}
#endif

#endif // LIBOPENREIL_H
