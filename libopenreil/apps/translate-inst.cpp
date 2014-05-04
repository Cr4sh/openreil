#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>

#include "libopenreil.h"

int reil_inst_handler(reil_inst_t *inst, void *context)
{
    // print instruction representation to the stdout
    reil_inst_print(inst);
    return 0;
}

//======================================================================
//
// Main
//
//======================================================================

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("USAGE: tarnslate-inst arch hex_bytes\n");
        return 0;
    }

    reil_arch_t arch;
    int inst_len = 0;
    uint8_t inst[MAX_INST_LEN];
    memset(inst, 0, sizeof(inst));

    if (!strcmp(argv[1], "-x86"))
    {
        // set target architecture
        arch = REIL_X86;
    }
    else
    {
        printf("ERROR: Invalid architecture\n");
        return -1;
    }

    for (int i = 2; i < argc; i++)
    {
        // get user specified bytes to dissassembly
        inst[i - 2] = strtol(argv[i], NULL, 16);
        inst_len += 1;

        if (errno == EINVAL)
        {
            printf("ERROR: Invalid hex byte\n");
            return -1;
        }
    }

    // translate single instruction
    void *reil = reil_init(arch, reil_inst_handler, NULL);
    if (reil)
    {
        reil_translate(reil, 0, inst, inst_len);
        reil_close(reil);
    }

    return 0;
}

