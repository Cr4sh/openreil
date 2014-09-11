
#ifndef _REIL_TRANSLATOR_H_
#define _REIL_TRANSLATOR_H_

string to_string_constant(reil_const_t val, reil_size_t size);
string to_string_size(reil_size_t size);
string to_string_operand(reil_arg_t *a);
string to_string_inst_code(reil_op_t inst_code);

typedef pair<int32_t, string> TEMPREG_BAP;

class CReilTranslatorException
{
public:
    
    CReilTranslatorException(string s) : reason(s) {};
    string reason;
};

class CReilFromBilTranslator
{
public:
    
    CReilFromBilTranslator(VexArch arch, reil_inst_handler_t handler, void *context); 
    ~CReilFromBilTranslator();

    void reset_state();    

    void process_bil(reil_raw_t *raw_info, uint64_t inst_flags, Stmt *s);
    void process_bil(reil_raw_t *raw_info, bap_block_t *block);

private:

    bool is_unknown_insn(bap_block_t *block);
    void process_unknown_insn(reil_raw_t *raw_info);

    string tempreg_get_name(int32_t tempreg_num);
    int32_t tempreg_bap_find(string name);
    int32_t tempreg_alloc(void);
    
    uint64_t convert_special(Special *special);
    reil_size_t convert_operand_size(reg_t typ);
    void convert_operand(Exp *exp, reil_arg_t *reil_arg);    

    void process_reil_inst(reil_inst_t *reil_inst);

    void free_bil_exp(Exp *exp);
    Exp *process_bil_exp(Exp *exp);    
    Exp *process_bil_inst(reil_op_t inst, uint64_t inst_flags, Exp *c, Exp *exp);

    VexArch guest;
    vector<TEMPREG_BAP> tempreg_bap;
    int32_t tempreg_count;
    reil_inum_t inst_count;
    reil_raw_t *current_raw_info;
    bool skip_eflags;

    reil_inst_handler_t inst_handler;
    void *inst_handler_context;
};

class CReilTranslator
{
public:

    CReilTranslator(VexArch arch, reil_inst_handler_t handler, void *context);
    ~CReilTranslator();

    int process_inst(address_t addr, uint8_t *data, int size);

private:

    VexArch guest;
    CReilFromBilTranslator *translator;
};

#endif

