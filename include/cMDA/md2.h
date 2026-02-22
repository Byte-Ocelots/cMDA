#ifndef INC_cMD2
#define INC_cMD2

#include <stdint.h>

#define MD2_DIGEST_LENGTH 16

/** Single-message MD2 hash (RFC 1319). Scalar only; S-box lookups are serial. */
uint8_t *cMD2(uint8_t *message, uint64_t message_len, uint8_t *digest);

#endif /* INC_cMD2 */