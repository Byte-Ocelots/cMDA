/*
 * utils.c — cMDA CLI engine
 *
 * Features:
 *   message hashing    md5 "hello" "world"
 *   file hashing       md5 -f a.bin -f b.bin
 *   stdin hashing      echo hello | md5 --stdin
 *   interactive mode   md5 -i            (hash each line you type)
 *   hash compare       md5 -c HASH msg   (exit 0=match, 1=mismatch)
 *   compare-file       md5 --compare-file hash.txt msg
 *   verify mode        md5 --verify sums.md5
 *   quiet output       md5 -q msg        (print hash only)
 *   format options     md5 --format upper|lower|base64 msg
 *   version info       md5 --version
 *
 * Exit codes:  0=ok  1=mismatch  2=usage error  3=i/o error
 */
#include "utils.h"

/* ── internal helpers ───────────────────────────────────────────── */

typedef struct {
    const char *fmt;        /* "lower" | "upper" | "base64"  */
    const char *cmp;        /* expected hex string or NULL    */
    int quiet;              /* 1 = print hash only            */
    int mismatch_seen;      /* 1 = at least one --compare fail */
} Opts;

static void die_usage(const char *prog, const char *msg)
{
    if (msg)
        fprintf(stderr, "error: %s\n", msg);
    fprintf(stderr, "Try '%s --help' for usage.\n", prog);
    exit(2);
}

static void die_io(const char *context)
{
    fprintf(stderr, "error: %s\n", context);
    exit(3);
}

/* ── output helpers ─────────────────────────────────────────────── */

static void print_hash(const uint8_t *d, uint8_t len, const char *fmt)
{
    static const char B64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    uint8_t i;
    if (strcmp(fmt, "upper") == 0) {
        for (i = 0; i < len; i++) printf("%02X", d[i]);
    } else if (strcmp(fmt, "base64") == 0) {
        char out[25]; int j = 0, bits = 0, buf = 0;
        memset(out, 0, sizeof(out));
        for (i = 0; i < len; i++) {
            buf = (buf << 8) | d[i]; bits += 8;
            while (bits >= 6) { out[j++] = B64[(buf >> (bits-6)) & 0x3F]; bits -= 6; }
        }
        if (bits > 0) out[j++] = B64[(buf << (6-bits)) & 0x3F];
        while (j % 4) out[j++] = '=';
        printf("%s", out);
    } else {
        for (i = 0; i < len; i++) printf("%02x", d[i]);
    }
}

static void digest_to_hex(const uint8_t *d, uint8_t len, char *out)
{
    uint8_t i;
    for (i = 0; i < len; i++) sprintf(out + i*2, "%02x", d[i]);
    out[len*2] = '\0';
}

/* case-insensitive match; expected may be uppercase */
static int hex_match(const uint8_t *digest, uint8_t len, const char *expected)
{
    char got[33]; digest_to_hex(digest, len, got);
    return strcasecmp(got, expected) == 0;
}

/* ── compute + emit for one buffer ─────────────────────────────── */

static int emit(const uint8_t *data, uint64_t datalen,
                const char *label, Opts *o,
                uint8_t *(*cMD)(uint8_t *, uint64_t, uint8_t *))
{
    uint8_t digest[MD_DIGEST_LENGTH];
    cMD((uint8_t *)data, datalen, digest);

    if (o->cmp) {
        int ok = hex_match(digest, MD_DIGEST_LENGTH, o->cmp);
        if (!o->quiet) {
            printf("%s -> ", label);
            print_hash(digest, MD_DIGEST_LENGTH, o->fmt);
            printf("  [%s]\n", ok ? "MATCH" : "MISMATCH");
        } else {
            print_hash(digest, MD_DIGEST_LENGTH, o->fmt);
            printf("\n");
        }
        if (!ok) { o->mismatch_seen = 1; return 1; }
        return 0;
    }

    if (!o->quiet) printf("%s -> ", label);
    print_hash(digest, MD_DIGEST_LENGTH, o->fmt);
    printf("\n");
    return 0;
}

/* ── read whole file into heap buffer ──────────────────────────── */

static uint8_t *slurp(FILE *f, size_t *out_len)
{
    size_t cap = 4096, n = 0; uint8_t *buf = malloc(cap), *tmp; uint8_t chunk[4096];
    if (!buf) return NULL;
    size_t rd;
    while ((rd = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (n + rd > cap) {
            cap = cap * 2 + rd;
            tmp = realloc(buf, cap);
            if (!tmp) { free(buf); return NULL; }
            buf = tmp;
        }
        memcpy(buf + n, chunk, rd); n += rd;
    }
    *out_len = n;
    return buf;
}

/* ── hash a file path ───────────────────────────────────────────── */

static int hash_file(const char *path, Opts *o,
                     uint8_t *(*cMD)(uint8_t *, uint64_t, uint8_t *))
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "error: cannot open '%s': %s\n", path, strerror(0)); return 3; }
    size_t len; uint8_t *data = slurp(f, &len); fclose(f);
    if (!data) { fprintf(stderr, "error: out of memory reading '%s'\n", path); return 3; }
    int rc = emit(data, (uint64_t)len, path, o, cMD);
    free(data);
    return rc;
}

/* ── read expected hash from file (first non-empty line) ────────── */

static int read_hash_file(const char *path, char *out, int maxlen)
{
    FILE *f = fopen(path, "r"); if (!f) return 0;
    if (!fgets(out, maxlen, f)) { fclose(f); return 0; }
    size_t n = strlen(out);
    while (n > 0 && (out[n-1] == '\n' || out[n-1] == '\r')) out[--n] = '\0';
    fclose(f); return n > 0;
}

/* ── interactive mode ───────────────────────────────────────────── */

static void interactive(const char *prog, Opts *o,
                        uint8_t *(*cMD)(uint8_t *, uint64_t, uint8_t *))
{
    char line[8192];
    fprintf(stderr, "%s interactive mode — enter messages, Ctrl-D to quit\n", prog);
    while (1) {
        fprintf(stderr, "> ");
        fflush(stderr);
        if (!fgets(line, sizeof(line), stdin)) break; /* EOF */
        /* strip trailing newline */
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = '\0';
        if (n == 0) continue; /* blank line: skip */
        emit((uint8_t *)line, (uint64_t)n, line, o, cMD);
    }
    fprintf(stderr, "\n");
}

/* ── --verify mode: read "hash  file" lines ─────────────────────── */

static int verify_file(const char *sumfile, Opts *o,
                        uint8_t *(*cMD)(uint8_t *, uint64_t, uint8_t *))
{
    FILE *f = fopen(sumfile, "r");
    if (!f) { fprintf(stderr, "error: cannot open verify file '%s'\n", sumfile); return 3; }
    char line[4096]; int pass = 0, fail = 0, rc = 0;
    while (fgets(line, sizeof(line), f)) {
        /* strip trailing newline/whitespace */
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r' || line[n-1] == ' '))
            line[--n] = '\0';
        if (n == 0 || line[0] == '#') continue; /* blank / comment */
        /* format: "<hash>  <path>" or "<hash> <path>" */
        char *sp = strchr(line, ' ');
        if (!sp) {
            fprintf(stderr, "warning: malformed line (no space): %s\n", line);
            continue;
        }
        *sp = '\0'; const char *expected = line;
        const char *path = sp + 1;
        while (*path == ' ') path++; /* skip extra spaces */
        if (*path == '\0') {
            fprintf(stderr, "warning: malformed line (no path)\n");
            continue;
        }
        /* validate hash length */
        if (strlen(expected) != (size_t)(MD_DIGEST_LENGTH * 2)) {
            fprintf(stderr, "warning: unexpected hash length for '%s'\n", path);
            continue;
        }
        FILE *fp = fopen(path, "rb");
        if (!fp) {
            fprintf(stderr, "MISSING  %s\n", path);
            fail++; rc = 1; continue;
        }
        size_t dlen; uint8_t *data = slurp(fp, &dlen); fclose(fp);
        if (!data) { fprintf(stderr, "error: out of memory\n"); fclose(f); return 3; }
        uint8_t digest[MD_DIGEST_LENGTH];
        cMD(data, (uint64_t)dlen, digest); free(data);
        int ok = hex_match(digest, MD_DIGEST_LENGTH, expected);
        char got[33]; digest_to_hex(digest, MD_DIGEST_LENGTH, got);
        if (ok) {
            printf("OK       %s\n", path); pass++;
        } else {
            printf("FAIL     %s\n  expected: %s\n  got     : %s\n", path, expected, got);
            fail++; rc = 1;
        }
    }
    fclose(f);
    printf("\n%d passed, %d failed.\n", pass, fail);
    return rc;
}

/* ── help / version ─────────────────────────────────────────────── */

static void print_help(const char *prog)
{
    printf(
        "Usage: %s [OPTIONS] [MESSAGE...]\n"
        "\n"
        "Hash messages and files using the corresponding MD algorithm.\n"
        "Multiple sources can be mixed freely on a single command line.\n"
        "\n"
        "Options:\n"
        "  -h, --help                   Show this help and exit.\n"
        "  -V, --version                Show version and exit.\n"
        "  -q, --quiet                  Print only the hash (no label).\n"
        "  -i, --interactive            Read lines from stdin interactively.\n"
        "      --stdin                  Hash data read from stdin (pipe mode).\n"
        "  -f, --file FILE              Hash FILE (repeat for multiple files).\n"
        "      --format lower|upper|base64\n"
        "                               Output format (default: lower hex).\n"
        "  -c, --compare HASH           Compare each computed hash with HASH.\n"
        "                               Exit 1 if any mismatch, 0 if all match.\n"
        "      --compare-file FILE      Read expected hash from FILE (first line).\n"
        "      --verify FILE            Verify hashes listed in FILE.\n"
        "                               FILE format (one entry per line):\n"
        "                                 <hash>  <filepath>\n"
        "\n"
        "Exit codes:\n"
        "  0  All hashes matched (or no comparison requested).\n"
        "  1  Hash mismatch detected.\n"
        "  2  Argument / usage error.\n"
        "  3  I/O or runtime error.\n"
        "\n"
        "Examples:\n"
        "  %s \"hello world\"\n"
        "  %s -f /etc/hosts -f /etc/passwd\n"
        "  %s --format base64 \"abc\"\n"
        "  %s -c 900150983cd24fb0d6963f7d28e17f72 \"abc\"\n"
        "  echo -n abc | %s --stdin\n"
        "  %s -i\n"
        "  %s --verify checksums.txt\n",
        prog, prog, prog, prog, prog, prog, prog, prog
    );
}

static void print_version(const char *prog)
{
    printf("%s (cMDA) version %s\n", prog, CMDA_VERSION);
}

/* ── validate --format arg ──────────────────────────────────────── */

static const char *parse_format(const char *prog, const char *val)
{
    if (strcmp(val, "lower") == 0 ||
        strcmp(val, "upper") == 0 ||
        strcmp(val, "base64") == 0)
        return val;
    fprintf(stderr, "error: unknown format '%s'. Choose: lower, upper, base64.\n", val);
    die_usage(prog, NULL);
    return NULL; /* unreachable */
}

/* ═══════════════════════════════════════════════════════════════════ */
/* Main entry point                                                    */
/* ═══════════════════════════════════════════════════════════════════ */

int man(int argc, char **argv,
        uint8_t *(*cMD)(uint8_t *, uint64_t, uint8_t *))
{
    /* Extract basename from argv[0] for cleaner help output */
    const char *prog = argv[0];
    const char *p = strrchr(prog, '/');
    if (p) prog = p + 1;
    p = strrchr(prog, '\\');
    if (p) prog = p + 1;
    /* Strip .exe if present */
    char prog_name[64];
    strncpy(prog_name, prog, sizeof(prog_name) - 1);
    prog_name[sizeof(prog_name) - 1] = '\0';
    char *ext = strrchr(prog_name, '.');
    if (ext && strcasecmp(ext, ".exe") == 0) *ext = '\0';
    prog = prog_name;

    Opts o = { "lower", NULL, 0, 0 };
    char cmpf_buf[65] = {0};
    int did_something = 0;
    int rc = 0;

    if (argc < 2) {
        print_help(prog);
        return 0;
    }

    /* ── single-pass argument parse ─────────────────────────────── */
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];

        /* ── flags ─────────────────────────────────────────────── */
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            print_help(prog); return 0;
        }
        if (strcmp(a, "-V") == 0 || strcmp(a, "--version") == 0) {
            print_version(prog); return 0;
        }
        if (strcmp(a, "-q") == 0 || strcmp(a, "--quiet") == 0) {
            o.quiet = 1; continue;
        }

        /* ── --format ───────────────────────────────────────────── */
        if (strcmp(a, "--format") == 0) {
            if (i + 1 >= argc) die_usage(prog, "--format requires an argument");
            o.fmt = parse_format(prog, argv[++i]);
            continue;
        }
        /* --format=VALUE shorthand */
        if (strncmp(a, "--format=", 9) == 0) {
            o.fmt = parse_format(prog, a + 9);
            continue;
        }

        /* ── --compare / -c ─────────────────────────────────────── */
        if (strcmp(a, "--compare") == 0 || strcmp(a, "-c") == 0) {
            if (i + 1 >= argc) die_usage(prog, "--compare requires a HASH argument");
            o.cmp = argv[++i];
            if (strlen(o.cmp) != (size_t)(MD_DIGEST_LENGTH * 2)) {
                fprintf(stderr, "error: expected a %d-char hex hash, got %zu chars\n",
                        MD_DIGEST_LENGTH * 2, strlen(o.cmp));
                return 2;
            }
            continue;
        }

        /* ── --compare-file ─────────────────────────────────────── */
        if (strcmp(a, "--compare-file") == 0) {
            if (i + 1 >= argc) die_usage(prog, "--compare-file requires a FILE argument");
            if (!read_hash_file(argv[++i], cmpf_buf, sizeof(cmpf_buf))) {
                fprintf(stderr, "error: cannot read hash from '%s'\n", argv[i]);
                return 3;
            }
            if (strlen(cmpf_buf) != (size_t)(MD_DIGEST_LENGTH * 2)) {
                fprintf(stderr, "error: hash in file is not %d hex chars\n",
                        MD_DIGEST_LENGTH * 2);
                return 2;
            }
            o.cmp = cmpf_buf;
            continue;
        }

        /* ── --verify FILE ──────────────────────────────────────── */
        if (strcmp(a, "--verify") == 0) {
            if (i + 1 >= argc) die_usage(prog, "--verify requires a FILE argument");
            int vrc = verify_file(argv[++i], &o, cMD);
            if (vrc > rc) rc = vrc;
            did_something = 1;
            continue;
        }

        /* ── --stdin ─────────────────────────────────────────────── */
        if (strcmp(a, "--stdin") == 0) {
            size_t len; uint8_t *data = slurp(stdin, &len);
            if (!data) die_io("reading stdin");
            int r = emit(data, (uint64_t)len, "<stdin>", &o, cMD);
            free(data);
            if (r > rc) rc = r;
            did_something = 1;
            continue;
        }

        /* ── -i / --interactive ─────────────────────────────────── */
        if (strcmp(a, "-i") == 0 || strcmp(a, "--interactive") == 0) {
            interactive(prog, &o, cMD);
            did_something = 1;
            continue;
        }

        /* ── -f / --file FILE ───────────────────────────────────── */
        if (strcmp(a, "-f") == 0 || strcmp(a, "--file") == 0) {
            if (i + 1 >= argc) die_usage(prog, "--file requires a FILE argument");
            int r = hash_file(argv[++i], &o, cMD);
            if (r > rc) rc = r;
            did_something = 1;
            continue;
        }
        /* --file=PATH shorthand */
        if (strncmp(a, "--file=", 7) == 0) {
            int r = hash_file(a + 7, &o, cMD);
            if (r > rc) rc = r;
            did_something = 1;
            continue;
        }

        /* ── unknown option ─────────────────────────────────────── */
        if (a[0] == '-' && a[1] != '\0') {
            fprintf(stderr, "error: unknown option '%s'\n", a);
            die_usage(prog, NULL);
        }

        /* ── plain message argument ─────────────────────────────── */
        int r = emit((uint8_t *)a, (uint64_t)strlen(a), a, &o, cMD);
        if (r > rc) rc = r;
        did_something = 1;
    }

    if (!did_something) {
        fprintf(stderr, "error: no input given.\n");
        die_usage(prog, "provide a message, -f FILE, --stdin, -i, or --verify FILE");
    }

    /* consolidate mismatch into exit code */
    if (o.mismatch_seen && rc < 1) rc = 1;
    return rc;
}
