#ifndef DISASM_H
#define DISASM_H

#define DISASM_MAX_INST_LEN 24

#ifdef __cplusplus
extern "C" {
#endif

int disasm_insn(VexArch guest, uint8_t *data, string &mnemonic, string &op);

int disasm_arg_src(VexArch guest, uint8_t *data, vector<Temp *> &args);
int disasm_arg_dst(VexArch guest, uint8_t *data, vector<Temp *> &args);

#ifdef __cplusplus
}
#endif

#endif
