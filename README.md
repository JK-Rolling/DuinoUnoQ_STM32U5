# Arduino UNO Q STM32U585 - Duino-Coin Bridge Miner

High-performance SHA1 mining for Arduino UNO Q with STM32U5 (ARM Cortex-M33).

## Performance

| Metric | Value |
|--------|-------|
| **Hashrate** | ~106,000 H/s |
| **CPU** | STM32U585 @ 160 MHz |
| **Architecture** | ARM Cortex-M33 |

## Optimizations Applied

1. **Direct Block SHA1** - Bypasses all SHA1 context management overhead
2. **ARM Intrinsics** - Single-cycle `__ROR()` and `__REV()` instructions
3. **Incremental Nonce** - O(1) amortized nonce string updates
4. **XOR-OR Comparison** - Single branch instead of 5 conditional checks
5. **Pre-computed Lookup Tables** - Fast hex-to-byte conversion

## Files

| File | Description |
|------|-------------|
| `main.cpp` | Optimized mining function with Bridge integration |
| `sha1.h` | ARM Cortex-M33 optimized SHA1 implementation |
| `main.py` | PC-side mining script (communicates with board) |


## How It Works

### Bridge Architecture

```text
┌──────────────┐     Serial/USB     ┌──────────────┐
│   PC/Host    │◄──────────────────►│  Arduino UNO │
│  (main.py)   │                    │      Q       │
│              │  ducos1a(hash,     │              │
│  Pool Client │  expected, diff)   │  SHA1 Miner  │
└──────────────┘  ───────────────►  └──────────────┘
                        │
                        ▼
                  Returns nonce
```

The PC handles network communication with the Duino-Coin pool, while the STM32U5 performs the compute-intensive SHA1 mining.

### Mining Algorithm

```
For each nonce from 0 to (difficulty × 100):
    hash = SHA1(lastHash + nonce_string)
    if hash == expectedHash:
        return nonce
```

### Key Optimization: Direct Block SHA1

Instead of using SHA1 context management (Init → Update → Final), we:

1. Pre-build a 64-byte block with lastHash + padding
2. Insert nonce digits directly into the block
3. Call SHA1Transform once (no context overhead)

This eliminates ~40% of per-hash overhead.

## Performance Comparison

| Implementation | Hashrate | Improvement |
|----------------|----------|-------------|
| Original (context-based) | ~32,000 H/s | baseline |
| Optimized (direct block) | ~106,000 H/s | **+331%** |

## Author

Original author: Eisberg


## License

All refers back to original [Duino-Coin licensee and terms of service](https://github.com/revoxhere/duino-coin)