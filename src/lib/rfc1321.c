#include "common.h"
#include "cMDA/md5.h"
#include "funcs.h"
#include <math.h>

void _a5(uint32_t *a, uint32_t b, uint32_t c, uint32_t d, uint8_t k, uint8_t s, uint32_t i, cMDA_func_t OP, uint32_t *X)
{
	(*a) = b + ROTL(((*a) + OP(b, c, d) + X[k] + (fabs(sin(i)) * pow(2, 32))), s);
}

uint8_t *cMD5(uint8_t *message, uint64_t message_len, uint8_t *digest)
{
	uint64_t i;
	uint32_t ABCD[4];
	uint8_t j;

	uint8_t *M = NULL;
	uint32_t A = 0x67452301, B = 0xefcdab89, C = 0x98badcfe, D = 0x10325476; /* Step 3. Initialize MD Buffer */
	uint32_t AA, BB, CC, DD;
	uint32_t X[16];
	uint64_t b = message_len * 8;
	uint64_t shy_bits = b % 512;
	uint64_t remaining_bits;
	uint64_t N;

	/* Step 1. Append Padding Bits */
	if (shy_bits < 448)
	{
		remaining_bits = 448 - shy_bits;
	}
	else if (shy_bits >= 448)
	{
		remaining_bits = 512 - shy_bits + 448;
	}
	N = (b + remaining_bits + 64) / 8;
	M = (uint8_t *)malloc(N);
	memset(M, 0, N);
	memcpy(M, message, message_len);
	memset(M + message_len, 0, remaining_bits / 8);
	M[message_len] = 1 << 7;

	/* Step 2. Append Length */
	for (i = 0; i < 8; i++)
	{
		M[N - 8 + i] = (uint8_t)(b >> i * 8) & 0xFF;
	}

	/* amid */
	if (__cMDA_CPU == EMPTY)
	{
		set_cpu_supported_op();
	}

	/* Step 4. Process Message in 16-Word Blocks */
	for (i = 0; i <= N / 64 - 1; i++)
	{
		for (j = 0; j <= 15; j++)
		{
			X[j] = (uint32_t)M[i * 64 + j * 4] |
				   ((uint32_t)M[i * 64 + j * 4 + 1] << 8) |
				   ((uint32_t)M[i * 64 + j * 4 + 2] << 16) |
				   ((uint32_t)M[i * 64 + j * 4 + 3] << 24);
		}

		AA = A;
		BB = B;
		CC = C;
		DD = D;

		/* Round 1 */
		_a5(&A, B, C, D, 0, 7, 1, F, X);
		_a5(&D, A, B, C, 1, 12, 2, F, X);
		_a5(&C, D, A, B, 2, 17, 3, F, X);
		_a5(&B, C, D, A, 3, 22, 4, F, X);

		_a5(&A, B, C, D, 4, 7, 5, F, X);
		_a5(&D, A, B, C, 5, 12, 6, F, X);
		_a5(&C, D, A, B, 6, 17, 7, F, X);
		_a5(&B, C, D, A, 7, 22, 8, F, X);

		_a5(&A, B, C, D, 8, 7, 9, F, X);
		_a5(&D, A, B, C, 9, 12, 10, F, X);
		_a5(&C, D, A, B, 10, 17, 11, F, X);
		_a5(&B, C, D, A, 11, 22, 12, F, X);

		_a5(&A, B, C, D, 12, 7, 13, F, X);
		_a5(&D, A, B, C, 13, 12, 14, F, X);
		_a5(&C, D, A, B, 14, 17, 15, F, X);
		_a5(&B, C, D, A, 15, 22, 16, F, X);

		/* Round 2 */
		_a5(&A, B, C, D, 1, 5, 17, G5, X);
		_a5(&D, A, B, C, 6, 9, 18, G5, X);
		_a5(&C, D, A, B, 11, 14, 19, G5, X);
		_a5(&B, C, D, A, 0, 20, 20, G5, X);

		_a5(&A, B, C, D, 5, 5, 21, G5, X);
		_a5(&D, A, B, C, 10, 9, 22, G5, X);
		_a5(&C, D, A, B, 15, 14, 23, G5, X);
		_a5(&B, C, D, A, 4, 20, 24, G5, X);

		_a5(&A, B, C, D, 9, 5, 25, G5, X);
		_a5(&D, A, B, C, 14, 9, 26, G5, X);
		_a5(&C, D, A, B, 3, 14, 27, G5, X);
		_a5(&B, C, D, A, 8, 20, 28, G5, X);

		_a5(&A, B, C, D, 13, 5, 29, G5, X);
		_a5(&D, A, B, C, 2, 9, 30, G5, X);
		_a5(&C, D, A, B, 7, 14, 31, G5, X);
		_a5(&B, C, D, A, 12, 20, 32, G5, X);

		/* Round 3 */
		_a5(&A, B, C, D, 5, 4, 33, H, X);
		_a5(&D, A, B, C, 8, 11, 34, H, X);
		_a5(&C, D, A, B, 11, 16, 35, H, X);
		_a5(&B, C, D, A, 14, 23, 36, H, X);

		_a5(&A, B, C, D, 1, 4, 37, H, X);
		_a5(&D, A, B, C, 4, 11, 38, H, X);
		_a5(&C, D, A, B, 7, 16, 39, H, X);
		_a5(&B, C, D, A, 10, 23, 40, H, X);

		_a5(&A, B, C, D, 13, 4, 41, H, X);
		_a5(&D, A, B, C, 0, 11, 42, H, X);
		_a5(&C, D, A, B, 3, 16, 43, H, X);
		_a5(&B, C, D, A, 6, 23, 44, H, X);

		_a5(&A, B, C, D, 9, 4, 45, H, X);
		_a5(&D, A, B, C, 12, 11, 46, H, X);
		_a5(&C, D, A, B, 15, 16, 47, H, X);
		_a5(&B, C, D, A, 2, 23, 48, H, X);

		/* Round 4 */
		_a5(&A, B, C, D, 0, 6, 49, I, X);
		_a5(&D, A, B, C, 7, 10, 50, I, X);
		_a5(&C, D, A, B, 14, 15, 51, I, X);
		_a5(&B, C, D, A, 5, 21, 52, I, X);

		_a5(&A, B, C, D, 12, 6, 53, I, X);
		_a5(&D, A, B, C, 3, 10, 54, I, X);
		_a5(&C, D, A, B, 10, 15, 55, I, X);
		_a5(&B, C, D, A, 1, 21, 56, I, X);

		_a5(&A, B, C, D, 8, 6, 57, I, X);
		_a5(&D, A, B, C, 15, 10, 58, I, X);
		_a5(&C, D, A, B, 6, 15, 59, I, X);
		_a5(&B, C, D, A, 13, 21, 60, I, X);

		_a5(&A, B, C, D, 4, 6, 61, I, X);
		_a5(&D, A, B, C, 11, 10, 62, I, X);
		_a5(&C, D, A, B, 2, 15, 63, I, X);
		_a5(&B, C, D, A, 9, 21, 64, I, X);

		A += AA;
		B += BB;
		C += CC;
		D += DD;
	}
	free(M);

	ABCD[0] = A;
	ABCD[1] = B;
	ABCD[2] = C;
	ABCD[3] = D;

	for (i = 0; i < 4; ++i)
	{
		digest[i * 4] = (uint8_t)(ABCD[i] & 0xFF);
		digest[i * 4 + 1] = (uint8_t)((ABCD[i] >> 8) & 0xFF);
		digest[i * 4 + 2] = (uint8_t)((ABCD[i] >> 16) & 0xFF);
		digest[i * 4 + 3] = (uint8_t)((ABCD[i] >> 24) & 0xFF);
	}

	return digest;
}
