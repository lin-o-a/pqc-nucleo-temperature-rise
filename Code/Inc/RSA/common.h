#ifndef MBEDTLS_COMMON_H
#define MBEDTLS_COMMON_H

#define MBEDTLS_BIGNUM_C
#define MBEDTLS_RSA_C
#define MBEDTLS_ECP_WITH_MPI_UINT

/* Target architecture setting for 32-bit ARM Cortex-M33 */
#define MBEDTLS_HAVE_INT32
#undef MBEDTLS_HAVE_INT64

#endif
