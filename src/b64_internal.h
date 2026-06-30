/*
Internal (non-public) declarations for libb64. The public block functions
dispatch to a SIMD bulk path when one is available and applicable, then fall
back to these scalar cores. Exposed so the test suite can assert the SIMD
path is byte-identical to the scalar one. Not installed; not an API.
*/

#ifndef B64_INTERNAL_H
#define B64_INTERNAL_H

#include <stddef.h>

/* ---- compiler portability, shared by the scalar and SIMD paths ----

   restrict spelling across C/C++ and compilers. */
#if defined(__cplusplus) || defined(_MSC_VER)
#  define B64_RESTRICT __restrict
#else
#  define B64_RESTRICT restrict
#endif

/* SIMD dispatch abstraction. The x86 kernels compile every ISA variant into
   one TU and pick at runtime; these macros hide the three GNU-isms so cl.exe
   can build the same kernels (clang-cl keeps the GNU path).

   - B64_TARGET_*    : per-function ISA opt-in. GNU/Clang need it to legally
                       emit AVX2/SSE intrinsics; cl.exe allows the intrinsics
                       unconditionally, so it expands to nothing.
   - b64_cpu_has_*   : runtime feature test (cpu_features.c on cl.exe).
   - B64_DISPATCH_VOLATILE / B64_ATOMIC_*_PTR : race-free publish of the
                       resolved kernel pointer. The resolve race is benign (all
                       threads store the same value, pointer stores are atomic
                       on x86-64/ARM64); relaxed atomics on GNU/Clang, a
                       volatile-qualified plain access on cl.exe. */
#if defined(_MSC_VER) && !defined(__clang__)
int b64_cpu_has_sse41(void);
int b64_cpu_has_avx2(void);
#  define B64_TARGET_SSE41
#  define B64_TARGET_AVX2
#  define B64_DISPATCH_VOLATILE volatile
#  define B64_ATOMIC_LOAD_PTR(p)     (p)
#  define B64_ATOMIC_STORE_PTR(p, v) ((p) = (v))
#else
#  define b64_cpu_has_sse41() __builtin_cpu_supports("sse4.1")
#  define b64_cpu_has_avx2()  __builtin_cpu_supports("avx2")
#  define B64_TARGET_SSE41 __attribute__((target("sse4.1")))
#  define B64_TARGET_AVX2  __attribute__((target("avx2")))
#  define B64_DISPATCH_VOLATILE
#  define B64_ATOMIC_LOAD_PTR(p)     __atomic_load_n(&(p), __ATOMIC_RELAXED)
#  define B64_ATOMIC_STORE_PTR(p, v) __atomic_store_n(&(p), (v), __ATOMIC_RELAXED)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <b64/cencode.h>
#include <b64/cdecode.h>

size_t base64_encode_block_scalar(const void* plaintext_in, size_t length_in,
                                  char* code_out, base64_encodestate* state_in);
size_t base64_decode_block_scalar(const char* code_in, size_t length_in,
                                  void* plaintext_out,
                                  base64_decodestate* state_in);

/* SIMD bulk encode of whole 3-byte groups; returns input bytes consumed
   (a multiple of 3), or 0 if no SIMD path is available/used. */
size_t base64_encode_bulk_simd(const unsigned char* src, size_t len,
                               char* dst);

/* SIMD bulk decode of clean 4-char groups; returns input chars consumed
   (a multiple of 4), stopping before the first group containing a
   non-alphabet byte. 0 if no SIMD path is available/used. */
size_t base64_decode_bulk_simd(const char* src, size_t len, void* dst);

#ifdef __cplusplus
}
#endif

#endif /* B64_INTERNAL_H */
