#ifndef INC_cMD5
#define INC_cMD5

#include <stdint.h>

#define MD5_DIGEST_LENGTH 16

/** Single-message MD5 hash (RFC 1321). Always scalar. */
uint8_t *cMD5(uint8_t *message, uint64_t message_len, uint8_t *digest);

/**
 * Multi-message MD5 hash.
 * Processes `count` independent messages in parallel using the best
 * available SIMD backend (SSE2 x4, AVX2 x8, AVX-512 x16, or scalar).
 * Each digests[i] must be a caller-allocated MD5_DIGEST_LENGTH-byte buffer.
 */
void cMD5_multi(uint8_t **msgs, uint64_t *lens,
                uint8_t **digests, uint32_t count);

#endif /* INC_cMD5 */