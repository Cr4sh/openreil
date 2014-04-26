/* This was a hack to get rodata in VinE. The data structure and API
   to this is dumb.
 */

#include <errno.h>
#include <stdlib.h>
#include <assert.h>

#include "asm_program.h"


  // Structure for rodata
typedef struct memory_cell_data {
address_t address;
int value; // suppose to have 256 possible byte values
} memory_cell_data_t;


extern void destroy_memory_data(memory_data_t *md) {
    if (md) {
        for (vector<memory_cell_data_t*>::iterator j = md->begin();
             j != md->end(); j++) {
            free(*j);
        }
        delete md;
    }
}

address_t memory_cell_data_address(memory_cell_data_t *md) {
    return md->address;
}

int memory_cell_data_value(memory_cell_data_t *md) {
    return md->value;
}

int memory_data_size(memory_data_t *md) {
    return md->size();
}

memory_cell_data_t * memory_data_get(memory_data_t *md, int i) {
    return md->at(i);
}

/**
 * Obtain data from the section with readonly flags.
 * I.e. ALLOC, READONLY, and LOAD flags are set.
 * FIXME: Use a reasonable data structure.
 */
memory_data_t *
get_rodata(asm_program_t *prog) {
    vector<memory_cell_data_t *> *rodata = new vector<memory_cell_data_t *>();

    bfd *abfd = prog->abfd;

    unsigned int opb = bfd_octets_per_byte(abfd);
    assert(opb == 1);

    for(asection *section = abfd->sections;
        section != (asection *)  NULL; section = section->next){

        if(!((section->flags & SEC_READONLY) &&
             (section->flags & SEC_ALLOC) &&
             (section->flags & SEC_LOAD))
            ) continue;
        bfd_byte *data = NULL;
        bfd_vma datasize = bfd_get_section_size_before_reloc(section);
        if(datasize == 0) continue;
        data = (bfd_byte *) calloc((size_t) datasize, (size_t) sizeof(bfd_byte));
        if (data == NULL) {
            perror("memory allocation failed\n");
            exit(EXIT_FAILURE);
        }
        bfd_get_section_contents(abfd, section, data, 0, datasize);
        bfd_vma start_addr = section->vma;
        bfd_vma end_addr = start_addr + datasize/opb;
        for (bfd_vma itr = start_addr; itr < end_addr; itr++) {
            memory_cell_data_t *mcd = (memory_cell_data_t *)
                calloc((size_t) 1, (size_t) sizeof(memory_cell_data_t));
            if (mcd == NULL) {
                perror("memory allocation failed\n");
                exit(EXIT_FAILURE);
            }
            mcd->address = itr;
            mcd->value = data[itr-start_addr];
            rodata->push_back(mcd);
        }
        free(data);
    }
    /* FIXMEO: close the BFD */
    return rodata;
}
