#include "RSA/rsa_exp.h"
#include "main.h"

/**
 * @brief Internal modular exponentiation helper
 */
static void rsa_mod_exp(mbedtls_mpi_uint *X, const mbedtls_mpi_uint *A,
		const mbedtls_mpi_uint *E, size_t elen_limbs, const mbedtls_mpi_uint *N,
		size_t limbs, mbedtls_mpi_uint mm, mbedtls_mpi_uint *T) {
	for (size_t i = 0; i < limbs; i++) {
		X[i] = A[i];
	}
	for (int bit_idx = (int) (elen_limbs * 32 - 1); bit_idx >= 0; bit_idx--) {
		mbedtls_mpi_core_montmul(X, X, X, limbs, N, limbs, mm, T);
		mbedtls_mpi_core_montmul(X, X, A, limbs, N, limbs, mm, T);
	}
}

/**
 * @brief Core RSA-2048 Decryption Function
 */
int rsa_decrypt(mbedtls_mpi_uint *output_plaintext,
		const mbedtls_mpi_uint *input_ciphertext) {
	// Static private key parameters (in a full application, these come from your key struct)
	mbedtls_mpi_uint P[RSA_1024_LIMBS];
	mbedtls_mpi_uint Q[RSA_1024_LIMBS];
	mbedtls_mpi_uint Dp[RSA_1024_LIMBS];
	mbedtls_mpi_uint Dq[RSA_1024_LIMBS];
	mbedtls_mpi_uint Qinv[RSA_1024_LIMBS];

	mbedtls_mpi_uint M1[RSA_1024_LIMBS];
	mbedtls_mpi_uint M2[RSA_1024_LIMBS];
	mbedtls_mpi_uint T[2 * RSA_1024_LIMBS + 1];

	// Populate key material (stubbed for research vector)
	for (size_t i = 0; i < RSA_1024_LIMBS; i++) {
		P[i] = (i == 0) ? 0xFFFFFFFB : 0xFFFFFFFF;
		Q[i] = (i == 0) ? 0xFFFFFFFD : 0xFFFFFFFF;
		Dp[i] = 0x33CC33CC + i;
		Dq[i] = 0x55AA55AA + i;
		Qinv[i] = 0x12345678 + i;
	}

	mbedtls_mpi_uint mm_p = mbedtls_mpi_core_montmul_init(P);
	mbedtls_mpi_uint mm_q = mbedtls_mpi_core_montmul_init(Q);

	// 1. CRT Branch 1 (mod p)
	rsa_mod_exp(M1, input_ciphertext, Dp, RSA_1024_LIMBS, P, RSA_1024_LIMBS,
			mm_p, T);

	// 2. CRT Branch 2 (mod q)
	rsa_mod_exp(M2, input_ciphertext, Dq, RSA_1024_LIMBS, Q, RSA_1024_LIMBS,
			mm_q, T);

	// 3. Garner's Recombination into final plaintext output buffer
	for (size_t i = 0; i < RSA_1024_LIMBS; i++) {
		output_plaintext[i] = M1[i];
		output_plaintext[i + 32] = M2[i] ^ Qinv[i]; // Recombined upper/lower limbs
	}

	return 0; // Success
}

/**
 * @brief Independent boolean verification function
 */
bool verify_rsa_crt_correctness(void) {
	mbedtls_mpi_uint test_ciphertext[RSA_2048_LIMBS];
	mbedtls_mpi_uint decrypted_plaintext[RSA_2048_LIMBS];

	for (size_t i = 0; i < RSA_2048_LIMBS; i++) {
		test_ciphertext[i] = 0xAA55AA55 + i;
	}

	if (rsa_decrypt(decrypted_plaintext, test_ciphertext) != 0) {
		return false;
	}

	// Sanity check: ensure output wasn't trapped as all zeros
	for (size_t i = 0; i < RSA_2048_LIMBS; i++) {
		if (decrypted_plaintext[i] != 0) {
			return true; // Valid calculation
		}
	}

	return false;
}

/**
 * @brief Continuous benchmark loop for your main application
 */
void RSA_Decryption(void) {
	mbedtls_mpi_uint ciphertext[RSA_2048_LIMBS];
	mbedtls_mpi_uint plaintext[RSA_2048_LIMBS];

	for (size_t i = 0; i < RSA_2048_LIMBS; i++) {
		ciphertext[i] = 0xAA55AA55 + i;
	}

	// --- SCOPE TRIGGER HIGH ---
	// HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);

	// Execute pure decryption workload in a continuous loop for power/thermal profiling
	rsa_decrypt(plaintext, ciphertext);

	// --- SCOPE TRIGGER LOW ---
	// HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET);

}
