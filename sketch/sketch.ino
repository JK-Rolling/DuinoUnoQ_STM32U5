// Duino-coin Hardware HASH Miner for STM32U585
// Optimized for Arduino Uno Q - Reaches ~830 kH/s
// Strategy: Manual FIFO feeding + Instruction Pipelining + Loop Unrolling

#include <Arduino.h>
#include <Arduino_RouterBridge.h>
#include "stm32u5xx.h"

/**
 * Perform Duino-Coin SHA-1 Mining
 * @param lastHash: The previous block hash
 * @param expHash: The target hash to find
 * @param difficulty: Mining difficulty (determines max nonce)
 */
__attribute__((optimize("O2")))
//__attribute__((section(".ramfunc")))
int ducos1a(String lastHash, String expHash, int difficulty) {
    uint32_t jobWords[5];
    for (int i = 0; i < 5; i++) {
        String chunk = expHash.substring(i * 8, (i * 8) + 8);
        jobWords[i] = strtoul(chunk.c_str(), NULL, 16);
    }

    // Lock the first target word directly into an ARM CPU core register
    register uint32_t target0 = jobWords[0]; 

    uint32_t buffer[25] = {0}; 
    uint8_t* payload = (uint8_t*)buffer;
    
    int lastHashLen = lastHash.length();
    memcpy(payload, lastHash.c_str(), lastHashLen);

    uint8_t* noncePtr = payload + lastHashLen;
    int nonceLen = 1;
    noncePtr[0] = '0';

    uint32_t totalLen = lastHashLen + nonceLen;
    uint32_t numWords = (totalLen + 3) / 4;
    uint32_t nblw = (totalLen % 4) * 8;

    const int lenThresholds[] = {10, 100, 1000, 10000, 100000, 1000000, 10000000};
    int nextThreshold = lenThresholds[0];
    int thresholdIdx = 0;
    const int maxNonce = difficulty * 100 + 1;

    __HAL_RCC_HASH_CLK_ENABLE();
    
    volatile uint32_t* const hash_din = &(HASH->DIN);
    const uint32_t* __restrict buf = buffer;

    int nonce = 1;
    
    // Outer loop purely for handling threshold (length) changes
    while (nonce < maxNonce) {
        
        // Define how far the inner loop can run before the string length changes
        int currentLimit = nextThreshold;
        if (currentLimit > maxNonce) currentLimit = maxNonce;

        // ==========================================
        // TIGHT LOOP A: FOR 12 WORDS (45-48 bytes)
        // ==========================================
        if (numWords == 12) {
            uint32_t limit_unrolled = currentLimit - 1; 
            
            // CACHE TRICK: The first 6 words contain the 'lastHash' and NEVER change.
            // Loading them here forces the compiler to hold them in the CPU core 
            // registers, entirely eliminating SRAM fetch latency for these words.
            const uint32_t w0 = buf[0], w1 = buf[1], w2 = buf[2];
            const uint32_t w3 = buf[3], w4 = buf[4], w5 = buf[5];

            for (; nonce < limit_unrolled; ) { 
                
                // ---------------- HASH 1 ----------------
                HASH->CR = 0x24; 
                *hash_din = w0; *hash_din = w1; *hash_din = w2; *hash_din = w3;
                *hash_din = w4; *hash_din = w5; *hash_din = buf[6]; *hash_din = buf[7];
                *hash_din = buf[8]; *hash_din = buf[9]; *hash_din = buf[10];
                HASH->STR = nblw; 
                *hash_din = buf[11];
                HASH->STR = (1 << 8) | nblw; 

                // ASCII Increment 1
                for (int i = nonceLen - 1; i >= 0; i--) {
                    if (noncePtr[i] < '9') { noncePtr[i]++; break; }
                    noncePtr[i] = '0';
                }

                while (!(HASH->SR & HASH_SR_DCIS)); 

                if (HASH->HR[0] == target0 && HASH->HR[1] == jobWords[1] && 
                    HASH->HR[2] == jobWords[2] && HASH->HR[3] == jobWords[3] && 
                    HASH->HR[4] == jobWords[4]) return nonce; 

                // ---------------- HASH 2 ----------------
                HASH->CR = 0x24; 
                *hash_din = w0; *hash_din = w1; *hash_din = w2; *hash_din = w3;
                *hash_din = w4; *hash_din = w5; *hash_din = buf[6]; *hash_din = buf[7];
                *hash_din = buf[8]; *hash_din = buf[9]; *hash_din = buf[10];
                HASH->STR = nblw; 
                *hash_din = buf[11];
                HASH->STR = (1 << 8) | nblw; 

                // ASCII Increment 2
                for (int i = nonceLen - 1; i >= 0; i--) {
                    if (noncePtr[i] < '9') { noncePtr[i]++; break; }
                    noncePtr[i] = '0';
                }
                
                // SHADOWED INTEGER INCREMENT: 
                // The CPU updates the outer loop variable while waiting for the hardware!
                nonce += 2; 

                while (!(HASH->SR & HASH_SR_DCIS)); 

                if (HASH->HR[0] == target0 && HASH->HR[1] == jobWords[1] && 
                    HASH->HR[2] == jobWords[2] && HASH->HR[3] == jobWords[3] && 
                    HASH->HR[4] == jobWords[4]) return nonce - 1; 
            }
            
            // ---------------- CATCH-UP ----------------
            // Handles the final hash if the threshold limit was an odd number
            for (; nonce < currentLimit; nonce++) {
                HASH->CR = 0x24; 
                *hash_din = w0; *hash_din = w1; *hash_din = w2; *hash_din = w3;
                *hash_din = w4; *hash_din = w5; *hash_din = buf[6]; *hash_din = buf[7];
                *hash_din = buf[8]; *hash_din = buf[9]; *hash_din = buf[10];
                HASH->STR = nblw; 
                *hash_din = buf[11];
                HASH->STR = (1 << 8) | nblw; 

                for (int i = nonceLen - 1; i >= 0; i--) {
                    if (noncePtr[i] < '9') { noncePtr[i]++; break; }
                    noncePtr[i] = '0';
                }
                while (!(HASH->SR & HASH_SR_DCIS)); 
                if (HASH->HR[0] == target0 && HASH->HR[1] == jobWords[1] && 
                    HASH->HR[2] == jobWords[2] && HASH->HR[3] == jobWords[3] && 
                    HASH->HR[4] == jobWords[4]) return nonce; 
            }
        }
        // ==========================================
        // TIGHT LOOP B: FOR 13 WORDS (49-52 bytes)
        // ==========================================
        else if (numWords == 13) {
            for (; nonce < currentLimit; nonce++) {
                HASH->CR = 0x24; 
                
                *hash_din = buf[0]; *hash_din = buf[1]; *hash_din = buf[2]; *hash_din = buf[3];
                *hash_din = buf[4]; *hash_din = buf[5]; *hash_din = buf[6]; *hash_din = buf[7];
                *hash_din = buf[8]; *hash_din = buf[9]; *hash_din = buf[10]; *hash_din = buf[11];
                HASH->STR = nblw; 
                *hash_din = buf[12];
                HASH->STR = (1 << 8) | nblw; 

                for (int i = nonceLen - 1; i >= 0; i--) {
                    if (noncePtr[i] < '9') { noncePtr[i]++; break; }
                    noncePtr[i] = '0';
                }

                while (!(HASH->SR & HASH_SR_DCIS)); 

                if (HASH->HR[0] == target0) {
                    if (HASH->HR[1] == jobWords[1] && HASH->HR[2] == jobWords[2] && 
                        HASH->HR[3] == jobWords[3] && HASH->HR[4] == jobWords[4]) return nonce; 
                }
            }
        } 
        // ==========================================
        // TIGHT LOOP C: FALLBACK
        // ==========================================
        else {
            for (; nonce < currentLimit; nonce++) {
                HASH->CR = 0x24; 
                for (uint32_t i = 0; i < numWords - 1; i++) *hash_din = buf[i]; 
                HASH->STR = nblw; 
                *hash_din = buf[numWords - 1]; 
                HASH->STR = (1 << 8) | nblw; 

                for (int i = nonceLen - 1; i >= 0; i--) {
                    if (noncePtr[i] < '9') { noncePtr[i]++; break; }
                    noncePtr[i] = '0';
                }

                while (!(HASH->SR & HASH_SR_DCIS)); 

                if (HASH->HR[0] == target0) {
                    if (HASH->HR[1] == jobWords[1] && HASH->HR[2] == jobWords[2] && 
                        HASH->HR[3] == jobWords[3] && HASH->HR[4] == jobWords[4]) return nonce; 
                }
            }
        }

        // --- THE THRESHOLD HAS CROSSED (e.g., 99 to 100) ---
        // This only runs a few times per job. We update our hardware counts and loop again.
        if (nonce == nextThreshold) {
            nonceLen++;
            noncePtr[0] = '1';
            for (int i = 1; i < nonceLen; i++) noncePtr[i] = '0';
            
            totalLen = lastHashLen + nonceLen;
            numWords = (totalLen + 3) / 4;
            nblw = (totalLen % 4) * 8;
            nextThreshold = lenThresholds[++thresholdIdx];
        }
    }
    
    return -1; 
}

void configureCache(void) {
    // 1. Disable cache just in case it's in a weird state
    ICACHE->CR &= ~ICACHE_CR_EN;
    // 2. Erase the random boot garbage from the cache memory
    ICACHE->CR |= ICACHE_CR_CACHEINV;
    // 3. Wait for the silicon to finish erasing (Crucial!)
    while(ICACHE->SR & ICACHE_SR_BUSYF); 
    // 4. Enable 2-way associative mode and power it on
    ICACHE->CR |= ICACHE_CR_WAYSEL;
    ICACHE->CR |= ICACHE_CR_EN;
    // 5. ARM Assembly Barriers: Force the CPU to flush its pipeline and sync with the new cache
    __DSB(); // Data Synchronization Barrier
    __ISB(); // Instruction Synchronization Barrier
}

void setup() {

    FLASH->ACR |= FLASH_ACR_PRFTEN;
    configureCache();
    Bridge.begin();
    Bridge.provide("ducos1a", ducos1a);
}

void loop() {
    // Empty: Give 100% of CPU cycles to the RPC Bridge and Mining function
}