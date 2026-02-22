#ifndef _COMMON_CMDA_H
#define _COMMON_CMDA_H

/* AVR has no uint64_t in avr-libc stdint — catch early */
#ifdef __AVR__
#  error "cMDA single-message API requires at least 32-bit uint64_t support. " \
         "For AVR, build with CMDA_NO_MULTI only (lib targets produce uint32_t-safe code)."
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#endif /* _COMMON_CMDA_H */
