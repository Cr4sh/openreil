/** $Id: llvm.cpp 5968 2012-04-05 00:33:39Z swhitman $
 *
 *  Helper functions for llvm code generation
 *
 *  XXX: Add support for multibyte reads and writes
*/

#include <cassert>
#include <map>
#include <stdio.h>
#include <stdint.h>

typedef uint64_t addr_t;
typedef uint8_t value_t;
typedef std::map<addr_t,value_t> memory;

static memory m;

extern "C" {

  void fake_assert(uint32_t b) {
    assert(b);
  }

  memory create_memory(void) {
    m.clear();
  }

  void set_memory(addr_t a, value_t v) {
    //    printf("Writing %d to %Lx\n", v, a);
    m[a] = v;
  }

  void set_memory_multi(addr_t a, value_t *v, uint32_t nbytes) {
    /* Sanity check */
    assert (nbytes <= 32);

    for (uint32_t i = 0; i < nbytes; i++) {
      set_memory(a+i, v[i]);
    }
  }

  value_t get_memory(addr_t a) {
    /* Sets memory if undefined. This may not be what we want.q */
    //    printf("Returning %d from %Lx\n", m[a], a);
    return m[a];
  }

  void get_memory_multi(addr_t a, value_t *vout, uint32_t nbytes) {
    /* Sanity check */
    assert (nbytes <= 32);

    for (uint32_t i = 0; i < nbytes; i++) {
      vout[i] = get_memory(a+i);
    }
  }
}
