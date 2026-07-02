/*
alloc.h - optional customer-provided allocator for libb64.

The core block API (cencode.h / cdecode.h) is zero-allocation: it works on
caller-supplied buffers. These optional convenience helpers allocate the output
buffer for you, routing every allocation through a customer-provided realloc-
style callback that carries an opaque context pointer and a lifetime hint.

This is part of the libb64 project, and has been placed in the public domain.
*/

#ifndef BASE64_ALLOC_H
#define BASE64_ALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Lifetime hint, so an allocator can pool transient scratch separately from
   buffers handed back to the caller. */
typedef enum
{
	B64_MEM_SHORT,  /* transient scratch, released within the same call */
	B64_MEM_LONG    /* returned to the caller, freed later by the caller */
} b64_memlife;

/* Single realloc-style callback; semantics mirror C realloc/free:
     ptr == NULL  -> allocate `size` bytes
     size == 0    -> free `ptr`, return NULL
     otherwise    -> resize `ptr` to `size`
   Returns NULL on allocation failure (and, like realloc, leaves the old block
   valid). `ctx` is the opaque pointer below; `life` is the lifetime hint. */
typedef void* (*b64_realloc_fn)(void* ctx, void* ptr, size_t size,
                                b64_memlife life);

typedef struct
{
	b64_realloc_fn realloc_fn;
	void* ctx;
} b64_allocator;

/* One-shot encode. Allocates the output via `alloc` (B64_MEM_LONG), writes the
   NUL-terminated base64 to *out and its length (excluding the NUL) to *outlen.
   No line wrapping. Pass alloc == NULL (or a NULL realloc_fn) to use a built-in
   allocator backed by the C library's realloc/free. Returns 0 on success, or
   non-zero on allocation failure or size overflow (then *out = NULL,
   *outlen = 0). Free *out with base64_free(alloc, *out). */
int base64_encode_alloc(const b64_allocator* alloc, const void* plaintext,
                        size_t length, char** out, size_t* outlen);

/* One-shot decode. Allocates the output via `alloc` (B64_MEM_LONG), writes the
   decoded bytes to *out and the count to *outlen. Same allocator and return
   conventions as base64_encode_alloc. Free *out with base64_free. */
int base64_decode_alloc(const b64_allocator* alloc, const char* code,
                        size_t length, void** out, size_t* outlen);

/* Free a buffer returned by base64_encode_alloc / base64_decode_alloc, using
   the same allocator and the B64_MEM_LONG lifetime. NULL ptr is a no-op. */
void base64_free(const b64_allocator* alloc, void* ptr);

/* Construct a b64_allocator from a callback + context -- the C counterpart of
   the C++ builder's realloc setter. */
b64_allocator base64_allocator(b64_realloc_fn fn, void* ctx);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

namespace base64
{

/* Default allocator for the C++ stream wrappers: wraps new[]/delete[] so the
   default path keeps operator new's throw-on-OOM behavior. The wrappers resolve
   their allocator to this (or the caller's) once at construction, so their hot
   path needs no per-call check.

   Allocate/free only: the resize case (ptr != NULL && size != 0) of the
   b64_realloc_fn contract is NOT supported -- new[] cannot resize in place and
   the old block's size is unknown, so the contents are not copied and the old
   block is not freed. The wrappers never resize; don't use this callback with
   code that does. */
extern "C" inline void* b64_new_delete_realloc(void* ctx, void* ptr,
                                               size_t size, b64_memlife life)
{
	(void)ctx;
	(void)life;
	if (size)
		return new char[size];
	delete[] static_cast<char*>(ptr);
	return 0;
}

inline b64_allocator default_cpp_allocator()
{
	b64_allocator a = { b64_new_delete_realloc, 0 };
	return a;
}

} /* namespace base64 */

#endif /* __cplusplus */

#endif /* BASE64_ALLOC_H */
