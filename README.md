# Arduino UNO Q STM32U585 - Duino-Coin Bridge Miner

High-performance SHA1 mining for Arduino UNO Q with STM32U5 (ARM Cortex-M33).

## Performance

| CPU Model | Architecture | Clock Speed | Hashrate | Tested By | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| STM32U585 (Arduino&nbsp;Uno&nbsp;Q) | ARM Cortex&#8209;M33 | 160 MHz | ~1,036,000&nbsp;H/s | Eisberg | |


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

### STM32U5 Hardware Hasher

Raw performance of the hasher is 82 cycles per 512-bits block, this translates into 1.95MH/s at 160MHz HCLK. Without considering overhead.

CycloneCrypto managed to get throughput of 74.626MB/s or 1.6MH/s. But that's not how Duco algo works where CPU need to check nonce from each digest, so just fyi.

### Mining Algorithm

```
For each nonce from 0 to (difficulty × 100):
    hash = SHA1(lastHash + nonce_string)
    if hash == expectedHash:
        return nonce
```

## Performance Comparison

| Implementation | Hashrate | Improvement |
|----------------|----------|-------------|
| Original (context-based) | ~32,000 H/s | baseline |
| Optimized (direct block) | ~1,036,000 H/s | **+3200%** |

## Author

Original author: Eisberg


## License

All refers back to original [Duino-Coin licensee and terms of service](https://github.com/revoxhere/duino-coin)
