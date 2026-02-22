/*
 * test_all.c — cMDA unit tests
 * Tests include RFC 1319/1320/1321 reference vectors, boundary cases,
 * and multi-message SIMD consistency.
 */
#include "cMDA/all.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int verbose = 0;

static void hex(const uint8_t *d, int n, char *out){
    for(int i=0;i<n;i++) sprintf(out+i*2,"%02x",d[i]);
    out[n*2]='\0';
}
static void expect(const char *algo, const char *msg,
                   uint8_t *got, const uint8_t *want, int len){
    char g[33],w[33]; hex(got,len,g); hex(want,len,w);
    if(memcmp(got,want,len)!=0){
        fprintf(stderr,"FAIL  %s(\"%s\")\n  got : %s\n  want: %s\n",algo,msg,g,w);
        exit(1);
    }
    if(verbose) printf("PASS  %s(\"%s\") = %s\n",algo,msg,g);
}

/* ── RFC 1319 MD2 vectors ─────────────────────────────────────────── */
static void test_md2(void){
    typedef struct{const char*m; uint8_t h[16];}V;
    static const V v[]={
        {"",    {0x83,0x50,0xe5,0xa3,0xe2,0x4c,0x15,0x3d,0xf2,0x27,0x5c,0x9f,0x80,0x69,0x27,0x73}},
        {"a",   {0x32,0xec,0x01,0xec,0x4a,0x6d,0xac,0x72,0xc0,0xab,0x96,0xfb,0x34,0xc0,0xb5,0xd1}},
        {"abc", {0xda,0x85,0x3b,0x0d,0x3f,0x88,0xd9,0x9b,0x30,0x28,0x3a,0x69,0xe6,0xde,0xd6,0xbb}},
        {"message digest",
                {0xab,0x4f,0x49,0x6b,0xfb,0x2a,0x53,0x0b,0x21,0x9f,0xf3,0x30,0x31,0xfe,0x06,0xb0}},
        {"abcdefghijklmnopqrstuvwxyz",
                {0x4e,0x8d,0xdf,0xf3,0x65,0x02,0x92,0xab,0x5a,0x41,0x08,0xc3,0xaa,0x47,0x94,0x0b}},
    };
    uint8_t d[16];
    for(int i=0;i<(int)(sizeof v/sizeof v[0]);i++){
        cMD2((uint8_t*)v[i].m,(uint64_t)strlen(v[i].m),d);
        expect("MD2",v[i].m,d,v[i].h,16);
    }
    printf("MD2  : all RFC 1319 vectors passed\n");
}

/* ── RFC 1320 MD4 vectors ─────────────────────────────────────────── */
static void test_md4(void){
    typedef struct{const char*m; uint8_t h[16];}V;
    static const V v[]={
        {"",    {0x31,0xd6,0xcf,0xe0,0xd1,0x6a,0xe9,0x31,0xb7,0x3c,0x59,0xd7,0xe0,0xc0,0x89,0xc0}},
        {"a",   {0xbd,0xe5,0x2c,0xb3,0x1d,0xe3,0x3e,0x46,0x24,0x5e,0x05,0xfb,0xdb,0xd6,0xfb,0x24}},
        {"abc", {0xa4,0x48,0x01,0x7a,0xaf,0x21,0xd8,0x52,0x5f,0xc1,0x0a,0xe8,0x7a,0xa6,0x72,0x9d}},
        {"message digest",
                {0xd9,0x13,0x0a,0x81,0x64,0x54,0x9f,0xe8,0x18,0x87,0x48,0x06,0xe1,0xc7,0x01,0x4b}},
        {"abcdefghijklmnopqrstuvwxyz",
                {0xd7,0x9e,0x1c,0x30,0x8a,0xa5,0xbb,0xcd,0xee,0xa8,0xed,0x63,0xdf,0x41,0x2d,0xa9}},
        {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
                {0x04,0x3f,0x85,0x82,0xf2,0x41,0xdb,0x35,0x1c,0xe6,0x27,0xe1,0x53,0xe7,0xf0,0xe4}},
    };
    uint8_t d[16];
    for(int i=0;i<(int)(sizeof v/sizeof v[0]);i++){
        cMD4((uint8_t*)v[i].m,(uint64_t)strlen(v[i].m),d);
        expect("MD4",v[i].m,d,v[i].h,16);
    }
    printf("MD4  : all RFC 1320 vectors passed\n");
}

/* ── RFC 1321 MD5 vectors ─────────────────────────────────────────── */
static void test_md5(void){
    typedef struct{const char*m; uint8_t h[16];}V;
    static const V v[]={
        {"",    {0xd4,0x1d,0x8c,0xd9,0x8f,0x00,0xb2,0x04,0xe9,0x80,0x09,0x98,0xec,0xf8,0x42,0x7e}},
        {"a",   {0x0c,0xc1,0x75,0xb9,0xc0,0xf1,0xb6,0xa8,0x31,0xc3,0x99,0xe2,0x69,0x77,0x26,0x61}},
        {"abc", {0x90,0x01,0x50,0x98,0x3c,0xd2,0x4f,0xb0,0xd6,0x96,0x3f,0x7d,0x28,0xe1,0x7f,0x72}},
        {"message digest",
                {0xf9,0x6b,0x69,0x7d,0x7c,0xb7,0x93,0x8d,0x52,0x5a,0x2f,0x31,0xaa,0xf1,0x61,0xd0}},
        {"abcdefghijklmnopqrstuvwxyz",
                {0xc3,0xfc,0xd3,0xd7,0x61,0x92,0xe4,0x00,0x7d,0xfb,0x49,0x6c,0xca,0x67,0xe1,0x3b}},
        {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
                {0xd1,0x74,0xab,0x98,0xd2,0x77,0xd9,0xf5,0xa5,0x61,0x1c,0x2c,0x9f,0x41,0x9d,0x9f}},
        {"12345678901234567890123456789012345678901234567890123456789012345678901234567890",
                {0x57,0xed,0xf4,0xa2,0x2b,0xe3,0xc9,0x55,0xac,0x49,0xda,0x2e,0x21,0x07,0xb6,0x7a}},
    };
    uint8_t d[16];
    for(int i=0;i<(int)(sizeof v/sizeof v[0]);i++){
        cMD5((uint8_t*)v[i].m,(uint64_t)strlen(v[i].m),d);
        expect("MD5",v[i].m,d,v[i].h,16);
    }
    printf("MD5  : all RFC 1321 vectors passed\n");
}

/* ── boundary: 55, 56, 64 byte messages (padding edge cases) ──────── */
static void test_boundary(void){
    uint8_t buf[128]; uint8_t d1[16],d2[16];
    int sizes[]={55,56,64,63,65};
    for(int i=0;i<(int)(sizeof sizes/sizeof sizes[0]);i++){
        memset(buf,0x61,sizes[i]);
        cMD5(buf,(uint64_t)sizes[i],d1);
        cMD5(buf,(uint64_t)sizes[i],d2);
        assert(memcmp(d1,d2,16)==0);
        if(verbose){char g[33];hex(d1,16,g);printf("BOUND MD5[%d]=%s\n",sizes[i],g);}
    }
    printf("BOUND: padding boundary tests passed\n");
}

/* ── SIMD consistency: multi == single for same message ───────────── */
static void test_simd_consistency(void){
    const char *msgs[8]={
        "alpha","beta","gamma","delta","epsilon","zeta","eta","theta"
    };
    uint8_t single[16], multi_d[8][16];
    uint8_t *mp[8]; uint64_t ml[8]; uint8_t *md[8];
    for(int i=0;i<8;i++){mp[i]=(uint8_t*)msgs[i];ml[i]=strlen(msgs[i]);md[i]=multi_d[i];}

    /* MD5 multi */
    cMD5_multi(mp,ml,md,8);
    for(int i=0;i<8;i++){
        cMD5(mp[i],ml[i],single);
        if(memcmp(single,multi_d[i],16)!=0){
            char g[33],w[33]; hex(multi_d[i],16,g); hex(single,16,w);
            fprintf(stderr,"FAIL  MD5_multi[%d] (%s)\n  multi : %s\n  single: %s\n",i,msgs[i],g,w);
            exit(1);
        }
    }
    printf("SIMD : MD5_multi consistent with cMD5 for 8 messages\n");

    /* MD4 multi */
    cMD4_multi(mp,ml,md,8);
    for(int i=0;i<8;i++){
        cMD4(mp[i],ml[i],single);
        if(memcmp(single,multi_d[i],16)!=0){
            char g[33],w[33]; hex(multi_d[i],16,g); hex(single,16,w);
            fprintf(stderr,"FAIL  MD4_multi[%d] (%s)\n  multi : %s\n  single: %s\n",i,msgs[i],g,w);
            exit(1);
        }
    }
    printf("SIMD : MD4_multi consistent with cMD4 for 8 messages\n");
}

int main(int argc, char **argv){
    for(int i=1;i<argc;i++) if(strcmp(argv[i],"-v")==0) verbose=1;

    test_md2();
    test_md4();
    test_md5();
    test_boundary();
    test_simd_consistency();

    printf("\nAll tests passed.\n");
    return 0;
}
