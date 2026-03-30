/**
 * @file sha1.h
 * @brief Optimized SHA1 implementation for ARM Cortex-M33 (STM32U5)
 * @author jk_rolling
 * 
 * Optimizations:
 * 1. ARM __ROR() intrinsic for single-cycle rotate
 * 2. ARM __REV() intrinsic for single-cycle byte reversal
 * 3. Direct block SHA1 - bypasses context management
 * 4. Fully unrolled rounds
 * 
 * Achieves ~106,000 H/s on STM32U585 @ 160MHz
 */
 
/*
   Test vector (Duino-Coin Job)
   438408604fd9e2816f54c3d58f90ce82c20397fa,f1ab0ad57b5145eea289f660ed06088516cb8f4c,128,
   
   Expected Result
   10115,<millis>,DUCOID<>
   Example: 10115,95378,DUCOID04402235435082037394c
*/

#ifndef SHA1_H
#define SHA1_H

#include <Arduino.h>
#include <cmsis_gcc.h>

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    unsigned char buffer[64];
} SHA1_CTX;

#define FORCE_INLINE __attribute__((always_inline)) inline

// ARM Cortex-M33 optimized rotate-left: ROL(x,n) = ROR(x, 32-n)
#define ROL_ASM(value, bits) __ROR((value), (32 - (bits)))

// ARM Cortex-M33 single-cycle byte reversal
#define BSWAP32(x) __REV(x)

// SHA1 round constants
#define K0 0x5A827999
#define K1 0x6ED9EBA1
#define K2 0x8F1BBCDC
#define K3 0xCA62C1D6

// Block expansion macros
#define BLK0(i) (block[i] = BSWAP32(block[i]))
#define BLK(i) (block[(i) & 15] = ROL_ASM(block[((i) + 13) & 15] ^ block[((i) + 8) & 15] ^ block[((i) + 2) & 15] ^ block[(i) & 15], 1))

// Round function macros
#define F0(b, c, d) (((b) & ((c) ^ (d))) ^ (d))
#define F1(b, c, d) ((b) ^ (c) ^ (d))
#define F2(b, c, d) ((((b) | (c)) & (d)) | ((b) & (c)))
#define F3(b, c, d) ((b) ^ (c) ^ (d))

#define ROUND0(a, b, c, d, e, i) \
    (e) += F0(b, c, d) + BLK0(i) + K0 + ROL_ASM(a, 5); \
    (b) = ROL_ASM(b, 30)

#define ROUND1(a, b, c, d, e, i) \
    (e) += F0(b, c, d) + BLK(i) + K0 + ROL_ASM(a, 5); \
    (b) = ROL_ASM(b, 30)

#define ROUND2(a, b, c, d, e, i) \
    (e) += F1(b, c, d) + BLK(i) + K1 + ROL_ASM(a, 5); \
    (b) = ROL_ASM(b, 30)

#define ROUND3(a, b, c, d, e, i) \
    (e) += F2(b, c, d) + BLK(i) + K2 + ROL_ASM(a, 5); \
    (b) = ROL_ASM(b, 30)

#define ROUND4(a, b, c, d, e, i) \
    (e) += F3(b, c, d) + BLK(i) + K3 + ROL_ASM(a, 5); \
    (b) = ROL_ASM(b, 30)

/**
 * Core SHA1 transform - fully unrolled, ARM optimized
 */
FORCE_INLINE void SHA1Transform_Opt(uint32_t * __restrict__ state, const uint8_t * __restrict__ buffer)
{
    uint32_t a, b, c, d, e;
    uint32_t block[16];
    
    const uint32_t * __restrict__ src = (const uint32_t *)buffer;
    block[0] = src[0];   block[1] = src[1];   block[2] = src[2];   block[3] = src[3];
    block[4] = src[4];   block[5] = src[5];   block[6] = src[6];   block[7] = src[7];
    block[8] = src[8];   block[9] = src[9];   block[10] = src[10]; block[11] = src[11];
    block[12] = src[12]; block[13] = src[13]; block[14] = src[14]; block[15] = src[15];
    
    a = state[0]; b = state[1]; c = state[2]; d = state[3]; e = state[4];
    
    // Rounds 0-15
    ROUND0(a, b, c, d, e, 0);  ROUND0(e, a, b, c, d, 1);
    ROUND0(d, e, a, b, c, 2);  ROUND0(c, d, e, a, b, 3);
    ROUND0(b, c, d, e, a, 4);  ROUND0(a, b, c, d, e, 5);
    ROUND0(e, a, b, c, d, 6);  ROUND0(d, e, a, b, c, 7);
    ROUND0(c, d, e, a, b, 8);  ROUND0(b, c, d, e, a, 9);
    ROUND0(a, b, c, d, e, 10); ROUND0(e, a, b, c, d, 11);
    ROUND0(d, e, a, b, c, 12); ROUND0(c, d, e, a, b, 13);
    ROUND0(b, c, d, e, a, 14); ROUND0(a, b, c, d, e, 15);
    
    // Rounds 16-19
    ROUND1(e, a, b, c, d, 16); ROUND1(d, e, a, b, c, 17);
    ROUND1(c, d, e, a, b, 18); ROUND1(b, c, d, e, a, 19);
    
    // Rounds 20-39
    ROUND2(a, b, c, d, e, 20); ROUND2(e, a, b, c, d, 21);
    ROUND2(d, e, a, b, c, 22); ROUND2(c, d, e, a, b, 23);
    ROUND2(b, c, d, e, a, 24); ROUND2(a, b, c, d, e, 25);
    ROUND2(e, a, b, c, d, 26); ROUND2(d, e, a, b, c, 27);
    ROUND2(c, d, e, a, b, 28); ROUND2(b, c, d, e, a, 29);
    ROUND2(a, b, c, d, e, 30); ROUND2(e, a, b, c, d, 31);
    ROUND2(d, e, a, b, c, 32); ROUND2(c, d, e, a, b, 33);
    ROUND2(b, c, d, e, a, 34); ROUND2(a, b, c, d, e, 35);
    ROUND2(e, a, b, c, d, 36); ROUND2(d, e, a, b, c, 37);
    ROUND2(c, d, e, a, b, 38); ROUND2(b, c, d, e, a, 39);
    
    // Rounds 40-59
    ROUND3(a, b, c, d, e, 40); ROUND3(e, a, b, c, d, 41);
    ROUND3(d, e, a, b, c, 42); ROUND3(c, d, e, a, b, 43);
    ROUND3(b, c, d, e, a, 44); ROUND3(a, b, c, d, e, 45);
    ROUND3(e, a, b, c, d, 46); ROUND3(d, e, a, b, c, 47);
    ROUND3(c, d, e, a, b, 48); ROUND3(b, c, d, e, a, 49);
    ROUND3(a, b, c, d, e, 50); ROUND3(e, a, b, c, d, 51);
    ROUND3(d, e, a, b, c, 52); ROUND3(c, d, e, a, b, 53);
    ROUND3(b, c, d, e, a, 54); ROUND3(a, b, c, d, e, 55);
    ROUND3(e, a, b, c, d, 56); ROUND3(d, e, a, b, c, 57);
    ROUND3(c, d, e, a, b, 58); ROUND3(b, c, d, e, a, 59);
    
    // Rounds 60-79
    ROUND4(a, b, c, d, e, 60); ROUND4(e, a, b, c, d, 61);
    ROUND4(d, e, a, b, c, 62); ROUND4(c, d, e, a, b, 63);
    ROUND4(b, c, d, e, a, 64); ROUND4(a, b, c, d, e, 65);
    ROUND4(e, a, b, c, d, 66); ROUND4(d, e, a, b, c, 67);
    ROUND4(c, d, e, a, b, 68); ROUND4(b, c, d, e, a, 69);
    ROUND4(a, b, c, d, e, 70); ROUND4(e, a, b, c, d, 71);
    ROUND4(d, e, a, b, c, 72); ROUND4(c, d, e, a, b, 73);
    ROUND4(b, c, d, e, a, 74); ROUND4(a, b, c, d, e, 75);
    ROUND4(e, a, b, c, d, 76); ROUND4(d, e, a, b, c, 77);
    ROUND4(c, d, e, a, b, 78); ROUND4(b, c, d, e, a, 79);
    
    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

// SHA1 initial state constant
static const uint32_t SHA1_INIT[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};

/**
 * Direct single-block SHA1 - no context overhead
 */
__attribute__((always_inline, hot))
inline void SHA1_DirectBlock(const uint8_t* block, uint32_t* resultWords)
{
    resultWords[0] = SHA1_INIT[0];
    resultWords[1] = SHA1_INIT[1];
    resultWords[2] = SHA1_INIT[2];
    resultWords[3] = SHA1_INIT[3];
    resultWords[4] = SHA1_INIT[4];
    SHA1Transform_Opt(resultWords, block);
}

#endif // SHA1_H
