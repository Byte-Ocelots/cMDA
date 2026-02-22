/*
 * rfc1321.c — MD5 single-message scalar implementation (RFC 1321)
 * T constants are precomputed (was using slow float sin/pow at runtime).
 */
#include "common.h"
#include "cMDA/md5.h"
#include "funcs.h"

/* floor(2^32 * |sin(i)|), i = 1..64 */
static const uint32_t MD5_T[65] = {
 0,
 0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
 0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
 0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
 0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
 0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
 0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
 0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
 0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
};

/* a = b + ROTL(a + OP(b,c,d) + X[k] + T[i], s) */
#define STEP(a,b,c,d,k,s,i,OP) \
    (a) = (b) + ROTL((a) + OP(b,c,d) + X[k] + MD5_T[i], s);

uint8_t *cMD5(uint8_t *message, uint64_t message_len, uint8_t *digest)
{
    uint64_t i;
    uint32_t ABCD[4];
    uint8_t  j;
    uint8_t *M = NULL;

    uint32_t A = 0x67452301, B = 0xefcdab89,
             C = 0x98badcfe, D = 0x10325476;
    uint32_t AA, BB, CC, DD, X[16];

    uint64_t b        = message_len * 8;
    uint64_t shy      = b % 512;
    uint64_t rem      = (shy < 448) ? (448 - shy) : (512 - shy + 448);
    uint64_t N        = (b + rem + 64) / 8;

    M = (uint8_t *)malloc(N);
    memset(M, 0, N);
    memcpy(M, message, message_len);
    M[message_len] = 0x80;
    for (i = 0; i < 8; i++)
        M[N - 8 + i] = (uint8_t)(b >> (i * 8)) & 0xFF;

    if (__cMDA_CPU == EMPTY) set_cpu_supported_op();

    for (i = 0; i <= N / 64 - 1; i++) {
        for (j = 0; j <= 15; j++)
            X[j] = (uint32_t)M[i*64+j*4]        |
                   ((uint32_t)M[i*64+j*4+1]<<8)  |
                   ((uint32_t)M[i*64+j*4+2]<<16) |
                   ((uint32_t)M[i*64+j*4+3]<<24);
        AA=A; BB=B; CC=C; DD=D;

        STEP(A,B,C,D, 0, 7, 1,F_scalar) STEP(D,A,B,C, 1,12, 2,F_scalar)
        STEP(C,D,A,B, 2,17, 3,F_scalar) STEP(B,C,D,A, 3,22, 4,F_scalar)
        STEP(A,B,C,D, 4, 7, 5,F_scalar) STEP(D,A,B,C, 5,12, 6,F_scalar)
        STEP(C,D,A,B, 6,17, 7,F_scalar) STEP(B,C,D,A, 7,22, 8,F_scalar)
        STEP(A,B,C,D, 8, 7, 9,F_scalar) STEP(D,A,B,C, 9,12,10,F_scalar)
        STEP(C,D,A,B,10,17,11,F_scalar) STEP(B,C,D,A,11,22,12,F_scalar)
        STEP(A,B,C,D,12, 7,13,F_scalar) STEP(D,A,B,C,13,12,14,F_scalar)
        STEP(C,D,A,B,14,17,15,F_scalar) STEP(B,C,D,A,15,22,16,F_scalar)

        STEP(A,B,C,D, 1, 5,17,G5_scalar) STEP(D,A,B,C, 6, 9,18,G5_scalar)
        STEP(C,D,A,B,11,14,19,G5_scalar) STEP(B,C,D,A, 0,20,20,G5_scalar)
        STEP(A,B,C,D, 5, 5,21,G5_scalar) STEP(D,A,B,C,10, 9,22,G5_scalar)
        STEP(C,D,A,B,15,14,23,G5_scalar) STEP(B,C,D,A, 4,20,24,G5_scalar)
        STEP(A,B,C,D, 9, 5,25,G5_scalar) STEP(D,A,B,C,14, 9,26,G5_scalar)
        STEP(C,D,A,B, 3,14,27,G5_scalar) STEP(B,C,D,A, 8,20,28,G5_scalar)
        STEP(A,B,C,D,13, 5,29,G5_scalar) STEP(D,A,B,C, 2, 9,30,G5_scalar)
        STEP(C,D,A,B, 7,14,31,G5_scalar) STEP(B,C,D,A,12,20,32,G5_scalar)

        STEP(A,B,C,D, 5, 4,33,H_scalar) STEP(D,A,B,C, 8,11,34,H_scalar)
        STEP(C,D,A,B,11,16,35,H_scalar) STEP(B,C,D,A,14,23,36,H_scalar)
        STEP(A,B,C,D, 1, 4,37,H_scalar) STEP(D,A,B,C, 4,11,38,H_scalar)
        STEP(C,D,A,B, 7,16,39,H_scalar) STEP(B,C,D,A,10,23,40,H_scalar)
        STEP(A,B,C,D,13, 4,41,H_scalar) STEP(D,A,B,C, 0,11,42,H_scalar)
        STEP(C,D,A,B, 3,16,43,H_scalar) STEP(B,C,D,A, 6,23,44,H_scalar)
        STEP(A,B,C,D, 9, 4,45,H_scalar) STEP(D,A,B,C,12,11,46,H_scalar)
        STEP(C,D,A,B,15,16,47,H_scalar) STEP(B,C,D,A, 2,23,48,H_scalar)

        STEP(A,B,C,D, 0, 6,49,I_scalar) STEP(D,A,B,C, 7,10,50,I_scalar)
        STEP(C,D,A,B,14,15,51,I_scalar) STEP(B,C,D,A, 5,21,52,I_scalar)
        STEP(A,B,C,D,12, 6,53,I_scalar) STEP(D,A,B,C, 3,10,54,I_scalar)
        STEP(C,D,A,B,10,15,55,I_scalar) STEP(B,C,D,A, 1,21,56,I_scalar)
        STEP(A,B,C,D, 8, 6,57,I_scalar) STEP(D,A,B,C,15,10,58,I_scalar)
        STEP(C,D,A,B, 6,15,59,I_scalar) STEP(B,C,D,A,13,21,60,I_scalar)
        STEP(A,B,C,D, 4, 6,61,I_scalar) STEP(D,A,B,C,11,10,62,I_scalar)
        STEP(C,D,A,B, 2,15,63,I_scalar) STEP(B,C,D,A, 9,21,64,I_scalar)

        A+=AA; B+=BB; C+=CC; D+=DD;
    }
    free(M);

    ABCD[0]=A; ABCD[1]=B; ABCD[2]=C; ABCD[3]=D;
    for (i = 0; i < 4; i++) {
        digest[i*4+0]=(uint8_t)(ABCD[i]);
        digest[i*4+1]=(uint8_t)(ABCD[i]>>8);
        digest[i*4+2]=(uint8_t)(ABCD[i]>>16);
        digest[i*4+3]=(uint8_t)(ABCD[i]>>24);
    }
    return digest;
}
