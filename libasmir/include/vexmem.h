#ifndef VEXMEM_H
#define VEXMEM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "libvex.h"

void *vx_Alloc(Int nbytes);
void vx_FreeAll();

IRSB* vx_dopyIRSB(IRSB* bb);

#ifdef __cplusplus
}
#endif

#endif
