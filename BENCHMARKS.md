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
pseudo-random binary input:

| Input size | Encode | Decode | Decode (76-col wrapped) |
|-----------:|-------:|-------:|------------------------:|
| 64 B       | 508    | 359    | 388 |
| 256 B      | 553    | 444    | 409 |
| 4 KiB      | 577    | 462    | 409 |
| 64 KiB     | 564    | 443    | 433 |
| 1 MiB      | 563    | 441    | 423 |
| 16 MiB     | 584    | 427    | 413 |
| 256 MiB    | 542    | 433    | 413 |
| 1 GiB      | 366*   | 391    | 407 |

## Observations

- **Peak ~580 MB/s encode, ~460 MB/s decode** on this box; encode is
  consistently a little faster than decode.
- **Sweet spot is L1/L2-resident buffers** (4–64 KiB). Very small inputs
  pay a fixed per-call cost; beyond the cache size throughput settles into
  a memory-bound regime (~420–540 MB/s).
- **Data-independent.** Across binary, ASCII-text and all-zero inputs the
  throughput agreed within ~15% — base64 is a fixed per-byte transform
  with no data-dependent branches on valid input.
- **MIME line-wrapped input** makes the decoder skip a newline every 76
  chars; in practice it tracks plain decode within a few percent (the
  newline test is a cheap `< '+'` rejection), staying ~410–430 MB/s.
- \* The 1 GiB figures are single-iteration and dominated by memory
  bandwidth/TLB effects, so they are noisier (text and zeros encode at
  ~520–560 MB/s at the same size); treat them as ballpark.

These are indicative numbers from one developer machine, not an
authoritative cross-platform comparison.

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
