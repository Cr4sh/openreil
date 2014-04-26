#ifndef _PIN_COMMON_H
#define _PIN_COMMON_H

// Size of temporary buffers
#define BUFSIZE 128

#define MAX_INSN_BYTES 15

#define MAX_VALUES_COUNT 0x300

#define MAX_CACHEMASK_BTYES 2

// TODO: Make these values something proper.
#define MAX_FRAME_MEMSIZE sizeof(StdFrame)
#define MAX_FRAME_DISKSIZE 1024

  /** Total syscall args */
#define MAX_SYSCALL_ARGS 9
  /** Number to record on this platform */
#ifdef _WIN32
#define PLAT_SYSCALL_ARGS MAX_SYSCALL_ARGS
#else
#define PLAT_SYSCALL_ARGS 5
#endif



#endif
