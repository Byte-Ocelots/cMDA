#ifndef _UTILS_MD_CLI_H
#define _UTILS_MD_CLI_H

#define MD_DIGEST_LENGTH 16
#define CMDA_VERSION     "1.1.0"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * CLI entry-point shared by md2/md4/md5 binaries.
 *
 * Exit codes:
 *   0  Success / hash match
 *   1  Hash mismatch  (--compare / --verify)
 *   2  Usage / argument error
 *   3  I/O or runtime error
 */
int man(int argc, char **argv,
        uint8_t *(*cMD)(uint8_t *, uint64_t, uint8_t *));

#endif /* _UTILS_MD_CLI_H */
