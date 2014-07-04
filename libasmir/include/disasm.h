#ifndef _DISASM_H
#define _DISASM_H

#ifdef __cplusplus
extern "C" {
#endif

int disasm_insn(VexArch guest, uint8_t *data, string &op_str);

int disasm_arg_src(VexArch guest, uint8_t *data, vector<Temp *> &args);
int disasm_arg_dst(VexArch guest, uint8_t *data, vector<Temp *> &args);

#ifdef __cplusplus
}
#endif

#endif
