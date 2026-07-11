libb64: Base64 Encoding/Decoding Routines
======================================

## Changelog ##

Unreleased
----------
* Build: the repo-root `VERSION` file is renamed `VERSION.txt`. A bare `VERSION` shadows the C++ `<version>` header on case-insensitive filesystems (macOS) when the repo root lands on an include path

Version 2.1.0 Release
---------------------
* Optional customer-provided allocator (b64/alloc.h): a single realloc-style callback `void *(void *ctx, void *ptr, size_t size, b64_memlife life)` carrying an opaque context pointer and a short/long lifetime hint. New one-shot C helpers base64_encode_alloc / base64_decode_alloc allocate the output for you (B64_MEM_LONG, freed via base64_free); base64_allocator(fn, ctx) builds an allocator from C. The C++ stream wrappers are now configured via base64::encoder_builder / base64::decoder_builder (with a realloc setter) and route their scratch buffers through it (B64_MEM_SHORT); the direct encoder/decoder constructors are deprecated in favour of the builders. A NULL allocator falls back to the C library's realloc/free; the core block API stays zero-allocation
* SIMD-accelerated encode/decode behind the existing API: x86 SSE4.1/AVX2 and ARM NEON (aarch64 + ARMv7-A), runtime-dispatched, with a portable scalar fallback — up to ~20x faster (AVX2). No public API, ABI, streaming, line-wrapping or decoder-tolerance changes
* MSVC (cl.exe) builds now use the SSE4.1/AVX2 and NEON kernels too, via a CPUID/baseline dispatch, instead of falling back to the scalar core
* Faster portable scalar core (the SIMD fallback and residue path): a 12-bit dual-char encode table and a four-table SWAR decoder — ~3x encode / ~5x decode over the original byte-at-a-time code, even with SIMD disabled. Decode re-engages SIMD across MIME line wrapping
* Fix a 1-byte heap overflow: base64_decode_maxlength under-sized the output buffer by one for input lengths ≡ 3 (mod 4), so the decoder's speculative store could write one byte past an exactly-sized buffer. Also harden base64_encode_length's overflow guard near SIZE_MAX and base64_encode_value against a negative-index read, and the base64 CLI now validates its mode before truncating the output and refuses identical input/output paths
* CMake now defaults to a Release build when no build type is set
* C++ stream wrappers size their output buffer with base64_encode_length / base64_decode_maxlength instead of a fixed 2*N / N — less memory, and fixes an overflow when line wrapping with a narrow width
* Single top-level VERSION file is the authoritative version; b64/version.h is generated from it (BASE64_VERSION_MAJOR/MINOR/PATCH/STRING) and the existing per-header version macros now derive from it. Public headers also install under include/b64/ so <b64/...> includes resolve post-install

Version 2.0.0 Release
---------------------
* Introduce version macros for detection of incompatible API / version
* size_t as argument to allow longer base64 encoded strings
* re-introduce line break functionality this time with line with configurable
* add flags field for encoder to mage it configureable (currently unused)
* add functions to calculate required output buffer maximum lengths
* change in-/out-pointers to void* as we don't need to make assumptions about kind of data

Version 1.4.1 Release
---------------------
* Fix differing prototypes in cencode.h and cdecode.h
* Fix compiler errors due to C++ style "//" comments and `-pedantic` option on gcc

Version 1.4.0 Release
---------------------
* add ARM compatibility by Harry Rostovtsev
* Fix integer overflows in decoder by Jakub Wilk
* Make Visual studio project compile again, use Visual Studio 2013
* switch to warning level 4 and get rid of warnings
* init encoderstate on instantiation to make `encode()` work out of the box
* Make project compile with x64 compiler

Version 1.3.0 Release
---------------------
* Remove newlines in output because json doesn't allow them in string values.

Version 1.2.1 Release
---------------------
* Fixed a long-standing bug in `src/cdecode.c` where `value_in` was not correctly checked against the bounds [0..decoding_size) Thanks to both Mario Rugiero and Shlok Datye for pointing this out.
* Added some simple example code to answer some of the most common misconceptions people have about the library usage.

Version 1.2 Release
-------------------
* Removed the `b64dec`, `b64enc`, encoder and decoder programs in favour of a better example, called `base64`, which encodes and decodes depending on its arguments.
* Created a solution for Microsoft Visual Studio C++ Express 2010 edition, which simply builds the base64 example as a console application.

Version 1.1 Release
-------------------
* Modified `encode.h` to (correctly) read from the `iostream` argument, instead of `std::cin`. Thanks to Peter K. Lee for the heads-up.
* No API changes.

Version 1.0 Release
-------------------
* The current content is the changeset.
