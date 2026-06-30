/*
Internal (non-public) declarations for libb64. The public block functions
dispatch to a SIMD bulk path when one is available and applicable, then fall
back to these scalar cores. Exposed so the test suite can assert the SIMD
path is byte-identical to the scalar one. Not installed; not an API.
*/

#ifndef B64_INTERNAL_H
#define B64_INTERNAL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <b64/cencode.h>

size_t base64_encode_block_scalar(const void* plaintext_in, size_t length_in,
                                  char* code_out, base64_encodestate* state_in);

/* SIMD bulk encode of whole 3-byte groups; returns input bytes consumed
   (a multiple of 3), or 0 if no SIMD path is available/used. */
size_t base64_encode_bulk_simd(const unsigned char* src, size_t len,
                               char* dst);

#ifdef __cplusplus
}
#endif

#endif /* B64_INTERNAL_H */
