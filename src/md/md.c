#include <cMDA/all.h>
#include "utils.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <2|4|5> [options] [messages...]\n", argv[0]);
        fprintf(stderr, "Run '%s 5 --help' for full options.\n", argv[0]);
        return 2;
    }

    /* determine algorithm from first arg */
    char *algo = argv[1];
    if (strcmp(algo, "-h") == 0 || strcmp(algo, "--help") == 0) {
        printf("Usage: %s <2|4|5|md2|md4|md5> [options] [messages...]\n", argv[0]);
        printf("Run '%s 5 --help' for full options of a specific algorithm.\n", argv[0]);
        return 0;
    }

    uint8_t *(*func)(uint8_t *, uint64_t, uint8_t *) = NULL;

    if (strcmp(algo, "2") == 0 || strcmp(algo, "md2") == 0) func = cMD2;
    else if (strcmp(algo, "4") == 0 || strcmp(algo, "md4") == 0) func = cMD4;
    else if (strcmp(algo, "5") == 0 || strcmp(algo, "md5") == 0) func = cMD5;

    if (!func) {
        fprintf(stderr, "error: '%s' is not a valid algorithm. Use 2, 4, or 5.\n", algo);
        return 2;
    }

    /* shift arguments backward so man() sees a normal argc/argv */
    argv[1] = argv[0];
    return man(argc - 1, argv + 1, func);
}