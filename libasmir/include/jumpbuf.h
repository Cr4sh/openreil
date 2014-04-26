#ifndef __JUMPBUF_H
#define __JUMPBUF_H

#include <setjmp.h>

/* Jump buffer used to return from VEX errors */
extern jmp_buf vex_error;
extern char jmp_buf_set;

#endif
