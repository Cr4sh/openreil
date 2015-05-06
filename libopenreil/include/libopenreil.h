
#ifndef LIBOPENREIL_H
#define LIBOPENREIL_H

// IR format definitions
#include "reil_ir.h"


// max. size of one machine instruction (all arhitectures)
#define MAX_INST_LEN 24

// return value that indicates initialization/translation error
#define REIL_ERROR -1


typedef void * reil_t;

typedef enum _reil_arch_t 
{ 
    ARCH_X86, 
    ARCH_ARM 

} reil_arch_t;


#ifdef __cplusplus
extern "C" {
#endif

void reil_inst_print(reil_inst_t *inst);


typedef int (* reil_inst_handler_t)(reil_inst_t *inst, void *context);

reil_t reil_init(reil_arch_t arch, reil_inst_handler_t handler, void *context);
void reil_close(reil_t reil);

int reil_log_init(uint32_t mask, const char *path);
void reil_log_close(void);

int reil_translate(reil_t reil, reil_addr_t addr, unsigned char *buff, int len);
int reil_translate_insn(reil_t reil, reil_addr_t addr, unsigned char *buff, int len);

#ifdef __cplusplus
}
#endif

#endif // LIBOPENREIL_H
