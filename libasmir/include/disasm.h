#ifndef DISASM_H
#define DISASM_H

#define DISASM_MAX_INST_LEN 24

#define IS_ARM_THUMB(_addr_) ((_addr_) & 1)

#ifdef __cplusplus

extern "C" {

int disasm_insn(VexArch guest, uint8_t *data, address_t addr, string &mnemonic, string &op);

int disasm_arg_src(VexArch guest, uint8_t *data, address_t addr, vector<Temp *> &args);
int disasm_arg_dst(VexArch guest, uint8_t *data, address_t addr, vector<Temp *> &args);

}

#endif
#endif
