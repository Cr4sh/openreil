#ifndef IRTOIR_I386_H
#define IRTOIR_I386_H

bool i386_op_is_very_broken(string mnemonic);

void set_eflags_bits(vector<Stmt *> *irout, Exp *CF, Exp *PF, Exp *AF, Exp *ZF, Exp *SF, Exp *OF);

#endif
