# cMDA

## Overview

`cMDA` is a portable, high-performance C library implementing the **MD2**, **MD4**, and **MD5** message-digest algorithms. Key features:

- **Single-message API** — `cMD2`, `cMD4`, `cMD5`: always scalar, zero overhead.
- **Multi-message SIMD API** — `cMD4_multi` / `cMD5_multi`: processes N independent messages **in parallel** across SIMD lanes.  The correct multi-buffer technique is used — each lane carries a separate message, so every intrinsic does real work:
  | Backend | Register width | Messages in parallel |
  |---|---|---|
  | Scalar fallback | — | 1 |
  | SSE2 | 128-bit | 4 |
  | AVX2 | 256-bit | 8 |
  | AVX-512F | 512-bit | 16 |
  The best backend is selected **at runtime** via CPUID — no recompilation needed.
- **Fragment builds** — link only what you need: `libcMD2.a`, `libcMD4.a`, `libcMD5.a`.
- **Cross-platform** — Linux, macOS, Windows (MinGW/MSYS2), **and embedded** (AVR, ESP32, ESP8266, STM32). Embedded targets automatically use the scalar path.
- **CLI tools** — `md2`, `md4`, `md5` binaries for hashing messages and files, with hash comparison support.

---

## Installation

### From Source

```sh
git clone https://github.com/Byte-Ocelots/cMDA
cd cMDA
make clean all
```

#### Install — all (libs + headers + CLI)
```sh
# Linux (default prefix /usr/local)
make install-all
make install-all PREFIX=/your/path

# macOS (default /usr/local/Byte-Ocelots)
make install-all INSTALL_DIR=/your/path

# Windows (default C:\Byte-Ocelots)
make install-all INSTALL_DIR="D:\MyLibs"
```

#### Install — libraries only (no CLI)
```sh
make install-libs-only
```

#### Install — CLI only
```sh
make install-cli-only
```

#### Uninstall
```sh
make uninstall
```

### Pre-built Releases

Download from the [Releases](../../releases) page. Each release provides archives for:

| Platform | Architectures | Variants |
|---|---|---|
| Linux | x86_64, i686 | libs-only, cli-only, all |
| macOS | x86_64, arm64 | libs-only, cli-only, all |
| Windows | x86_64, i686 | libs-only, cli-only, all |
| AVR | 8-bit (avr-gcc) | libs-only |
| STM32 | Cortex-M4 (arm-none-eabi) | libs-only |

---

## Usage

### Library API

```c
#include <cMDA/all.h>   // all three, or include individual headers
#include <cMDA/md5.h>

// Single message
uint8_t digest[MD5_DIGEST_LENGTH];
cMD5((uint8_t *)"hello", 5, digest);

// Multiple messages in parallel (SIMD-accelerated)
uint8_t *msgs[4]    = { (uint8_t*)"a", (uint8_t*)"bb", (uint8_t*)"ccc", (uint8_t*)"dddd" };
uint64_t lens[4]    = { 1, 2, 3, 4 };
uint8_t  d[4][16];
uint8_t *digs[4]    = { d[0], d[1], d[2], d[3] };
cMD5_multi(msgs, lens, digs, 4);
```

Link with:
```sh
gcc your_program.c -lcMDA -lm -o your_program
# or fragment link (MD5 only):
gcc your_program.c -lcMD5 -lm -o your_program
```

### CLI

```sh
# Hash a message
md5 "Hello, World!"

# Hash files
md5 -f file1.bin -f file2.bin

# Multiple messages at once
md5 "foo" "bar" "baz"

# Choose output format
md5 --format upper "abc"
md5 --format base64 "abc"

# Compare against a known hash (exit 0 = match, 1 = mismatch)
md5 --compare 900150983cd24fb0d6963f7d28e17f72 "abc"

# Compare using a hash stored in a file
md5 --compare-file expected.md5 "abc"
```

---

## Building

### Default (all targets)
```sh
make all
```

### Individual targets
```sh
make static          # libcMDA.a
make shared          # libcMDA.so / .dylib / .dll
make bins            # CLI binaries
make tests           # unit test binary

# Fragment libs (single-algorithm)
make lib-md5         # libcMD5.a  (static)
make shared-md5      # libcMD5.so (shared)
make lib-md2 lib-md4 lib-md5

# Run tests
./tests/bin/test_all -v
bash tests/test_cli.sh    # Linux/macOS
tests\test_cli.bat        # Windows
```

### Architecture (Linux/macOS)
```sh
make all arch=64
make all arch=32
```

### Embedded Cross-Compile
```sh
# AVR (default MCU: atmega328p)
make avr
make avr AVR_MCU=attiny85

# ESP32 (requires xtensa-esp32-elf-gcc)
make esp32

# ESP8266 (requires xtensa-lx106-elf-gcc)
make esp8266

# STM32 Cortex-M4 (requires arm-none-eabi-gcc)
make stm32
make stm32 STM32_MCU=cortex-m3
```
Output: `lib/libcMDA_avr.a`, `lib/libcMDA_stm32.a`, etc.

> **Note**: embedded targets automatically disable all SIMD code (`-DCMDA_NO_SIMD`) and use the scalar path only.

### Compiler Override
```sh
make CC=clang all
```

---

## CI / CD

- **Every push / PR** → `build-and-test.yml` runs on Linux (x86_64 + i686), macOS (x86_64 + arm64), and Windows (x86_64 + i686), plus AVR and STM32 cross-compile checks.
- **Version tag push** (`v*.*.*`) → `release-on-tag.yml` builds and attaches all release archives automatically.

---

## License

MIT — see [LICENSE](./LICENSE).

---

## RFC References

- **MD2** — [RFC 1319](https://www.rfc-editor.org/rfc/rfc1319): *The MD2 Message-Digest Algorithm* — R. Rivest, April 1992.
- **MD4** — [RFC 1320](https://www.rfc-editor.org/rfc/rfc1320): *The MD4 Message-Digest Algorithm* — R. Rivest, April 1992.
- **MD5** — [RFC 1321](https://www.rfc-editor.org/rfc/rfc1321): *The MD5 Message-Digest Algorithm* — R. Rivest, April 1992.
