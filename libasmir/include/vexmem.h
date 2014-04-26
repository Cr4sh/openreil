/*
 Owned and copyright BitBlaze, 2007. All rights reserved.
 Do not copy, disclose, or distribute without explicit written
 permission.
*/
//======================================================================
//
// Vexmem is a memory management module created for Vexir. See Notes
// for more details.
//
//======================================================================

#ifndef __VEXMEM_H
#define __VEXMEM_H


#ifdef __cplusplus
extern "C"
{
#endif

#include "libvex.h"

void *vx_Alloc( Int nbytes );
void vx_FreeAll();
IRSB* vx_dopyIRSB ( IRSB* bb );

#ifdef __cplusplus
}
#endif

#endif

