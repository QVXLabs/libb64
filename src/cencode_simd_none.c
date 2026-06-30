/*
cencode_simd_none.c - scalar fallback for base64_encode_bulk_simd.

Built by CMake on targets without a SIMD kernel (MSVC, non-x86/arm, etc.).
Returning 0 means the public encoder uses the scalar core for everything.
*/

#include "b64_internal.h"

size_t base64_encode_bulk_simd(const unsigned char* src, size_t len, char* dst)
{
	(void)src;
	(void)len;
	(void)dst;
	return 0;
}
