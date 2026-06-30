/*
cdecode_simd_none.c - scalar fallback for base64_decode_bulk_simd.

Built by CMake on targets without a SIMD decode kernel. Returning 0 means
the public decoder uses the scalar core for everything.
*/

#include "b64_internal.h"

size_t base64_decode_bulk_simd(const char* src, size_t len, void* dst)
{
	(void)src;
	(void)len;
	(void)dst;
	return 0;
}
