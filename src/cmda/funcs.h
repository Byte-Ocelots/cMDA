#ifndef _FUNCS_CMDA_H
#define _FUNCS_CMDA_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* CPU capability enum (set at runtime by set_cpu_supported_op)        */
/* ------------------------------------------------------------------ */
typedef enum {
    EMPTY   = 0,  /* not yet initialised   */
    NONE    = 1,  /* no SIMD               */
    SSE2    = 2,
    AVX2    = 3,
    AVX512F = 4
} cMDA_CPU;

extern cMDA_CPU __cMDA_CPU;

/* Detect CPU features and populate __cMDA_CPU at runtime. */
void set_cpu_supported_op(void);

/* ------------------------------------------------------------------ */
/* Utility helpers                                                     */
/* ------------------------------------------------------------------ */
uint64_t htobe64(uint64_t host_64bits);
uint32_t ROTL(uint32_t x, uint8_t n);

/* ------------------------------------------------------------------ */
/* Scalar round functions (used by single-message path)               */
/* ------------------------------------------------------------------ */
uint32_t F_scalar (uint32_t B, uint32_t C, uint32_t D); /* (B&C)|(~B&D)   */
uint32_t G4_scalar(uint32_t B, uint32_t C, uint32_t D); /* (B&C)|(B&D)|(C&D) */
uint32_t G5_scalar(uint32_t B, uint32_t C, uint32_t D); /* (B&D)|(C&~D)   */
uint32_t H_scalar (uint32_t B, uint32_t C, uint32_t D); /* B^C^D          */
uint32_t I_scalar (uint32_t B, uint32_t C, uint32_t D); /* C^(B|~D)       */

/* ------------------------------------------------------------------ */
/* Multi-message API (SIMD-accelerated when available)                 */
/* Feed `count` independent messages; each digests[i] must be         */
/* a caller-allocated 16-byte buffer.                                 */
/* MD2 is excluded: its S-box lookups are inherently serial.          */
/* ------------------------------------------------------------------ */
void cMD4_multi(uint8_t **msgs, uint64_t *lens,
                uint8_t **digests, uint32_t count);

void cMD5_multi(uint8_t **msgs, uint64_t *lens,
                uint8_t **digests, uint32_t count);

#endif /* _FUNCS_CMDA_H */
