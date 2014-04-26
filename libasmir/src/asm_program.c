#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>

#include "asm_program.h"

/* This elf stuff is interal to BFD (TOO BAD) */

#define SHT_PROGBITS	1		/* Program specific (private) data */
#define PT_GNU_STACK  0x6474e551

struct elf_internal_phdr {
  unsigned long	p_type;			/* Identifies program segment type */
  unsigned long	p_flags;		/* Segment flags */
  bfd_vma	p_offset;		/* Segment file offset */
  bfd_vma	p_vaddr;		/* Segment virtual address */
  bfd_vma	p_paddr;		/* Segment physical address */
  bfd_vma	p_filesz;		/* Segment size in file */
  bfd_vma	p_memsz;		/* Segment size in memory */
  bfd_vma	p_align;		/* Segment alignment, file & memory */
};

typedef struct elf_internal_phdr Elf_Internal_Phdr;
extern Elf_Internal_Phdr * _bfd_elf_find_segment_containing_section(bfd * abfd, asection * section);

/* End internal BFD elf stuff */

static void initialize_sections(asm_program_t *p, bfd_vma base);
static bfd* initialize_bfd(const char *filename);

/* find the segment for memory address addr */
static section_t* get_section_of(asm_program_t *prog, bfd_vma addr)
{
  assert(prog);
  section_t *segment;

  for (segment = prog->segs; segment != NULL; segment = segment->next)
    if ((addr >= segment->start_addr) && (addr < segment->end_addr))
      return segment;

  return NULL;
}



bfd_byte *asmir_get_ptr_to_instr(asm_program_t *prog, bfd_vma addr)
{
  section_t *seg = get_section_of(prog, addr);
  if(seg == NULL){
    fprintf(stderr, "get_ptr_to_instr: Segment for %llx not found\n", (long long int)addr);
    return NULL;
  }
  return seg->data + (addr - seg->start_addr);
}


asm_program_t *
asmir_open_file(const char *filename, bfd_vma base)
{
  asm_program_t *prog = malloc(sizeof(asm_program_t));
  if (!prog)
    return NULL;
  bfd *abfd = initialize_bfd(filename);
  if (!abfd) {
    free(prog);
    return NULL;
  }
  prog->abfd = abfd;
  initialize_sections(prog, base);

  return prog;
}


void asmir_close(asm_program_t *p)
{
  // FIXME: free sections
  bfd_close_all_done(p->abfd);
  free(p);
  vx_FreeAll();
}

static int ignore() {
  return 1;
}

int asmir_get_instr_length(asm_program_t *prog, bfd_vma addr)
{
  disassembler_ftype disas = disassembler(prog->abfd);
  fprintf_ftype old_fprintf_func = prog->disasm_info.fprintf_func;
  prog->disasm_info.fprintf_func = (fprintf_ftype)ignore;
  assert(disas);
  int len = disas(addr, &prog->disasm_info);
  prog->disasm_info.fprintf_func = old_fprintf_func;
  return len;
}


static int
my_read_memory (bfd_vma memaddr,
		bfd_byte *myaddr,
		unsigned int length,
		struct disassemble_info *info)
{
  int ret = buffer_read_memory(memaddr,myaddr,length,info);

  if (EIO == ret) {
    section_t *seg = get_section_of(info->application_data, memaddr);
    if (NULL == seg)
      return EIO;

    info->buffer = seg->data;
    info->buffer_vma = seg->start_addr;
    info->buffer_length = seg->datasize;
    info->section = seg->section;

    ret = buffer_read_memory(memaddr,myaddr,length,info);
  }
  return ret;
}


static void init_disasm_info2(bfd *abfd, struct disassemble_info *disasm_info)
{
  init_disassemble_info (disasm_info, stdout, (fprintf_ftype) fprintf);
  disasm_info->flavour = bfd_get_flavour (abfd);
  disasm_info->arch = bfd_get_arch (abfd);
  disasm_info->mach = bfd_get_mach (abfd);
  disasm_info->octets_per_byte = bfd_octets_per_byte (abfd);
  disasm_info->disassembler_needs_relocs = FALSE;

  if (bfd_big_endian (abfd))
    disasm_info->display_endian = disasm_info->endian = BFD_ENDIAN_BIG;
  else if (bfd_little_endian (abfd))
    disasm_info->display_endian = disasm_info->endian = BFD_ENDIAN_LITTLE;

  disassemble_init_for_target(disasm_info);

  disasm_info->read_memory_func = my_read_memory;
}

static void init_disasm_info(asm_program_t *prog)
{
  init_disasm_info2(prog->abfd, &prog->disasm_info);
  prog->disasm_info.application_data = prog;
}

/** Trace read memory is a special read memory function.  It simply
 * returns bytes set ahead of time (from the trace). */

static uint8_t trace_instruction_bytes[MAX_INSN_BYTES];
static size_t trace_instruction_size;
static bfd_vma trace_instruction_addr;

void set_trace_bytes(void *bytes, size_t len, bfd_vma addr) {
  memcpy(trace_instruction_bytes, bytes, len);
  trace_instruction_size = len;
  trace_instruction_addr = addr;
}

static int
trace_read_memory (bfd_vma memaddr,
                   bfd_byte *myaddr,
                   unsigned int length,
                   struct disassemble_info *info)
{
  uintptr_t memhelper = memaddr;
  size_t offstart = memaddr - trace_instruction_addr;
  size_t offend = offstart + length;

  if (offstart > trace_instruction_size || offend > trace_instruction_size) {
    fprintf(stderr, "WARNING: Disassembler looking for unknown address. Known base: %#" BFD_VMA_FMT "x Wanted: %#" BFD_VMA_FMT "x Length: %#x\n", trace_instruction_addr, memaddr, length);
  }

  memcpy(myaddr, trace_instruction_bytes+offstart, length);
  return 0;
}

bfd_vma asmir_get_base_address(asm_program_t *prog) {
#if SIZEOF_BFD_VMA == 4
  bfd_vma lowest = LONG_MAX;
#else
  bfd_vma lowest = LLONG_MAX;
#endif
  assert(prog);
  bfd *abfd = prog->abfd;
  asection *section;

  if (bfd_get_flavour(abfd) ==  bfd_target_elf_flavour) {

    /* BFD has some issues with ELF files.  ELF files have both
       sections and segments ("program headers").  We really care
       about the lowest address of program segments, but BFD only
       shows sections when an ELF file contains both. So, we need to
       use ELF-specific functions to learn the segment address, which
       is typically different than the section address. */
      int i, is_nice;
      size_t ubound = bfd_get_elf_phdr_upper_bound(abfd);
      Elf_Internal_Phdr *phdrs = bfd_alloc(abfd, ubound);
      if (phdrs == NULL) { bfd_perror(NULL); assert(0); }
      int n = bfd_get_elf_phdrs(abfd, phdrs);
      if (n == -1) { bfd_perror(NULL); assert(0); }

      for (i = 0; i < n; i++) {
        /*
        fprintf(stderr, "VA: %#" BFD_VMA_FMT "x Align: %#" BFD_VMA_FMT "x Aligned: %#" BFD_VMA_FMT "x p_flags: %#" BFD_VMA_FMT "x p_type: %#"BFD_VMA_FMT "x\n", 
            phdrs[i].p_vaddr, 
            phdrs[i].p_align, 
            phdrs[i].p_vaddr & (~(phdrs[i].p_align)), 
            phdrs[i].p_flags, 
            phdrs[i].p_type);
        */
        bfd_vma aligned = phdrs[i].p_vaddr & (~(phdrs[i].p_align));
        /* STACK segment has vaddr=paddr=0. If we don't ignore it, imagebase=0*/
        is_nice = (phdrs[i].p_flags & SHT_PROGBITS) && 
                (phdrs[i].p_type != PT_GNU_STACK);
        if (is_nice && aligned < lowest) { lowest = aligned; }
      }
    } else {

      /* Non-ELF files */
      for (section = abfd->sections; section; section = section->next) {
        /* fprintf(stderr, "Section %s, vma %Lx\n", section->name, section->vma); */
        if ((section->vma < lowest) && (section->flags & SEC_LOAD)) { lowest = section->vma; }
      }
    }

  //fprintf(stderr, "Lowest section is %#" BFD_VMA_FMT "x\n", lowest);
  //fprintf(stderr, "Adjusting by %Lx\n", offset);

  return lowest;
}

static void
initialize_sections(asm_program_t *prog, bfd_vma base)
{
  bfd_vma offset = 0;
  struct disassemble_info *disasm_info = &prog->disasm_info;
  assert(prog);
  bfd *abfd = prog->abfd;
  unsigned int opb = bfd_octets_per_byte(abfd);
  disasm_info->octets_per_byte = opb;
  init_disasm_info(prog);
  section_t **nextseg = &prog->segs;
  asection *section;

  /* Set to NULL in case there are zero segments. */
  *nextseg = NULL;

  /* Look for the section loaded into the lowest memory address */
  if (base != -1) {

    offset = base - asmir_get_base_address(prog);
    //fprintf(stderr, "Adjusting by %Lx\n", offset);
  }

  for(section = abfd->sections; section; section = section->next) {
    section_t *seg;
    bfd_byte *data;
    bfd_vma datasize = bfd_get_section_size_before_reloc(section);

    if(datasize == 0) continue;

    section->vma += offset;
    data = bfd_alloc2(abfd, datasize, sizeof(bfd_byte));
    bfd_get_section_contents(abfd, section, data, 0, datasize);
    seg = bfd_alloc(abfd, sizeof(section_t));
    seg->data = data;
    seg->datasize = datasize;
    seg->start_addr = section->vma;
    seg->end_addr = section->vma + datasize/opb;
    seg->section = section;
    seg->is_code = !!(section->flags & SEC_CODE);


    seg->next = NULL;
    *nextseg = seg;
    nextseg = &seg->next;
  }

}


static bfd *
initialize_bfd(const char *filename)
{
  bfd * abfd;
  char **matching;
  char *target = "i686-pc-linux-gnu";

  bfd_init();

  if(!bfd_set_default_target(target)) {
    fprintf(stderr, "initialize_bfd: couldn't set default bfd target\n");
    return NULL;
  }

  abfd = bfd_openr(filename, target);
  if(abfd == NULL) {
    fprintf(stderr, "initialize_bfd: cannot open %s\n", filename);
    return NULL;
  }

  if (bfd_check_format (abfd, bfd_archive)) {
    fprintf(stderr, "initalize_bfd: archive files  not supported\n");
    bfd_close_all_done(abfd);
    return NULL;
  }

  /* if(!bfd_check_format_matches(abfd, bfd_object, &matching)) { */
  if( (!bfd_check_format_matches(abfd, bfd_object, &matching)) &&
      (!bfd_check_format_matches(abfd, bfd_core, &matching)) ) {

    fprintf(stderr, "initialize_bfd: bfd_check_format_matches failed\n");
    bfd_close_all_done(abfd);
    return NULL;
  }
  return abfd;
}




struct bprintf_buffer {
  char *str; // the start of the string
  char *end; // the null terminator at the end of the written string.
  size_t size; // the size of str
};

int bprintf(struct bprintf_buffer *dest, const char *fmt, ...) {
  va_list ap;
  int ret;
  size_t size_left = dest->size - (dest->end - dest->str);
  va_start(ap, fmt);
  ret = vsnprintf(dest->end, size_left, fmt, ap);
  va_end(ap);
  if (ret >= size_left) {
    // we seem to need to call va_start again... is this legal?
    dest->size = dest->size+ret+1-size_left;
    char *str = (char*)realloc(dest->str, dest->size);

    assert(str); // this code is full of xalloc anyways...

    dest->end = str + (dest->end - dest->str);
    dest->str = str;
    size_left = dest->size - (dest->end - dest->str);
    va_start(ap, fmt);
    ret = vsnprintf(dest->end, size_left, fmt, ap);
    va_end(ap);
    assert(ret == size_left-1 && ret > 0);
  }
  dest->end += ret;
  return ret;
}

// returns a string which is valid until the next call to this function
char* asmir_string_of_insn(asm_program_t *prog, bfd_vma inst)
{
  static struct bprintf_buffer bits = {NULL, NULL, 0};

  disassembler_ftype disas = disassembler(prog->abfd);
  fprintf_ftype old_fprintf_func = prog->disasm_info.fprintf_func;
  void *oldstream = prog->disasm_info.stream;
  prog->disasm_info.fprintf_func = (fprintf_ftype)bprintf;
  prog->disasm_info.stream = &bits;

  bits.end = bits.str;
  
  disas(inst, &prog->disasm_info);

  prog->disasm_info.fprintf_func = old_fprintf_func;
  prog->disasm_info.stream = oldstream;
  
  return bits.str;
}


enum bfd_architecture asmir_get_asmp_arch(asm_program_t *prog) {
  return bfd_get_arch(prog->abfd);
}


// from old translate.cpp fake_prog_for_arch()
// Returns a fake asm_program_t for use when disassembling bits out of memory
asm_program_t* asmir_new_asmp_for_arch(enum bfd_architecture arch)
{
  
  int machine = 0; // TODO: pick based on arch
  asm_program_t *prog = malloc(sizeof(asm_program_t));
  assert(prog);
  
  prog->abfd = bfd_openw("/dev/null", NULL);
  if (!prog->abfd) {
    bfd_perror("Unable to open fake bfd");
  }
  
  assert(prog->abfd);
  bfd_set_arch_info(prog->abfd, bfd_lookup_arch(arch, machine));

  //not in .h bfd_default_set_arch_mach(prog->abfd, arch, machine);
  bfd_set_arch_info(prog->abfd, bfd_lookup_arch(arch, machine));
  init_disasm_info(prog);

  // Use a special read memory function
  prog->disasm_info.read_memory_func = my_read_memory;

  return prog;
}

/* Uses trace_read_memory, which assumes set_trace_bytes is used to update for each instruction. */
asm_program_t* asmir_trace_asmp_for_arch(enum bfd_architecture arch)
{
  
  int machine = 0; // TODO: pick based on arch
  asm_program_t *prog = malloc(sizeof(asm_program_t));
  assert(prog);
  
  prog->abfd = bfd_openw("/dev/null", NULL);
  if (!prog->abfd) {
    bfd_perror("Unable to open fake bfd");
  }
  
  assert(prog->abfd);
  bfd_set_arch_info(prog->abfd, bfd_lookup_arch(arch, machine));

  //not in .h bfd_default_set_arch_mach(prog->abfd, arch, machine);
  bfd_set_arch_info(prog->abfd, bfd_lookup_arch(arch, machine));
  init_disasm_info(prog);

  // Use a special read memory function
  prog->disasm_info.read_memory_func = trace_read_memory;

  return prog;
}

/*
 * Get the starting memory address of the section named sectionname.
 */  
bfd_vma asmir_get_sec_startaddr(asm_program_t *prog, const char *sectionname) {
  section_t* section;
  asection* asection;

  assert(prog);
  assert(sectionname);

  section = prog->segs;

  /* Traverse the linked list of sections */
  while (section != NULL) {
    asection = section->section; /* Get the BFD section struct */
    if (strcmp(asection->name, sectionname) == 0) {
      /* Got it */
      return section->start_addr;
    }

    section = section->next;
  }

  /* We didn't find it.
   *
   * I doubt the start or end address will be -1, so, this will
   * hopefully not cause any problems.
   */
  return -1;
}

/*
 * Get the ending memory address of the section named sectionname.
 */  
bfd_vma asmir_get_sec_endaddr(asm_program_t *prog, const char *sectionname) {
  section_t* section;
  asection* asection;

  assert(prog);
  assert(sectionname);

  section = prog->segs;

  /* Traverse the linked list of sections */
  while (section != NULL) {
    asection = section->section; /* Get the BFD section struct */
    if (strcmp(asection->name, sectionname) == 0) {
      /* Got it */
      return section->end_addr;
    }

    section = section->next;
  }

  /* We didn't find it.
   *
   * I doubt the start or end address will be -1, so, this will
   * hopefully not cause any problems.
   */
  return -1;
}

/*
 * Get the start address (entry point) of the program.
 */
bfd_vma asmir_get_start_addr(asm_program_t *prog) {
    return prog->abfd->start_address;
}

/*
 * Return a list of all sections
 *
 * XXX: Does not free memory
 */  
asection** asmir_get_all_sections(asm_program_t *prog, long *out) {
  section_t* section;
  /* asection* asection; */
  asection** sectionarray;
  asection** sectionptr;
  *out = 0;
  
  assert(prog);
  assert(out);
  
  section = prog->segs;

  /* Traverse the linked list of sections to count how many there are */
  while (section != NULL) {
    (*out)++;
    section = section->next;
  }

  /* Now, allocate space for an array of pointers */
  sectionarray = bfd_alloc(prog->abfd, (*out * sizeof(asection*)));
  if (sectionarray == 0) goto error;  

  /* Traverse the linked list again, and add the ptrs to each section
   * to the array. */
  section = prog->segs;
  sectionptr = sectionarray;
  while (section != NULL) {
    *(sectionptr++) = section->section;
    section = section->next;
  }

  return sectionarray;
  
  error:

  *out = 0;
  return NULL;
}

