/*
balloc.c - allocating convenience helpers over the libb64 block API, routed
through an optional customer-provided allocator (see b64/alloc.h).

This is part of the libb64 project, and has been placed in the public domain.
*/

#include <b64/alloc.h>
#include <b64/cencode.h>
#include <b64/cdecode.h>

#include <stdlib.h>

/* Default allocator: ignore ctx/lifetime, wrap the C library. */
static void* default_realloc(void* ctx, void* ptr, size_t size,
                             b64_memlife life)
{
	(void)ctx;
	(void)life;
	return size ? realloc(ptr, size) : (free(ptr), NULL);
}

static const b64_allocator default_allocator = { default_realloc, NULL };

static const b64_allocator* resolve(const b64_allocator* alloc)
{
	return (alloc && alloc->realloc_fn) ? alloc : &default_allocator;
}

int base64_encode_alloc(const b64_allocator* alloc, const void* plaintext,
                        size_t length, char** out, size_t* outlen)
{
	const b64_allocator* a = resolve(alloc);
	base64_encodestate state;
	size_t cap, n;
	char* buf;

	if (out) *out = NULL;
	if (outlen) *outlen = 0;
	if (!out || !outlen)
		return -1;

	base64_init_encodestate(&state);
	/* No wrapping; encode_length reserves room for the trailing NUL too. */
	cap = base64_encode_length(length, &state);
	if (cap == 0 && length != 0)
		return -1;  /* size overflow */

	buf = (char*)a->realloc_fn(a->ctx, NULL, cap + 1, B64_MEM_LONG);
	if (!buf)
		return -1;

	n = base64_encode_block(plaintext, length, buf, &state);
	n += base64_encode_blockend(buf + n, &state);
	buf[n] = '\0';

	*out = buf;
	*outlen = n;
	return 0;
}

int base64_decode_alloc(const b64_allocator* alloc, const char* code,
                        size_t length, void** out, size_t* outlen)
{
	const b64_allocator* a = resolve(alloc);
	base64_decodestate state;
	size_t cap, n;
	void* buf;

	if (out) *out = NULL;
	if (outlen) *outlen = 0;
	if (!out || !outlen)
		return -1;

	cap = base64_decode_maxlength(length);
	buf = a->realloc_fn(a->ctx, NULL, cap, B64_MEM_LONG);
	if (!buf)
		return -1;

	base64_init_decodestate(&state);
	n = base64_decode_block(code, length, buf, &state);

	*out = buf;
	*outlen = n;
	return 0;
}

void base64_free(const b64_allocator* alloc, void* ptr)
{
	const b64_allocator* a = resolve(alloc);
	if (ptr)
		a->realloc_fn(a->ctx, ptr, 0, B64_MEM_LONG);
}
