#include <stddef.h>

// Global, non-static definition to permanently satisfy the linker
unsigned int mbedtls_ct_bool(unsigned int condition) {
    return (condition != 0) ? 1 : 0;
}
