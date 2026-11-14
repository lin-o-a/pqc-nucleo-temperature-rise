#ifndef MBEDTLS_PLATFORM_UTIL_H
#define MBEDTLS_PLATFORM_UTIL_H
#include <stddef.h>
#include <string.h>

static inline void mbedtls_platform_zeroize(void *buf, size_t len) {
    volatile unsigned char *p = (volatile unsigned char *) buf;
    while (len--) {
        *p++ = 0;
    }
}
#define mbedtls_zeroize_and_free(v, n) mbedtls_platform_zeroize(v, n)

#endif
