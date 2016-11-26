#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>

#include "libopenreil.h"

#define LOG_NAME "translate-inst.log"

int reil_inst_handler(reil_inst_t *inst, void *context)
{
    // print instruction representation to the stdout
    reil_inst_print(inst);

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("USAGE: tarnslate-inst arch hex_bytes\n");
        return 0;
    }    

    reil_arch_t arch;
    char *arch_name = argv[1];

    int inst_len = 0;
    uint8_t inst[MAX_INST_LEN];    
    reil_addr_t addr = 0;

    memset(inst, 0, sizeof(inst));

    if (!strcmp(arch_name, "i386") || !strcmp(arch_name, "x86"))
    {
        printf("[+] Using i386 architecture\n");

        // set target architecture
        arch = ARCH_X86;        
    }
    else if (!strcmp(arch_name, "arm"))
    {
        printf("[+] Using ARM architecture\n");

        // set target architecture
        arch = ARCH_ARM;
    }
    else
    {
        printf("ERROR: Bad architecture\n");
        return -1;
    }

    for (int i = 2; i < argc; i++)
    {
        char *arg = argv[i];

        // get additional command line options
        if (inst_len == 0)
        {
            if (!strcmp(arg, "--debug") || !strcmp(arg, "-d"))
            {
                printf("[+] Log name is %s\n", LOG_NAME);

                // enable extra debug output
                reil_log_init(REIL_LOG_ALL, LOG_NAME);
                continue;
            }
            else if (!strcmp(arg, "--thumb") || !strcmp(arg, "-t"))
            {
                printf("[+] Disassembling in Thumb mode\n");

                // enable thumb mode
                addr = REIL_ARM_THUMB(addr);
                continue;
            }
        }

        errno = 0;

        // get user specified bytes to dissassembly
        inst[inst_len] = strtol(argv[i], NULL, 16);
        inst_len += 1;

        if (errno == EINVAL)
        {
            printf("ERROR: Invalid hex value\n");
            return -1;
        }
    }

    // translate single instruction
    void *reil = reil_init(arch, reil_inst_handler, NULL);
    if (reil)
    {
        reil_translate(reil, addr, inst, inst_len);
        reil_close(reil);
    }

    return 0;
}
