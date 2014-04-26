#ifndef _REG_MAPPING_H
#define _REG_MAPPING_H

#include <stmt.h>
#include "pin_frame.h" // for VT_REGx...
#include "reg_mapping_pin.h"

uint32_t regid_to_full(uint32_t id);
string regid_to_fullname(uint32_t id);
reg_t regid_to_type(uint32_t id);
string register_name(uint32_t id);
cval_type_t get_type(uint32_t typ);

Move* write_reg(uint32_t id, Exp* val, int offset=-1);

// returns a 32 bit wide unsigned int,
Exp* read_reg(uint32_t id);

// returns a 32 bit wide unsigned int,
// but only 8 bits of it (in lowest 8 bits of the expression)
// offset 0 is least significant byte
Exp* read_reg(uint32_t id, int offset);

/****************************** Intel-386 Operations ***************************/

/*** Define Registers ***/

/* segment registers */
#define es_reg 100
#define cs_reg 101
#define ss_reg 102
#define ds_reg 103
#define fs_reg 104
#define gs_reg 105

/* address-modifier dependent registers */
#define eAX_reg 108
#define eCX_reg 109
#define eDX_reg 110
#define eBX_reg 111
#define eSP_reg 112
#define eBP_reg 113
#define eSI_reg 114
#define eDI_reg 115

/* 8-bit registers */
#define al_reg 116
#define cl_reg 117
#define dl_reg 118
#define bl_reg 119
#define ah_reg 120
#define ch_reg 121
#define dh_reg 122
#define bh_reg 123

/* 16-bit registers */
#define ax_reg 124
#define cx_reg 125
#define dx_reg 126
#define bx_reg 127
#define sp_reg 128
#define bp_reg 129
#define si_reg 130
#define di_reg 131

/* 32-bit registers */
#define eax_reg 132
#define ecx_reg 133
#define edx_reg 134
#define ebx_reg 135
#define esp_reg 136
#define ebp_reg 137
#define esi_reg 138
#define edi_reg 139


#define indir_dx_reg 150

#endif
