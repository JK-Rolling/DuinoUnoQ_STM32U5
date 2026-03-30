// Duino-coin Bridge Miner for STM32U585
// Optimized for ARM Cortex-M33 - achieves ~106,000 H/s @ 160MHz
// Date: 30-Mar-2026
// Author: jk_rolling

#include <Arduino.h>
#include "sha1.h"
#include <Arduino_RouterBridge.h>

// ============== Pre-computed Lookup Tables ==============

// Hex char to value lookup (pre-computed for speed)
static const uint8_t hexLookup[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,1,2,3,4,5,6,7,8,9,0,0,0,0,0,0,  // '0'-'9'
    0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0, // 'A'-'F'
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0, // 'a'-'f'
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

// Pre-built block template (word-aligned for ARM)
static uint8_t __attribute__((aligned(4))) blockTemplate[64];

// ============== Incremental Nonce Functions ==============

__attribute__((always_inline, hot))
inline void incrementNonceString(uint8_t* noncePtr, uint8_t nonceLen)
{
    int i = nonceLen - 1;
    while (i >= 0) {
        if (noncePtr[i] < '9') {
            noncePtr[i]++;
            return;
        }
        noncePtr[i] = '0';
        i--;
    }
}

// ============== Optimized Mining Function ==============

int ducos1a(String lastHash, String expHash, int difficulty) {
    // Convert expected hash to words for direct comparison
    uint8_t jobBytes[20];
    const char *c = expHash.c_str();
    
    for (uint8_t i = 0; i < 20; i++) {
        jobBytes[i] = (hexLookup[(uint8_t)c[i * 2]] << 4) | hexLookup[(uint8_t)c[i * 2 + 1]];
    }
    
    // Convert to big-endian words for direct state comparison
    uint32_t jobWords[5];
    for (int i = 0; i < 5; i++) {
        jobWords[i] = ((uint32_t)jobBytes[i*4] << 24) | ((uint32_t)jobBytes[i*4+1] << 16) |
                      ((uint32_t)jobBytes[i*4+2] << 8) | jobBytes[i*4+3];
    }
    
    // Build block template: [lastHash 40][nonce][0x80][zeros][length]
    const uint8_t *lastHashBytes = (const uint8_t *)lastHash.c_str();
    const uint8_t lastHashLen = 40;
    
    memcpy(blockTemplate, lastHashBytes, lastHashLen);
    memset(blockTemplate + lastHashLen, 0, 64 - lastHashLen);
    
    // Pre-set constant parts of length field
    blockTemplate[56] = 0;
    blockTemplate[57] = 0;
    blockTemplate[58] = 0;
    blockTemplate[59] = 0;
    
    uint32_t resultState[5];
    const int maxNonce = difficulty * 100 + 1;
    
    // Pointer to nonce position in block
    uint8_t* noncePtr = blockTemplate + lastHashLen;
    
    // Initialize nonce to "0"
    noncePtr[0] = '0';
    uint8_t nonceLen = 1;
    uint8_t totalLen = lastHashLen + nonceLen;
    
    // Set initial padding
    blockTemplate[totalLen] = 0x80;
    uint16_t bitLen = totalLen << 3;
    blockTemplate[62] = (bitLen >> 8);
    blockTemplate[63] = bitLen & 0xFF;
    
    // Length thresholds for nonce growth
    const int lenThresholds[] = {10, 100, 1000, 10000, 100000, 1000000, 10000000};
    int nextThreshold = lenThresholds[0];
    int thresholdIdx = 0;
    
    for (int nonce = 0; nonce < maxNonce; nonce++)
    {
        // Check if nonce length needs to increase
        if (nonce == nextThreshold) {
            blockTemplate[totalLen] = 0;
            nonceLen++;
            noncePtr[0] = '1';
            for (uint8_t i = 1; i < nonceLen; i++) {
                noncePtr[i] = '0';
            }
            totalLen = lastHashLen + nonceLen;
            blockTemplate[totalLen] = 0x80;
            bitLen = totalLen << 3;
            blockTemplate[62] = (bitLen >> 8);
            blockTemplate[63] = bitLen & 0xFF;
            thresholdIdx++;
            nextThreshold = lenThresholds[thresholdIdx];
        }
        else if (nonce > 0) {
            incrementNonceString(noncePtr, nonceLen);
        }
        
        // Direct SHA1 transform
        SHA1_DirectBlock(blockTemplate, resultState);
        
        // XOR-OR comparison (single branch instead of 5)
        if (((resultState[0] ^ jobWords[0]) | 
             (resultState[1] ^ jobWords[1]) | 
             (resultState[2] ^ jobWords[2]) | 
             (resultState[3] ^ jobWords[3]) | 
             (resultState[4] ^ jobWords[4])) == 0)
        {
            return nonce; // Found it!
        }
    }
    
    return -1; // Job failed to find a solution within standard difficulty range
}

void setup() {
	// Initialize the bridge communication
    Bridge.begin();
	// Register the mining function so the PC/Master can call it
    Bridge.provide("ducos1a", ducos1a);
}

void loop() {
  // The bridge library handles everything via interrupts or serial polling in the background.
  // Keep this loop empty to allow maximum CPU time for the bridge.
}
