/*
 * funcs.c — cMDA SIMD dispatch & multi-buffer hashing
 *
 * DESIGN: The only correct way to use SIMD for MD4/MD5 is the multi-buffer
 * technique: N independent messages are processed in parallel across N SIMD
 * lanes (SSE2=4, AVX2=8, AVX-512F=16). Round functions take full vectors,
 * not broadcast scalars — every intrinsic does real work across all lanes.
 *
 * The single-message API (cMD4/cMD5 in rfc130x.c) always uses scalar.
 */
#include "funcs.h"
#include "common.h"

/* ── platform ─────────────────────────────────────────────────────── */
#if defined(__x86_64__)||defined(_M_X64)||defined(__i386__)||defined(_M_IX86)
#  define CMDA_X86 1
#  ifdef _MSC_VER
#    include <intrin.h>
#  else
#    include <cpuid.h>
#    include <immintrin.h>
#  endif
#endif

/* ── global CPU state ─────────────────────────────────────────────── */
cMDA_CPU __cMDA_CPU = EMPTY;

/* ── utilities ────────────────────────────────────────────────────── */
uint64_t htobe64(uint64_t h){
#if __BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__
    return __builtin_bswap64(h);
#else
    return h;
#endif
}
uint32_t ROTL(uint32_t x, uint8_t n){ return (x<<n)|(x>>(32-n)); }

/* ── scalar round functions (single-message path) ─────────────────── */
uint32_t F_scalar (uint32_t B,uint32_t C,uint32_t D){return (B&C)|(~B&D);}
uint32_t G4_scalar(uint32_t B,uint32_t C,uint32_t D){return (B&C)|(B&D)|(C&D);}
uint32_t G5_scalar(uint32_t B,uint32_t C,uint32_t D){return (B&D)|(C&~D);}
uint32_t H_scalar (uint32_t B,uint32_t C,uint32_t D){return B^C^D;}
uint32_t I_scalar (uint32_t B,uint32_t C,uint32_t D){return C^(B|~D);} /* fixed */

/* ── CPUID runtime detection ──────────────────────────────────────── */
void set_cpu_supported_op(void){
    __cMDA_CPU = NONE;
#ifdef CMDA_X86
    unsigned eax=0,ebx=0,ecx=0,edx=0;
#ifdef _MSC_VER
    int i4[4]; __cpuid(i4,1);
    eax=i4[0];ebx=i4[1];ecx=i4[2];edx=i4[3];
#else
    __get_cpuid(1,&eax,&ebx,&ecx,&edx);
#endif
    if(!(edx&(1u<<26))) return; /* SSE2 required */
    __cMDA_CPU = SSE2;
    if(!((ecx&(1u<<28))&&(ecx&(1u<<27)))) return; /* AVX+OSXSAVE */
    unsigned xeax=0,xedx=0;
#ifdef _MSC_VER
    {unsigned long long xv=_xgetbv(0);xeax=(unsigned)xv;xedx=(unsigned)(xv>>32);}
#else
    __asm__ volatile("xgetbv":"=a"(xeax),"=d"(xedx):"c"(0));
#endif
    if((xeax&6)!=6) return; /* OS YMM save */
#ifdef _MSC_VER
    __cpuidex(i4,7,0); ebx=i4[1];
#else
    __get_cpuid_count(7,0,&eax,&ebx,&ecx,&edx);
#endif
    if(!(ebx&(1u<<5))) return; /* AVX2 */
    __cMDA_CPU = AVX2;
    if(!(ebx&(1u<<16))) return; /* AVX-512F */
    if((xeax&0xe6)!=0xe6) return; /* OS ZMM save */
    __cMDA_CPU = AVX512F;
#endif
}

/* ── MD5 T constants (floor(2^32*|sin(i)|), i=1..64) ────────────── */
static const uint32_t T[65]={0,
 0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
 0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
 0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
 0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
 0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
 0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
 0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
 0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};

/* ── padding (Merkle-Damgård, same for MD4 & MD5) ────────────────── */
typedef struct{uint8_t*data;uint64_t nb;} PM; /* nb = number of 64-byte blocks */
static PM md_pad(const uint8_t*m,uint64_t len){
    uint64_t bits=len*8, shy=bits%512;
    uint64_t rem=(shy<448)?(448-shy):(512-shy+448);
    uint64_t N=(bits+rem+64)/8;
    PM p; p.data=(uint8_t*)malloc(N); p.nb=N/64;
    memset(p.data,0,N); memcpy(p.data,m,len);
    p.data[len]=0x80;
    for(int i=0;i<8;i++) p.data[N-8+i]=(uint8_t)((bits>>(i*8))&0xFF);
    return p;
}
static inline uint32_t rl32(const uint8_t*p){
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}

/* ═══════════════════════════════════════════════════════════════════ */
/* SSE2 — 4-lane multi-buffer (each lane = one independent message)  */
/* ═══════════════════════════════════════════════════════════════════ */
#if defined(CMDA_X86)
/* Round functions: each operates on 4 messages simultaneously */
#define VF4(b,c,d) _mm_or_si128(_mm_and_si128(b,c),_mm_andnot_si128(b,d))
#define VG4(b,c,d) _mm_or_si128(_mm_and_si128(b,d),_mm_andnot_si128(d,c))
#define VH4(b,c,d) _mm_xor_si128(_mm_xor_si128(b,c),d)
#define VI4(b,c,d) _mm_xor_si128(c,_mm_or_si128(b,_mm_andnot_si128(d,_mm_set1_epi32(-1))))
#define VMAJ4(b,c,d) _mm_or_si128(_mm_or_si128(_mm_and_si128(b,c),_mm_and_si128(b,d)),_mm_and_si128(c,d))
#define ROT4(v,n) _mm_or_si128(_mm_slli_epi32(v,n),_mm_srli_epi32(v,32-(n)))
#define ADD4(a,b) _mm_add_epi32(a,b)
#define SET4(x)   _mm_set1_epi32((int)(x))
/* MD5 step */
#define MS4(a,b,c,d,k,s,t,FN) a=ADD4(ADD4(FN(b,c,d),ADD4(a,ADD4(X[k],SET4(t)))),SET4(0)); a=ADD4(ROT4(a,s),b);
/* MD4 step */
#define M4S4(a,b,c,d,k,s,z,FN) a=ROT4(ADD4(ADD4(ADD4(a,FN(b,c,d)),X[k]),SET4(z)),s);

__attribute__((target("sse2,sse4.1")))
static void md5_compress_x4(__m128i*pA,__m128i*pB,__m128i*pC,__m128i*pD,__m128i X[16]){
    __m128i A=*pA,B=*pB,C=*pC,D=*pD,AA=A,BB=B,CC=C,DD=D;
    MS4(A,B,C,D, 0, 7,T[ 1],VF4) MS4(D,A,B,C, 1,12,T[ 2],VF4) MS4(C,D,A,B, 2,17,T[ 3],VF4) MS4(B,C,D,A, 3,22,T[ 4],VF4)
    MS4(A,B,C,D, 4, 7,T[ 5],VF4) MS4(D,A,B,C, 5,12,T[ 6],VF4) MS4(C,D,A,B, 6,17,T[ 7],VF4) MS4(B,C,D,A, 7,22,T[ 8],VF4)
    MS4(A,B,C,D, 8, 7,T[ 9],VF4) MS4(D,A,B,C, 9,12,T[10],VF4) MS4(C,D,A,B,10,17,T[11],VF4) MS4(B,C,D,A,11,22,T[12],VF4)
    MS4(A,B,C,D,12, 7,T[13],VF4) MS4(D,A,B,C,13,12,T[14],VF4) MS4(C,D,A,B,14,17,T[15],VF4) MS4(B,C,D,A,15,22,T[16],VF4)
    MS4(A,B,C,D, 1, 5,T[17],VG4) MS4(D,A,B,C, 6, 9,T[18],VG4) MS4(C,D,A,B,11,14,T[19],VG4) MS4(B,C,D,A, 0,20,T[20],VG4)
    MS4(A,B,C,D, 5, 5,T[21],VG4) MS4(D,A,B,C,10, 9,T[22],VG4) MS4(C,D,A,B,15,14,T[23],VG4) MS4(B,C,D,A, 4,20,T[24],VG4)
    MS4(A,B,C,D, 9, 5,T[25],VG4) MS4(D,A,B,C,14, 9,T[26],VG4) MS4(C,D,A,B, 3,14,T[27],VG4) MS4(B,C,D,A, 8,20,T[28],VG4)
    MS4(A,B,C,D,13, 5,T[29],VG4) MS4(D,A,B,C, 2, 9,T[30],VG4) MS4(C,D,A,B, 7,14,T[31],VG4) MS4(B,C,D,A,12,20,T[32],VG4)
    MS4(A,B,C,D, 5, 4,T[33],VH4) MS4(D,A,B,C, 8,11,T[34],VH4) MS4(C,D,A,B,11,16,T[35],VH4) MS4(B,C,D,A,14,23,T[36],VH4)
    MS4(A,B,C,D, 1, 4,T[37],VH4) MS4(D,A,B,C, 4,11,T[38],VH4) MS4(C,D,A,B, 7,16,T[39],VH4) MS4(B,C,D,A,10,23,T[40],VH4)
    MS4(A,B,C,D,13, 4,T[41],VH4) MS4(D,A,B,C, 0,11,T[42],VH4) MS4(C,D,A,B, 3,16,T[43],VH4) MS4(B,C,D,A, 6,23,T[44],VH4)
    MS4(A,B,C,D, 9, 4,T[45],VH4) MS4(D,A,B,C,12,11,T[46],VH4) MS4(C,D,A,B,15,16,T[47],VH4) MS4(B,C,D,A, 2,23,T[48],VH4)
    MS4(A,B,C,D, 0, 6,T[49],VI4) MS4(D,A,B,C, 7,10,T[50],VI4) MS4(C,D,A,B,14,15,T[51],VI4) MS4(B,C,D,A, 5,21,T[52],VI4)
    MS4(A,B,C,D,12, 6,T[53],VI4) MS4(D,A,B,C, 3,10,T[54],VI4) MS4(C,D,A,B,10,15,T[55],VI4) MS4(B,C,D,A, 1,21,T[56],VI4)
    MS4(A,B,C,D, 8, 6,T[57],VI4) MS4(D,A,B,C,15,10,T[58],VI4) MS4(C,D,A,B, 6,15,T[59],VI4) MS4(B,C,D,A,13,21,T[60],VI4)
    MS4(A,B,C,D, 4, 6,T[61],VI4) MS4(D,A,B,C,11,10,T[62],VI4) MS4(C,D,A,B, 2,15,T[63],VI4) MS4(B,C,D,A, 9,21,T[64],VI4)
    *pA=ADD4(AA,A); *pB=ADD4(BB,B); *pC=ADD4(CC,C); *pD=ADD4(DD,D);
}

__attribute__((target("sse2,sse4.1")))
static void md4_compress_x4(__m128i*pA,__m128i*pB,__m128i*pC,__m128i*pD,__m128i X[16]){
    __m128i A=*pA,B=*pB,C=*pC,D=*pD,AA=A,BB=B,CC=C,DD=D;
    M4S4(A,B,C,D, 0, 3,0,VF4) M4S4(D,A,B,C, 1, 7,0,VF4) M4S4(C,D,A,B, 2,11,0,VF4) M4S4(B,C,D,A, 3,19,0,VF4)
    M4S4(A,B,C,D, 4, 3,0,VF4) M4S4(D,A,B,C, 5, 7,0,VF4) M4S4(C,D,A,B, 6,11,0,VF4) M4S4(B,C,D,A, 7,19,0,VF4)
    M4S4(A,B,C,D, 8, 3,0,VF4) M4S4(D,A,B,C, 9, 7,0,VF4) M4S4(C,D,A,B,10,11,0,VF4) M4S4(B,C,D,A,11,19,0,VF4)
    M4S4(A,B,C,D,12, 3,0,VF4) M4S4(D,A,B,C,13, 7,0,VF4) M4S4(C,D,A,B,14,11,0,VF4) M4S4(B,C,D,A,15,19,0,VF4)
    M4S4(A,B,C,D, 0, 3,0x5A827999,VMAJ4) M4S4(D,A,B,C, 4, 5,0x5A827999,VMAJ4) M4S4(C,D,A,B, 8, 9,0x5A827999,VMAJ4) M4S4(B,C,D,A,12,13,0x5A827999,VMAJ4)
    M4S4(A,B,C,D, 1, 3,0x5A827999,VMAJ4) M4S4(D,A,B,C, 5, 5,0x5A827999,VMAJ4) M4S4(C,D,A,B, 9, 9,0x5A827999,VMAJ4) M4S4(B,C,D,A,13,13,0x5A827999,VMAJ4)
    M4S4(A,B,C,D, 2, 3,0x5A827999,VMAJ4) M4S4(D,A,B,C, 6, 5,0x5A827999,VMAJ4) M4S4(C,D,A,B,10, 9,0x5A827999,VMAJ4) M4S4(B,C,D,A,14,13,0x5A827999,VMAJ4)
    M4S4(A,B,C,D, 3, 3,0x5A827999,VMAJ4) M4S4(D,A,B,C, 7, 5,0x5A827999,VMAJ4) M4S4(C,D,A,B,11, 9,0x5A827999,VMAJ4) M4S4(B,C,D,A,15,13,0x5A827999,VMAJ4)
    M4S4(A,B,C,D, 0, 3,0x6ED9EBA1,VH4) M4S4(D,A,B,C, 8, 9,0x6ED9EBA1,VH4) M4S4(C,D,A,B, 4,11,0x6ED9EBA1,VH4) M4S4(B,C,D,A,12,15,0x6ED9EBA1,VH4)
    M4S4(A,B,C,D, 2, 3,0x6ED9EBA1,VH4) M4S4(D,A,B,C,10, 9,0x6ED9EBA1,VH4) M4S4(C,D,A,B, 6,11,0x6ED9EBA1,VH4) M4S4(B,C,D,A,14,15,0x6ED9EBA1,VH4)
    M4S4(A,B,C,D, 1, 3,0x6ED9EBA1,VH4) M4S4(D,A,B,C, 9, 9,0x6ED9EBA1,VH4) M4S4(C,D,A,B, 5,11,0x6ED9EBA1,VH4) M4S4(B,C,D,A,13,15,0x6ED9EBA1,VH4)
    M4S4(A,B,C,D, 3, 3,0x6ED9EBA1,VH4) M4S4(D,A,B,C,11, 9,0x6ED9EBA1,VH4) M4S4(C,D,A,B, 7,11,0x6ED9EBA1,VH4) M4S4(B,C,D,A,15,15,0x6ED9EBA1,VH4)
    *pA=ADD4(AA,A); *pB=ADD4(BB,B); *pC=ADD4(CC,C); *pD=ADD4(DD,D);
}

/* Gather 4 blocks → 16 SSE2 words */
__attribute__((target("sse2")))
static void gather_x4(__m128i X[16], const uint8_t*b0,const uint8_t*b1,const uint8_t*b2,const uint8_t*b3){
    for(int w=0;w<16;w++)
        X[w]=_mm_set_epi32((int)rl32(b3+w*4),(int)rl32(b2+w*4),(int)rl32(b1+w*4),(int)rl32(b0+w*4));
}

/* Extract lane i from __m128i via store */
__attribute__((target("sse2")))
static inline uint32_t lane4(const __m128i v, int i){
    uint32_t tmp[4]; _mm_storeu_si128((__m128i*)tmp,v); return tmp[i];
}

__attribute__((target("sse2,sse4.1")))
static void md5_group_x4(uint8_t**msgs,uint64_t*lens,uint8_t**digs){
    PM pm[4]; uint64_t mnb=UINT64_MAX;
    for(int j=0;j<4;j++){pm[j]=md_pad(msgs[j],lens[j]); if(pm[j].nb<mnb)mnb=pm[j].nb;}
    __m128i A=SET4(0x67452301),B=SET4((int)0xefcdab89),C=SET4((int)0x98badcfe),D=SET4(0x10325476);
    __m128i X[16];
    for(uint64_t b=0;b<mnb;b++){
        gather_x4(X,pm[0].data+b*64,pm[1].data+b*64,pm[2].data+b*64,pm[3].data+b*64);
        md5_compress_x4(&A,&B,&C,&D,X);
    }
    /* finish lanes that have more blocks scalar, then store */
    uint32_t sa[4],sb[4],sc[4],sd[4];
    _mm_storeu_si128((__m128i*)sa,A); _mm_storeu_si128((__m128i*)sb,B);
    _mm_storeu_si128((__m128i*)sc,C); _mm_storeu_si128((__m128i*)sd,D);
    for(int j=0;j<4;j++){
        uint32_t a=sa[j],b2=sb[j],c=sc[j],d=sd[j];
        for(uint64_t bl=mnb;bl<pm[j].nb;bl++){
            uint32_t XX[16]; for(int w=0;w<16;w++) XX[w]=rl32(pm[j].data+bl*64+w*4);
            uint32_t AA=a,BB=b2,CC=c,DD=d;
#define SC(av,bv,cv,dv,k,s,fn,z) av=ROTL(av+fn(bv,cv,dv)+XX[k]+z,s);
            SC(a,b2,c,d, 0, 7,F_scalar,T[ 1]) SC(d,a,b2,c, 1,12,F_scalar,T[ 2]) SC(c,d,a,b2, 2,17,F_scalar,T[ 3]) SC(b2,c,d,a, 3,22,F_scalar,T[ 4])
            SC(a,b2,c,d, 4, 7,F_scalar,T[ 5]) SC(d,a,b2,c, 5,12,F_scalar,T[ 6]) SC(c,d,a,b2, 6,17,F_scalar,T[ 7]) SC(b2,c,d,a, 7,22,F_scalar,T[ 8])
            SC(a,b2,c,d, 8, 7,F_scalar,T[ 9]) SC(d,a,b2,c, 9,12,F_scalar,T[10]) SC(c,d,a,b2,10,17,F_scalar,T[11]) SC(b2,c,d,a,11,22,F_scalar,T[12])
            SC(a,b2,c,d,12, 7,F_scalar,T[13]) SC(d,a,b2,c,13,12,F_scalar,T[14]) SC(c,d,a,b2,14,17,F_scalar,T[15]) SC(b2,c,d,a,15,22,F_scalar,T[16])
            SC(a,b2,c,d, 1, 5,G5_scalar,T[17]) SC(d,a,b2,c, 6, 9,G5_scalar,T[18]) SC(c,d,a,b2,11,14,G5_scalar,T[19]) SC(b2,c,d,a, 0,20,G5_scalar,T[20])
            SC(a,b2,c,d, 5, 5,G5_scalar,T[21]) SC(d,a,b2,c,10, 9,G5_scalar,T[22]) SC(c,d,a,b2,15,14,G5_scalar,T[23]) SC(b2,c,d,a, 4,20,G5_scalar,T[24])
            SC(a,b2,c,d, 9, 5,G5_scalar,T[25]) SC(d,a,b2,c,14, 9,G5_scalar,T[26]) SC(c,d,a,b2, 3,14,G5_scalar,T[27]) SC(b2,c,d,a, 8,20,G5_scalar,T[28])
            SC(a,b2,c,d,13, 5,G5_scalar,T[29]) SC(d,a,b2,c, 2, 9,G5_scalar,T[30]) SC(c,d,a,b2, 7,14,G5_scalar,T[31]) SC(b2,c,d,a,12,20,G5_scalar,T[32])
            SC(a,b2,c,d, 5, 4,H_scalar,T[33]) SC(d,a,b2,c, 8,11,H_scalar,T[34]) SC(c,d,a,b2,11,16,H_scalar,T[35]) SC(b2,c,d,a,14,23,H_scalar,T[36])
            SC(a,b2,c,d, 1, 4,H_scalar,T[37]) SC(d,a,b2,c, 4,11,H_scalar,T[38]) SC(c,d,a,b2, 7,16,H_scalar,T[39]) SC(b2,c,d,a,10,23,H_scalar,T[40])
            SC(a,b2,c,d,13, 4,H_scalar,T[41]) SC(d,a,b2,c, 0,11,H_scalar,T[42]) SC(c,d,a,b2, 3,16,H_scalar,T[43]) SC(b2,c,d,a, 6,23,H_scalar,T[44])
            SC(a,b2,c,d, 9, 4,H_scalar,T[45]) SC(d,a,b2,c,12,11,H_scalar,T[46]) SC(c,d,a,b2,15,16,H_scalar,T[47]) SC(b2,c,d,a, 2,23,H_scalar,T[48])
            SC(a,b2,c,d, 0, 6,I_scalar,T[49]) SC(d,a,b2,c, 7,10,I_scalar,T[50]) SC(c,d,a,b2,14,15,I_scalar,T[51]) SC(b2,c,d,a, 5,21,I_scalar,T[52])
            SC(a,b2,c,d,12, 6,I_scalar,T[53]) SC(d,a,b2,c, 3,10,I_scalar,T[54]) SC(c,d,a,b2,10,15,I_scalar,T[55]) SC(b2,c,d,a, 1,21,I_scalar,T[56])
            SC(a,b2,c,d, 8, 6,I_scalar,T[57]) SC(d,a,b2,c,15,10,I_scalar,T[58]) SC(c,d,a,b2, 6,15,I_scalar,T[59]) SC(b2,c,d,a,13,21,I_scalar,T[60])
            SC(a,b2,c,d, 4, 6,I_scalar,T[61]) SC(d,a,b2,c,11,10,I_scalar,T[62]) SC(c,d,a,b2, 2,15,I_scalar,T[63]) SC(b2,c,d,a, 9,21,I_scalar,T[64])
#undef SC
            a+=AA; b2+=BB; c+=CC; d+=DD;
        }
        uint32_t ABCD[4]={a,b2,c,d};
        for(int k=0;k<4;k++){
            digs[j][k*4+0]=(uint8_t)(ABCD[k]);
            digs[j][k*4+1]=(uint8_t)(ABCD[k]>>8);
            digs[j][k*4+2]=(uint8_t)(ABCD[k]>>16);
            digs[j][k*4+3]=(uint8_t)(ABCD[k]>>24);
        }
        free(pm[j].data);
    }
}

__attribute__((target("sse2,sse4.1")))
static void md4_group_x4(uint8_t**msgs,uint64_t*lens,uint8_t**digs){
    PM pm[4]; uint64_t mnb=UINT64_MAX;
    for(int j=0;j<4;j++){pm[j]=md_pad(msgs[j],lens[j]); if(pm[j].nb<mnb)mnb=pm[j].nb;}
    __m128i A=SET4(0x67452301),B=SET4((int)0xefcdab89),C=SET4((int)0x98badcfe),D=SET4(0x10325476);
    __m128i X[16];
    for(uint64_t b=0;b<mnb;b++){
        gather_x4(X,pm[0].data+b*64,pm[1].data+b*64,pm[2].data+b*64,pm[3].data+b*64);
        md4_compress_x4(&A,&B,&C,&D,X);
    }
    uint32_t sa[4],sb[4],sc[4],sd[4];
    _mm_storeu_si128((__m128i*)sa,A); _mm_storeu_si128((__m128i*)sb,B);
    _mm_storeu_si128((__m128i*)sc,C); _mm_storeu_si128((__m128i*)sd,D);
    static const uint32_t z2=0x5A827999,z3=0x6ED9EBA1;
    for(int j=0;j<4;j++){
        uint32_t a=sa[j],b2=sb[j],c=sc[j],d=sd[j];
        for(uint64_t bl=mnb;bl<pm[j].nb;bl++){
            uint32_t XX[16]; for(int w=0;w<16;w++) XX[w]=rl32(pm[j].data+bl*64+w*4);
            uint32_t AA=a,BB=b2,CC=c,DD=d;
#define S4(av,bv,cv,dv,k,s,fn,z) av=ROTL(av+fn(bv,cv,dv)+XX[k]+z,s);
            S4(a,b2,c,d, 0, 3,F_scalar,0) S4(d,a,b2,c, 1, 7,F_scalar,0) S4(c,d,a,b2, 2,11,F_scalar,0) S4(b2,c,d,a, 3,19,F_scalar,0)
            S4(a,b2,c,d, 4, 3,F_scalar,0) S4(d,a,b2,c, 5, 7,F_scalar,0) S4(c,d,a,b2, 6,11,F_scalar,0) S4(b2,c,d,a, 7,19,F_scalar,0)
            S4(a,b2,c,d, 8, 3,F_scalar,0) S4(d,a,b2,c, 9, 7,F_scalar,0) S4(c,d,a,b2,10,11,F_scalar,0) S4(b2,c,d,a,11,19,F_scalar,0)
            S4(a,b2,c,d,12, 3,F_scalar,0) S4(d,a,b2,c,13, 7,F_scalar,0) S4(c,d,a,b2,14,11,F_scalar,0) S4(b2,c,d,a,15,19,F_scalar,0)
            S4(a,b2,c,d, 0, 3,G4_scalar,z2) S4(d,a,b2,c, 4, 5,G4_scalar,z2) S4(c,d,a,b2, 8, 9,G4_scalar,z2) S4(b2,c,d,a,12,13,G4_scalar,z2)
            S4(a,b2,c,d, 1, 3,G4_scalar,z2) S4(d,a,b2,c, 5, 5,G4_scalar,z2) S4(c,d,a,b2, 9, 9,G4_scalar,z2) S4(b2,c,d,a,13,13,G4_scalar,z2)
            S4(a,b2,c,d, 2, 3,G4_scalar,z2) S4(d,a,b2,c, 6, 5,G4_scalar,z2) S4(c,d,a,b2,10, 9,G4_scalar,z2) S4(b2,c,d,a,14,13,G4_scalar,z2)
            S4(a,b2,c,d, 3, 3,G4_scalar,z2) S4(d,a,b2,c, 7, 5,G4_scalar,z2) S4(c,d,a,b2,11, 9,G4_scalar,z2) S4(b2,c,d,a,15,13,G4_scalar,z2)
            S4(a,b2,c,d, 0, 3,H_scalar,z3) S4(d,a,b2,c, 8, 9,H_scalar,z3) S4(c,d,a,b2, 4,11,H_scalar,z3) S4(b2,c,d,a,12,15,H_scalar,z3)
            S4(a,b2,c,d, 2, 3,H_scalar,z3) S4(d,a,b2,c,10, 9,H_scalar,z3) S4(c,d,a,b2, 6,11,H_scalar,z3) S4(b2,c,d,a,14,15,H_scalar,z3)
            S4(a,b2,c,d, 1, 3,H_scalar,z3) S4(d,a,b2,c, 9, 9,H_scalar,z3) S4(c,d,a,b2, 5,11,H_scalar,z3) S4(b2,c,d,a,13,15,H_scalar,z3)
            S4(a,b2,c,d, 3, 3,H_scalar,z3) S4(d,a,b2,c,11, 9,H_scalar,z3) S4(c,d,a,b2, 7,11,H_scalar,z3) S4(b2,c,d,a,15,15,H_scalar,z3)
#undef S4
            a+=AA; b2+=BB; c+=CC; d+=DD;
        }
        uint32_t ABCD[4]={a,b2,c,d};
        for(int k=0;k<4;k++){
            digs[j][k*4+0]=(uint8_t)(ABCD[k]);
            digs[j][k*4+1]=(uint8_t)(ABCD[k]>>8);
            digs[j][k*4+2]=(uint8_t)(ABCD[k]>>16);
            digs[j][k*4+3]=(uint8_t)(ABCD[k]>>24);
        }
        free(pm[j].data);
    }
}

/* ═══════════════════════════════════════════════════════════════════ */
/* AVX2 — 8-lane multi-buffer                                         */
/* ═══════════════════════════════════════════════════════════════════ */
#define VF8(b,c,d)   _mm256_or_si256(_mm256_and_si256(b,c),_mm256_andnot_si256(b,d))
#define VG8(b,c,d)   _mm256_or_si256(_mm256_and_si256(b,d),_mm256_andnot_si256(d,c))
#define VH8(b,c,d)   _mm256_xor_si256(_mm256_xor_si256(b,c),d)
#define VI8(b,c,d)   _mm256_xor_si256(c,_mm256_or_si256(b,_mm256_andnot_si256(d,_mm256_set1_epi32(-1))))
#define VMAJ8(b,c,d) _mm256_or_si256(_mm256_or_si256(_mm256_and_si256(b,c),_mm256_and_si256(b,d)),_mm256_and_si256(c,d))
#define ROT8(v,n) _mm256_or_si256(_mm256_slli_epi32(v,n),_mm256_srli_epi32(v,32-(n)))
#define ADD8(a,b) _mm256_add_epi32(a,b)
#define SET8(x)   _mm256_set1_epi32((int)(x))
#define MS8(a,b,c,d,k,s,t,FN) a=ADD8(ADD8(FN(b,c,d),ADD8(a,ADD8(X[k],SET8(t)))),SET8(0)); a=ADD8(ROT8(a,s),b);
#define M4S8(a,b,c,d,k,s,z,FN) a=ROT8(ADD8(ADD8(ADD8(a,FN(b,c,d)),X[k]),SET8(z)),s);

__attribute__((target("avx2")))
static void md5_compress_x8(__m256i*pA,__m256i*pB,__m256i*pC,__m256i*pD,__m256i X[16]){
    __m256i A=*pA,B=*pB,C=*pC,D=*pD,AA=A,BB=B,CC=C,DD=D;
    MS8(A,B,C,D, 0, 7,T[ 1],VF8) MS8(D,A,B,C, 1,12,T[ 2],VF8) MS8(C,D,A,B, 2,17,T[ 3],VF8) MS8(B,C,D,A, 3,22,T[ 4],VF8)
    MS8(A,B,C,D, 4, 7,T[ 5],VF8) MS8(D,A,B,C, 5,12,T[ 6],VF8) MS8(C,D,A,B, 6,17,T[ 7],VF8) MS8(B,C,D,A, 7,22,T[ 8],VF8)
    MS8(A,B,C,D, 8, 7,T[ 9],VF8) MS8(D,A,B,C, 9,12,T[10],VF8) MS8(C,D,A,B,10,17,T[11],VF8) MS8(B,C,D,A,11,22,T[12],VF8)
    MS8(A,B,C,D,12, 7,T[13],VF8) MS8(D,A,B,C,13,12,T[14],VF8) MS8(C,D,A,B,14,17,T[15],VF8) MS8(B,C,D,A,15,22,T[16],VF8)
    MS8(A,B,C,D, 1, 5,T[17],VG8) MS8(D,A,B,C, 6, 9,T[18],VG8) MS8(C,D,A,B,11,14,T[19],VG8) MS8(B,C,D,A, 0,20,T[20],VG8)
    MS8(A,B,C,D, 5, 5,T[21],VG8) MS8(D,A,B,C,10, 9,T[22],VG8) MS8(C,D,A,B,15,14,T[23],VG8) MS8(B,C,D,A, 4,20,T[24],VG8)
    MS8(A,B,C,D, 9, 5,T[25],VG8) MS8(D,A,B,C,14, 9,T[26],VG8) MS8(C,D,A,B, 3,14,T[27],VG8) MS8(B,C,D,A, 8,20,T[28],VG8)
    MS8(A,B,C,D,13, 5,T[29],VG8) MS8(D,A,B,C, 2, 9,T[30],VG8) MS8(C,D,A,B, 7,14,T[31],VG8) MS8(B,C,D,A,12,20,T[32],VG8)
    MS8(A,B,C,D, 5, 4,T[33],VH8) MS8(D,A,B,C, 8,11,T[34],VH8) MS8(C,D,A,B,11,16,T[35],VH8) MS8(B,C,D,A,14,23,T[36],VH8)
    MS8(A,B,C,D, 1, 4,T[37],VH8) MS8(D,A,B,C, 4,11,T[38],VH8) MS8(C,D,A,B, 7,16,T[39],VH8) MS8(B,C,D,A,10,23,T[40],VH8)
    MS8(A,B,C,D,13, 4,T[41],VH8) MS8(D,A,B,C, 0,11,T[42],VH8) MS8(C,D,A,B, 3,16,T[43],VH8) MS8(B,C,D,A, 6,23,T[44],VH8)
    MS8(A,B,C,D, 9, 4,T[45],VH8) MS8(D,A,B,C,12,11,T[46],VH8) MS8(C,D,A,B,15,16,T[47],VH8) MS8(B,C,D,A, 2,23,T[48],VH8)
    MS8(A,B,C,D, 0, 6,T[49],VI8) MS8(D,A,B,C, 7,10,T[50],VI8) MS8(C,D,A,B,14,15,T[51],VI8) MS8(B,C,D,A, 5,21,T[52],VI8)
    MS8(A,B,C,D,12, 6,T[53],VI8) MS8(D,A,B,C, 3,10,T[54],VI8) MS8(C,D,A,B,10,15,T[55],VI8) MS8(B,C,D,A, 1,21,T[56],VI8)
    MS8(A,B,C,D, 8, 6,T[57],VI8) MS8(D,A,B,C,15,10,T[58],VI8) MS8(C,D,A,B, 6,15,T[59],VI8) MS8(B,C,D,A,13,21,T[60],VI8)
    MS8(A,B,C,D, 4, 6,T[61],VI8) MS8(D,A,B,C,11,10,T[62],VI8) MS8(C,D,A,B, 2,15,T[63],VI8) MS8(B,C,D,A, 9,21,T[64],VI8)
    *pA=ADD8(AA,A); *pB=ADD8(BB,B); *pC=ADD8(CC,C); *pD=ADD8(DD,D);
}

__attribute__((target("avx2")))
static void gather_x8(__m256i X[16],const uint8_t*b[8]){
    for(int w=0;w<16;w++)
        X[w]=_mm256_set_epi32((int)rl32(b[7]+w*4),(int)rl32(b[6]+w*4),(int)rl32(b[5]+w*4),(int)rl32(b[4]+w*4),
                              (int)rl32(b[3]+w*4),(int)rl32(b[2]+w*4),(int)rl32(b[1]+w*4),(int)rl32(b[0]+w*4));
}

__attribute__((target("avx2")))
static void md5_group_x8(uint8_t**msgs,uint64_t*lens,uint8_t**digs){
    PM pm[8]; uint64_t mnb=UINT64_MAX;
    for(int j=0;j<8;j++){pm[j]=md_pad(msgs[j],lens[j]); if(pm[j].nb<mnb)mnb=pm[j].nb;}
    __m256i A=SET8(0x67452301),B=SET8((int)0xefcdab89),C=SET8((int)0x98badcfe),D=SET8(0x10325476);
    __m256i X[16];
    for(uint64_t bl=0;bl<mnb;bl++){
        const uint8_t*bp[8]; for(int j=0;j<8;j++) bp[j]=pm[j].data+bl*64;
        gather_x8(X,bp); md5_compress_x8(&A,&B,&C,&D,X);
    }
    uint32_t sa[8],sb[8],sc[8],sd[8];
    _mm256_storeu_si256((__m256i*)sa,A); _mm256_storeu_si256((__m256i*)sb,B);
    _mm256_storeu_si256((__m256i*)sc,C); _mm256_storeu_si256((__m256i*)sd,D);
    for(int j=0;j<8;j++){
        /* scalar finish for extra blocks + output (reuse x4 scalar fallback inline) */
        uint32_t a=sa[j],b2=sb[j],c=sc[j],d=sd[j];
        for(uint64_t bl=mnb;bl<pm[j].nb;bl++){
            uint32_t XX[16]; for(int w=0;w<16;w++) XX[w]=rl32(pm[j].data+bl*64+w*4);
            uint32_t AA=a,BB=b2,CC=c,DD=d;
#define SC(av,bv,cv,dv,k,s,fn,z) av=ROTL(av+fn(bv,cv,dv)+XX[k]+(z),s);
            SC(a,b2,c,d, 0, 7,F_scalar,T[ 1]) SC(d,a,b2,c, 1,12,F_scalar,T[ 2]) SC(c,d,a,b2, 2,17,F_scalar,T[ 3]) SC(b2,c,d,a, 3,22,F_scalar,T[ 4])
            SC(a,b2,c,d, 4, 7,F_scalar,T[ 5]) SC(d,a,b2,c, 5,12,F_scalar,T[ 6]) SC(c,d,a,b2, 6,17,F_scalar,T[ 7]) SC(b2,c,d,a, 7,22,F_scalar,T[ 8])
            SC(a,b2,c,d, 8, 7,F_scalar,T[ 9]) SC(d,a,b2,c, 9,12,F_scalar,T[10]) SC(c,d,a,b2,10,17,F_scalar,T[11]) SC(b2,c,d,a,11,22,F_scalar,T[12])
            SC(a,b2,c,d,12, 7,F_scalar,T[13]) SC(d,a,b2,c,13,12,F_scalar,T[14]) SC(c,d,a,b2,14,17,F_scalar,T[15]) SC(b2,c,d,a,15,22,F_scalar,T[16])
            SC(a,b2,c,d, 1, 5,G5_scalar,T[17]) SC(d,a,b2,c, 6, 9,G5_scalar,T[18]) SC(c,d,a,b2,11,14,G5_scalar,T[19]) SC(b2,c,d,a, 0,20,G5_scalar,T[20])
            SC(a,b2,c,d, 5, 5,G5_scalar,T[21]) SC(d,a,b2,c,10, 9,G5_scalar,T[22]) SC(c,d,a,b2,15,14,G5_scalar,T[23]) SC(b2,c,d,a, 4,20,G5_scalar,T[24])
            SC(a,b2,c,d, 9, 5,G5_scalar,T[25]) SC(d,a,b2,c,14, 9,G5_scalar,T[26]) SC(c,d,a,b2, 3,14,G5_scalar,T[27]) SC(b2,c,d,a, 8,20,G5_scalar,T[28])
            SC(a,b2,c,d,13, 5,G5_scalar,T[29]) SC(d,a,b2,c, 2, 9,G5_scalar,T[30]) SC(c,d,a,b2, 7,14,G5_scalar,T[31]) SC(b2,c,d,a,12,20,G5_scalar,T[32])
            SC(a,b2,c,d, 5, 4,H_scalar,T[33]) SC(d,a,b2,c, 8,11,H_scalar,T[34]) SC(c,d,a,b2,11,16,H_scalar,T[35]) SC(b2,c,d,a,14,23,H_scalar,T[36])
            SC(a,b2,c,d, 1, 4,H_scalar,T[37]) SC(d,a,b2,c, 4,11,H_scalar,T[38]) SC(c,d,a,b2, 7,16,H_scalar,T[39]) SC(b2,c,d,a,10,23,H_scalar,T[40])
            SC(a,b2,c,d,13, 4,H_scalar,T[41]) SC(d,a,b2,c, 0,11,H_scalar,T[42]) SC(c,d,a,b2, 3,16,H_scalar,T[43]) SC(b2,c,d,a, 6,23,H_scalar,T[44])
            SC(a,b2,c,d, 9, 4,H_scalar,T[45]) SC(d,a,b2,c,12,11,H_scalar,T[46]) SC(c,d,a,b2,15,16,H_scalar,T[47]) SC(b2,c,d,a, 2,23,H_scalar,T[48])
            SC(a,b2,c,d, 0, 6,I_scalar,T[49]) SC(d,a,b2,c, 7,10,I_scalar,T[50]) SC(c,d,a,b2,14,15,I_scalar,T[51]) SC(b2,c,d,a, 5,21,I_scalar,T[52])
            SC(a,b2,c,d,12, 6,I_scalar,T[53]) SC(d,a,b2,c, 3,10,I_scalar,T[54]) SC(c,d,a,b2,10,15,I_scalar,T[55]) SC(b2,c,d,a, 1,21,I_scalar,T[56])
            SC(a,b2,c,d, 8, 6,I_scalar,T[57]) SC(d,a,b2,c,15,10,I_scalar,T[58]) SC(c,d,a,b2, 6,15,I_scalar,T[59]) SC(b2,c,d,a,13,21,I_scalar,T[60])
            SC(a,b2,c,d, 4, 6,I_scalar,T[61]) SC(d,a,b2,c,11,10,I_scalar,T[62]) SC(c,d,a,b2, 2,15,I_scalar,T[63]) SC(b2,c,d,a, 9,21,I_scalar,T[64])
#undef SC
            a+=AA; b2+=BB; c+=CC; d+=DD;
        }
        uint32_t ABCD[4]={a,b2,c,d};
        for(int k=0;k<4;k++){
            digs[j][k*4+0]=(uint8_t)(ABCD[k]);
            digs[j][k*4+1]=(uint8_t)(ABCD[k]>>8);
            digs[j][k*4+2]=(uint8_t)(ABCD[k]>>16);
            digs[j][k*4+3]=(uint8_t)(ABCD[k]>>24);
        }
        free(pm[j].data);
    }
}
#endif /* CMDA_X86 */

/* ═══════════════════════════════════════════════════════════════════ */
/* Public multi-message dispatchers                                    */
/* ═══════════════════════════════════════════════════════════════════ */
void cMD5_multi(uint8_t**msgs,uint64_t*lens,uint8_t**digs,uint32_t count){
    if(__cMDA_CPU==EMPTY) set_cpu_supported_op();
    uint32_t i=0;
#ifdef CMDA_X86
    if(__cMDA_CPU>=AVX2){
        for(;i+8<=count;i+=8) md5_group_x8(msgs+i,lens+i,digs+i);
    }
    if(__cMDA_CPU>=SSE2){
        for(;i+4<=count;i+=4) md5_group_x4(msgs+i,lens+i,digs+i);
    }
#endif
    /* scalar remainder */
    for(;i<count;i++){
        extern uint8_t*cMD5(uint8_t*,uint64_t,uint8_t*);
        cMD5(msgs[i],lens[i],digs[i]);
    }
}

void cMD4_multi(uint8_t**msgs,uint64_t*lens,uint8_t**digs,uint32_t count){
    if(__cMDA_CPU==EMPTY) set_cpu_supported_op();
    uint32_t i=0;
#ifdef CMDA_X86
    if(__cMDA_CPU>=SSE2){
        for(;i+4<=count;i+=4) md4_group_x4(msgs+i,lens+i,digs+i);
    }
#endif
    for(;i<count;i++){
        extern uint8_t*cMD4(uint8_t*,uint64_t,uint8_t*);
        cMD4(msgs[i],lens[i],digs[i]);
    }
}