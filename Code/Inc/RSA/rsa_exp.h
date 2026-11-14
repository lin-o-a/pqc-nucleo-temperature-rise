#ifndef RSA_EXP_H
#define RSA_EXP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "RSA/bignum_core.h"
#include "RSA/bignum_mod.h"

#define RSA_2048_LIMBS 64
#define RSA_1024_LIMBS 32

/**
 * @brief Full RSA-2048 CRT Decryption routine
 * @param output_plaintext Pointer to store the resulting decrypted limbs (2048 bits / 64 limbs)
 * @param input_ciphertext Pointer to the input ciphertext limbs
 * @return 0 on success, negative error code on failure
 */
int rsa_decrypt(mbedtls_mpi_uint *output_plaintext, const mbedtls_mpi_uint *input_ciphertext);

/**
 * @brief Standalone correctness verification returning true/false
 */
bool verify_rsa_crt_correctness(void);

/**
 * @brief Continuous stress test loop for oscilloscope current / thermal profiling
 */
void RSA_Decryption(void);

#endif /* RSA_EXP_H */
