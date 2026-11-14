#ifndef MBEDTLS_PLATFORM_H
#define MBEDTLS_PLATFORM_H

#include <stdlib.h>
#include <stdio.h>

#define mbedtls_calloc    calloc
#define mbedtls_free      free
#define mbedtls_printf    printf

typedef int (*mbedtls_f_rng_t)(void *, unsigned char *, size_t);

#endif
