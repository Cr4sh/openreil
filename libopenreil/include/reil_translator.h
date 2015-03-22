
#ifndef REIL_TRANSLATOR_H
#define REIL_TRANSLATOR_H

#define EXPAND_EFLAGS

#define MAX_REG_NAME_LEN 20

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

    void reset_state(bap_block_t *block);    

    void process_bil_stmt(Stmt *s, uint64_t inst_flags);
    void process_bil(reil_raw_t *raw_info, bap_block_t *block);

private:        
    
    int32_t tempreg_find(string name);
    int32_t tempreg_alloc(void);
    string tempreg_get_name(int32_t tempreg_num);
    string tempreg_get(string name);
    
    uint64_t convert_special(Special *special);
    reg_t convert_operand_size(reil_size_t size);
    reil_size_t convert_operand_size(reg_t typ);
    void convert_operand(Exp *exp, reil_arg_t *reil_arg);    

    Exp *temp_operand(reg_t typ, reil_inum_t inum);

    bool is_unknown_insn(bap_block_t *block);
    void process_unknown_insn(void);
    void process_empty_insn(void);

    void process_reil_inst(reil_inst_t *reil_inst);

    bool get_bil_label(string name, reil_addr_t *addr);
    Stmt *get_bil_stmt(int pos);

    void check_cjmp_false_target(Exp *target);
    
    void process_bil_arshift(reil_inst_t *reil_inst);
    void process_bil_neq(reil_inst_t *reil_inst);
    void process_bil_le(reil_inst_t *reil_inst);
    bool process_bil_cast(Exp *exp, reil_inst_t *reil_inst);

    void free_bil_exp(Exp *exp);
    Exp *process_bil_exp(Exp *exp);    
    
    Exp *process_bil_inst(reil_op_t inst, uint64_t inst_flags, Exp *c, Exp *exp);

    VexArch guest;

    bap_block_t *current_block;
    int current_stmt;

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

#endif // REIL_TRANSLATOR_H
