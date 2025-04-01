#include "funcs.h"
#include <cpuid.h>
#include <immintrin.h>
#include <stdio.h>

cMDA_CPU __cMDA_CPU = EMPTY;

uint64_t htobe64(uint64_t host_64bits)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	return __builtin_bswap64(host_64bits);
#else
	return host_64bits;
#endif
}

cMDA_func_t F = NULL;

uint32_t F_sse(uint32_t X, uint32_t Y, uint32_t Z)
{
	// Load values into SSE registers
	__m128i vx = _mm_set1_epi32(X); // Set all elements of the register to X
	__m128i vy = _mm_set1_epi32(Y); // Set all elements of the register to Y
	__m128i vz = _mm_set1_epi32(Z); // Set all elements of the register to Z

	// Perform the bitwise operations (X & Y) | (~X & Z)
	__m128i vnot_x = _mm_andnot_si128(vx, _mm_set1_epi32(-1)); // ~X
	__m128i result_sse = _mm_or_si128(_mm_and_si128(vx, vy), _mm_and_si128(vnot_x, vz));

	// Extract the first element from the SIMD register and return it
	return _mm_extract_epi32(result_sse, 0); // Extract the first element (index 0)
}

uint32_t F_avx(uint32_t X, uint32_t Y, uint32_t Z)
{
	// Load values into AVX registers (256-bit registers)
	__m256i vx = _mm256_set1_epi32(X); // Set all elements of the register to X
	__m256i vy = _mm256_set1_epi32(Y); // Set all elements of the register to Y
	__m256i vz = _mm256_set1_epi32(Z); // Set all elements of the register to Z

	// Perform the bitwise operations (X & Y) | (~X & Z)
	__m256i vnot_x = _mm256_andnot_si256(vx, _mm256_set1_epi32(-1)); // ~X
	__m256i result_avx = _mm256_or_si256(_mm256_and_si256(vx, vy), _mm256_and_si256(vnot_x, vz));

	// Extract the first element from the SIMD register and return it
	return _mm256_extract_epi32(result_avx, 0); // Extract the first element (index 0)
}

uint32_t F_scalar(uint32_t X, uint32_t Y, uint32_t Z)
{
	return (X & Y) | ((~X) & Z);
}

cMDA_func_t G4 = NULL;

uint32_t G4_sse(uint32_t X, uint32_t Y, uint32_t Z)
{
	// Load values into SSE registers
	__m128i vx = _mm_set1_epi32(X); // Set all elements of the register to X
	__m128i vy = _mm_set1_epi32(Y); // Set all elements of the register to Y
	__m128i vz = _mm_set1_epi32(Z); // Set all elements of the register to Z

	// Perform the bitwise operations (X & Y) | (X & Z) | (Y & Z)
	__m128i result_sse = _mm_or_si128(_mm_or_si128(_mm_and_si128(vx, vy), _mm_and_si128(vx, vz)), _mm_and_si128(vy, vz));

	// Extract the first element from the SIMD register and return it
	return _mm_extract_epi32(result_sse, 0); // Extract the first element (index 0)
}

uint32_t G4_avx(uint32_t X, uint32_t Y, uint32_t Z)
{
	// Load values into AVX registers (256-bit registers)
	__m256i vx = _mm256_set1_epi32(X); // Set all elements of the register to X
	__m256i vy = _mm256_set1_epi32(Y); // Set all elements of the register to Y
	__m256i vz = _mm256_set1_epi32(Z); // Set all elements of the register to Z

	// Perform the bitwise operations (X & Y) | (X & Z) | (Y & Z)
	__m256i result_avx = _mm256_or_si256(_mm256_or_si256(_mm256_and_si256(vx, vy), _mm256_and_si256(vx, vz)), _mm256_and_si256(vy, vz));

	// Extract the first element from the SIMD register and return it
	return _mm256_extract_epi32(result_avx, 0); // Extract the first element (index 0)
}

uint32_t G4_scalar(uint32_t X, uint32_t Y, uint32_t Z)
{
	return (X & Y) | (X & Z) | (Y & Z);
}

/* G5 */
cMDA_func_t G5 = NULL;

uint32_t G5_sse(uint32_t X, uint32_t Y, uint32_t Z)
{
	// Load values into SSE registers
	__m128i vx = _mm_set1_epi32(X); // Set all elements of the register to X
	__m128i vy = _mm_set1_epi32(Y); // Set all elements of the register to Y
	__m128i vz = _mm_set1_epi32(Z); // Set all elements of the register to Z

	// Perform the bitwise operations (X & Z) | (Y & (~Z))
	__m128i vnot_z = _mm_andnot_si128(vz, _mm_set1_epi32(-1)); // ~Z
	__m128i result_sse = _mm_or_si128(_mm_and_si128(vx, vz), _mm_and_si128(vy, vnot_z));

	// Extract the first element from the SIMD register and return it
	return _mm_extract_epi32(result_sse, 0); // Extract the first element (index 0)
}

uint32_t G5_avx(uint32_t X, uint32_t Y, uint32_t Z)
{
	// Load values into AVX registers (256-bit registers)
	__m256i vx = _mm256_set1_epi32(X); // Set all elements of the register to X
	__m256i vy = _mm256_set1_epi32(Y); // Set all elements of the register to Y
	__m256i vz = _mm256_set1_epi32(Z); // Set all elements of the register to Z

	// Perform the bitwise operations (X & Z) | (Y & (~Z))
	__m256i vnot_z = _mm256_andnot_si256(vz, _mm256_set1_epi32(-1)); // ~Z
	__m256i result_avx = _mm256_or_si256(_mm256_and_si256(vx, vz), _mm256_and_si256(vy, vnot_z));

	// Extract the first element from the SIMD register and return it
	return _mm256_extract_epi32(result_avx, 0); // Extract the first element (index 0)
}

uint32_t G5_scalar(uint32_t X, uint32_t Y, uint32_t Z)
{
	return (X & Z) | (Y & (~Z));
}

/* H */
cMDA_func_t H = NULL;

uint32_t H_sse(uint32_t X, uint32_t Y, uint32_t Z)
{
	// Load values into SSE registers
	__m128i vx = _mm_set1_epi32(X); // Set all elements of the register to X
	__m128i vy = _mm_set1_epi32(Y); // Set all elements of the register to Y
	__m128i vz = _mm_set1_epi32(Z); // Set all elements of the register to Z

	// Perform the bitwise operations X ^ Y ^ Z
	__m128i result_sse = _mm_xor_si128(_mm_xor_si128(vx, vy), vz);

	// Extract the first element from the SIMD register and return it
	return _mm_extract_epi32(result_sse, 0); // Extract the first element (index 0)
}

uint32_t H_avx(uint32_t X, uint32_t Y, uint32_t Z)
{
	// Load values into AVX registers (256-bit registers)
	__m256i vx = _mm256_set1_epi32(X); // Set all elements of the register to X
	__m256i vy = _mm256_set1_epi32(Y); // Set all elements of the register to Y
	__m256i vz = _mm256_set1_epi32(Z); // Set all elements of the register to Z

	// Perform the bitwise operations X ^ Y ^ Z
	__m256i result_avx = _mm256_xor_si256(_mm256_xor_si256(vx, vy), vz);

	// Extract the first element from the SIMD register and return it
	return _mm256_extract_epi32(result_avx, 0); // Extract the first element (index 0)
}

uint32_t H_scalar(uint32_t X, uint32_t Y, uint32_t Z)
{
	return X ^ Y ^ Z;
}

/* I */
cMDA_func_t I = NULL;

uint32_t I_sse(uint32_t X, uint32_t Y, uint32_t Z)
{
	// Load values into SSE registers
	__m128i vx = _mm_set1_epi32(X); // Set all elements of the register to X
	__m128i vy = _mm_set1_epi32(Y); // Set all elements of the register to Y
	__m128i vz = _mm_set1_epi32(Z); // Set all elements of the register to Z

	// Perform the bitwise operations Y ^ (X | (~Z))
	__m128i vnot_z = _mm_andnot_si128(vz, _mm_set1_epi32(-1)); // ~Z
	__m128i result_sse = _mm_xor_si128(vy, _mm_or_si128(vx, vnot_z));

	// Extract the first element from the SIMD register and return it
	return _mm_extract_epi32(result_sse, 0); // Extract the first element (index 0)
}

uint32_t I_avx(uint32_t X, uint32_t Y, uint32_t Z)
{
	// Load values into AVX registers (256-bit registers)
	__m256i vx = _mm256_set1_epi32(X); // Set all elements of the register to X
	__m256i vy = _mm256_set1_epi32(Y); // Set all elements of the register to Y
	__m256i vz = _mm256_set1_epi32(Z); // Set all elements of the register to Z

	// Perform the bitwise operations Y ^ (X | (~Z))
	__m256i vnot_z = _mm256_andnot_si256(vz, _mm256_set1_epi32(-1)); // ~Z
	__m256i result_avx = _mm256_xor_si256(vy, _mm256_or_si256(vx, vnot_z));

	// Extract the first element from the SIMD register and return it
	return _mm256_extract_epi32(result_avx, 0); // Extract the first element (index 0)
}

uint32_t I_scalar(uint32_t X, uint32_t Y, uint32_t Z)
{
	return X ^ Y ^ Z;
}

uint32_t ROTL(uint32_t x, uint8_t n)
{
	return (x << n) | (x >> (32 - n));
}

void set_cpu_supported_op()
{
	unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;

	__cMDA_CPU = NONE;
	F = F_scalar;
	G4 = G4_scalar;
	G5 = G5_scalar;
	H = H_scalar;
	I = I_scalar;

	// Get the CPUID feature flags for the base (eax = 1)
	__get_cpuid(1, &eax, &ebx, &ecx, &edx);

	// Check if SSE is supported (Bit 25 of edx)
	if (edx & (1 << 25))
	{
		__cMDA_CPU = SSE;
		F = F_sse;
		G4 = G4_sse;
		G5 = G5_sse;
		H = H_sse;
		I = I_sse;
	}

	// Check if SSE2 is supported (Bit 26 of edx)
	if (edx & (1 << 26))
	{
		__cMDA_CPU = SSE2;
		F = F_sse;
		G4 = G4_sse;
		G5 = G5_sse;
		H = H_sse;
		I = I_sse;
	}

	// Check if AVX is supported (Bit 28 of ecx)
	if (ecx & (1 << 28))
	{
		__cMDA_CPU = AVX;
		F = F_avx;
		G4 = G4_avx;
		G5 = G5_avx;
		H = H_avx;
		I = I_avx;
	}

	// Get the CPUID feature flags for extended features (eax = 7, subleaf = 0)
	__get_cpuid(7, &eax, &ebx, &ecx, &edx);

	// Check if AVX2 is supported (Bit 5 of ebx for CPUID leaf 7, subleaf 0)
	if (ebx & (1 << 5))
	{
		__cMDA_CPU = AVX2;
		F = F_avx;
		G4 = G4_avx;
		G5 = G5_avx;
		H = H_avx;
		I = I_avx;
	}
}