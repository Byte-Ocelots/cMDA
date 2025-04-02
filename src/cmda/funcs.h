#ifndef _FUNCS_MD_RFC
#define _FUNCS_MD_RFC

#include <stdint.h>

typedef enum
{
	EMPTY,
	NONE,
	SSE,
	SSE2,
	AVX,
	AVX2
} cMDA_CPU;

typedef uint32_t (*cMDA_func_t)(uint32_t X, uint32_t Y, uint32_t Z);

extern cMDA_CPU __cMDA_CPU;

extern cMDA_func_t F;
extern cMDA_func_t G4;
extern cMDA_func_t G5;
extern cMDA_func_t H;
extern cMDA_func_t I;


uint64_t htobe64(uint64_t host_64bits);

uint32_t ROTL(uint32_t x, uint8_t n);

void set_cpu_supported_op();

#endif
