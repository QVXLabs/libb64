# libb64 microbenchmarks

The `b64-benchmark` target (built alongside the test suite, see
`benchmark/main.cpp`) measures encode/decode throughput across a range of
input sizes and data types using [Google Benchmark](https://github.com/google/benchmark).

## Running

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target b64-benchmark
./build/bin/b64-benchmark --benchmark_min_time=0.1
```

The companion `b64-gendata` tool writes a deterministic data file (any
size, `binary`/`text`/`zeros`) for ad-hoc testing or feeding the `base64`
CLI, e.g. `b64-gendata 1G binary data.bin`.

## Environment

| | |
|---|---|
| CPU      | Intel Core i9-8950HK @ 2.90 GHz |
| Compiler | Apple clang 17.0.0, `-O3` (Release) |
| Library  | Google Benchmark 1.6.1, single-threaded |
| Date     | 2026-06-30 |

## Results

Throughput in MB/s (1 MB = 10^6 bytes) of **plaintext** processed, for
pseudo-random binary input, using the runtime-dispatched best path (AVX2 on
this CPU):

| Input size | Encode | Decode | Decode (76-col wrapped) |
|-----------:|-------:|-------:|------------------------:|
| 64 B       |  2,406 |  1,448 | 1,143 |
| 256 B      |  6,835 |  4,325 | 1,771 |
| 4 KiB      | 12,900 |  7,497 | 1,955 |
| 64 KiB     | 11,834 |  9,026 | 1,905 |
| 1 MiB      | 11,542 |  9,094 | 1,939 |
| 16 MiB     |  6,624 |  4,164 | 1,562 |
| 256 MiB    |  4,036 |  4,793 | 1,376 |
| 1 GiB      | 3,573* | 1,137* | 1,625* |

### Stock vs. scalar vs. SIMD

Three builds on the same machine, measured by a single equal-conditions
tight-loop harness (5-run average, plaintext MB/s, cache-resident buffers):
**stock** is the original byte-at-a-time coroutine; **scalar** is this version's
portable table core with SIMD disabled (`base64_*_block_scalar`); **SIMD** is
the dispatched AVX2 path. (This harness has less per-call overhead than the
Google Benchmark sweep above, so its small-buffer SIMD figures run a little
higher; the sweep is the authoritative absolute SIMD detail.)

Encode:

| Input size | stock | scalar (non-SIMD) | SIMD         |
|-----------:|------:|------------------:|-------------:|
| 4 KiB      |   668 |   2,311 (3.5×)    | 12,940 (19×) |
| 64 KiB     |   664 |   2,149 (3.2×)    | 12,707 (19×) |
| 1 MiB      |   665 |   2,232 (3.4×)    | 12,387 (19×) |

Decode:

| Input size | stock | scalar (non-SIMD) | SIMD         |
|-----------:|------:|------------------:|-------------:|
| 4 KiB      |   464 |   2,444 (5.3×)    |  9,337 (20×) |
| 64 KiB     |   471 |   2,482 (5.3×)    |  9,734 (21×) |
| 1 MiB      |   465 |   2,415 (5.2×)    |  8,982 (19×) |

## Observations

- **Peak ~12.9 GB/s encode (4 KiB), ~9.1 GB/s decode (64 KiB–1 MiB)** —
  roughly 20× the original byte-at-a-time code at the L1/L2 sweet spot.
- **Two regimes.** Cache-resident buffers run compute-bound on the SIMD
  kernels; past the last-level cache (16 MiB+) throughput drops into a
  memory-bound ~4–6 GB/s, where the ceiling is DRAM bandwidth rather than
  the transform.
- **Data-independent.** Across binary, ASCII-text and all-zero inputs the
  throughput agreed within ~15% — base64 has no data-dependent branches on
  valid input, and the SIMD validate/translate stays branchless.
- **MIME line-wrapped decode (~1.9 GB/s)** is the one case the SIMD path
  can't fully accelerate: the kernel only engages on unbroken 16-byte runs,
  so a newline every 76 chars forces a scalar hand-off each line. It still
  beats the original wrapped decode (~410 MB/s) by ~4–5×.
- \* The 1 GiB figures are single-iteration and dominated by memory
  bandwidth/TLB effects, so treat them as ballpark.

These are indicative numbers from one developer machine, not an
authoritative cross-platform comparison.

## Scalar core (portable fallback)

The table-driven scalar core (the **scalar** column above) runs on targets
without a SIMD kernel and finishes the residue/dirty bytes the SIMD path hands
back. It uses a 12-bit dual-char encode table (two lookups per 3-byte triple)
and a four-32-bit-table SWAR decoder (four ORs + one sentinel test per 4-char
quad). With SIMD disabled it is **~3.3× faster on encode and ~5.2× on decode
than stock** — a worthwhile win on its own for non-x86/ARM targets and the
SIMD residue path.

Note that **MSVC (cl.exe) builds now use the SSE4.1/AVX2 and NEON kernels** via
a CPUID/baseline dispatch, where they previously fell back to this scalar core
for everything — so on Windows the SIMD column, not the scalar column, now
applies.

---

# Historical comparison (2010)

## Intro

Some people have expressed opinions about how fast libb64's encoding and decoding routines are, as compared to some other BASE64 packages out there.

This document shows the result of a short and sweet benchmark, which takes a large-ish file and encodes/decodes it a number of times.
The winner is the executable that does this task the quickest.

## Platform

The tests were all run on a Fujitsu-Siemens laptop, with a Pentium M processor running at 2 GHz, with 1 GB of RAM, running Ubuntu 10.4.

## Packages

The following BASE64 packages were used in this benchmark:

- libb64-1.2 (libb64-base64)
  From libb64.sourceforge.net
  Size of executable: 18808 bytes
  Compiled with:
    CFLAGS += -O3
    BUFFERSIZE = 16777216

- base64-1.5 (fourmilab-base64)
  From http://www.fourmilab.ch/webtools/base64/
  Size of executable: 20261 bytes
  Compiled with Default package settings

- coreutils 7.4-2ubuntu2 (coreutils-base64)
  From http://www.gnu.org/software/coreutils/
  Size of executable: 38488 bytes
  Default binary distributed with Ubuntu 10.4

## Input File

Using `blender-2.49b-linux-glibc236-py25-i386.tar.bz2` from http://www.blender.org/download/get-blender/
Size: 18285329 bytes (approx. 18 MB)

## Method

Encode and Decode the Input file 50 times in a loop, using a simple shell script, and get the running time.

## Results

    $ time ./benchmark-libb64.sh
    real    0m28.389s
    user    0m14.077s
    sys     0m12.309s

    $ time ./benchmark-fourmilab.sh
    real    1m43.160s
    user    1m23.769s
    sys     0m8.737s

    $ time ./benchmark-coreutils.sh
    real    0m36.288s
    user    0m24.746s
    sys     0m8.181s

    28.389 for 18 MB * 50
    = 28.389 for 900 MB

## Conclusion

libb64 is the fastest encoder/decoder, and has the smallest executable size.

On average it will encode and decode at roughly 31.7 MB/s.

The closest "competitor" is base64 from GNU coreutils, which reaches only 24.8 MB/s.

--
14/06/2010
chris.venter@gmail.com
