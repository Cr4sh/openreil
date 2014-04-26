#include "asm_program.h"
#include <errno.h>
#include <string.h>

/* http://sourceware.org/binutils/docs/bfd/Symbols.html#Symbols
 *
 * Also see objdump.c from binutils -ed
 */


static void make_assertions()
{
  // Assertions needed for the camlidl interface
}

/* Filter out (in place) symbols that are useless for disassembly.
 *    COUNT is the number of elements in SYMBOLS.
 *       Return the number of useful symbols.  */

/*
 * Taken from objdump.c
 */

long
remove_useless_symbols_local (asymbol *symbols, long count)
{
  asymbol *in_ptr = symbols, *out_ptr = symbols;

  while (--count >= 0)
  {
    asymbol *sym = in_ptr++;

    if (sym->name == NULL || sym->name[0] == '\0')
      continue;
    if (sym->flags & (BSF_DEBUGGING | BSF_SECTION_SYM))
      continue;
    if (bfd_is_und_section (sym->section)
        || bfd_is_com_section (sym->section))
      continue;

    *out_ptr++ = *sym;
  }
  return out_ptr - symbols;
}

/** Returns synthetic and real symbols.  Synthetic symbols include
 * entries in the PLT. */
asymbol ** asmir_get_all_symbols(asm_program_t *prog, long *num)
{
  static int do_asserts = 1;
  long storage_needed, res, numsyms, numdynsyms, numsynthsyms;
  long i;
  asymbol *synth_symbol_table;
  asymbol **out_ptr;
  asymbol **synth_symbol_table_final;
  asymbol **symbol_table;
  asymbol **dynsymbol_table;
  
  if (do_asserts) {
    make_assertions();
    do_asserts = 0;
  }
  
  storage_needed = bfd_get_symtab_upper_bound(prog->abfd);
  if (storage_needed <= 0) goto fail;

  symbol_table = bfd_alloc(prog->abfd, storage_needed);
  if (!symbol_table) goto fail;

  numsyms = bfd_canonicalize_symtab(prog->abfd, symbol_table);
  if (numsyms < 0) goto fail;

  storage_needed = bfd_get_dynamic_symtab_upper_bound(prog->abfd);
  if (storage_needed <= 0) goto fail;

  dynsymbol_table = bfd_alloc(prog->abfd, storage_needed);
  if (!dynsymbol_table) goto fail;

  numdynsyms = bfd_canonicalize_dynamic_symtab(prog->abfd, dynsymbol_table);
  if (numdynsyms < 0) goto fail;

  numsynthsyms = bfd_get_synthetic_symtab(prog->abfd, numsyms, symbol_table, numdynsyms, dynsymbol_table, &synth_symbol_table);
  if (numsynthsyms < 0) goto fail;

  numsynthsyms = remove_useless_symbols_local(synth_symbol_table, numsynthsyms);

  /* Merge synth. symbols with regular ones */
  storage_needed = (numsynthsyms + numsyms) * sizeof(asymbol*);

  synth_symbol_table_final = bfd_alloc(prog->abfd, storage_needed);
  if (!synth_symbol_table_final) goto fail;

  /* Copy over all of the normal syms */
  memcpy(synth_symbol_table_final, symbol_table, numsyms * sizeof(asymbol*));

  /* Point after the stuff we just copied */
  out_ptr = synth_symbol_table_final + numsyms;
  
  for (i = 0; i < numsynthsyms; i++) {
    *(out_ptr++) = &(synth_symbol_table[i]);
  }
  
  *num = numsynthsyms + numsyms;
  return synth_symbol_table_final;

  fail:
  *num = 0;
  return NULL;

}

asymbol ** asmir_get_symbols(asm_program_t *prog, long *num)
{
  static int do_asserts = 1;
  long storage_needed, res;
  asymbol **symbol_table;

  if (do_asserts) {
    make_assertions();
    do_asserts = 0;
  }

  storage_needed = bfd_get_symtab_upper_bound(prog->abfd);
  if (storage_needed <= 0) goto fail;

  symbol_table = bfd_alloc(prog->abfd, storage_needed);
  if (!symbol_table) goto fail;

  res = bfd_canonicalize_symtab(prog->abfd, symbol_table);
  if (res <= 0) goto fail;

  *num = res;
  return symbol_table;

  fail:
  *num = 0;
  return NULL;

}
