/*
cdecode_simd_neon.c - ARM NEON bulk base64 decode (internal).

Placeholder: NEON decode is not implemented yet, so this returns 0 and the
scalar core (already ~3x over the original) handles decoding on ARM. The
encode NEON kernel lives in cencode_simd_neon.c.
*/

#include "b64_internal.h"

size_t base64_decode_bulk_simd(const char* src, size_t len, void* dst)
{
	(void)src;
	(void)len;
	(void)dst;
	return 0;
}
