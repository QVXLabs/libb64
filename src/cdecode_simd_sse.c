/*
cdecode_simd_sse.c - x86 SSE bulk base64 decode (internal).

Built by CMake only on GNU/Clang x86 targets; provides
base64_decode_bulk_simd. Uses the Muła/Lemire SSE4.1 kernel, runtime-gated
by __builtin_cpu_supports. Decodes whole clean 16-char blocks (16 -> 12
bytes) and stops before the first block containing a non-alphabet byte
(whitespace, '=', >= 0x80, ...) so the scalar core preserves the exact
skip/padding semantics.
*/

#include "b64_internal.h"

#include <immintrin.h>
#include <string.h>

__attribute__((target("sse4.1")))
static size_t decode_bulk_sse41(const char* src, size_t len, char* dst)
{
	const __m128i lut_lo = _mm_setr_epi8(
		0x15, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
		0x11, 0x11, 0x13, 0x1A, 0x1B, 0x1B, 0x1B, 0x1A);
	const __m128i lut_hi = _mm_setr_epi8(
		0x10, 0x10, 0x01, 0x02, 0x04, 0x08, 0x04, 0x08,
		0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10);
	const __m128i lut_roll = _mm_setr_epi8(
		0, 16, 19, 4, -65, -65, -71, -71, 0, 0, 0, 0, 0, 0, 0, 0);
	const __m128i mask_0f = _mm_set1_epi8(0x0f);
	const __m128i mask_2f = _mm_set1_epi8(0x2f);
	const __m128i pack =
		_mm_setr_epi8(2, 1, 0, 6, 5, 4, 10, 9, 8, 14, 13, 12, -1, -1, -1, -1);
	const char* s = src;
	char* d = dst;

	while (len >= 16)
	{
		__m128i v = _mm_loadu_si128((const __m128i*)s);
		__m128i hi_nib = _mm_and_si128(_mm_srli_epi32(v, 4), mask_0f);
		__m128i lo_nib = _mm_and_si128(v, mask_0f);
		__m128i lo = _mm_shuffle_epi8(lut_lo, lo_nib);
		__m128i hi = _mm_shuffle_epi8(lut_hi, hi_nib);

		/* (lo & hi) != 0 in any lane => a non-alphabet byte; bail to scalar */
		if (!_mm_testz_si128(lo, hi))
			break;

		__m128i eq2f = _mm_cmpeq_epi8(v, mask_2f);
		__m128i roll = _mm_shuffle_epi8(lut_roll, _mm_add_epi8(eq2f, hi_nib));
		__m128i vals = _mm_add_epi8(v, roll);

		/* pack 16 six-bit values -> 12 bytes */
		__m128i ab = _mm_maddubs_epi16(vals, _mm_set1_epi32(0x01400140));
		__m128i merged = _mm_madd_epi16(ab, _mm_set1_epi32(0x00011000));
		__m128i out = _mm_shuffle_epi8(merged, pack);

		unsigned char tmp[16];
		_mm_storeu_si128((__m128i*)tmp, out);
		memcpy(d, tmp, 12);  /* only 12 bytes are valid; no over-write */

		s += 16;
		d += 12;
		len -= 16;
	}
	return (size_t)(s - src);
}

/* AVX2: 32 chars (clean) -> 24 bytes per iteration. Same algorithm as the
   SSE4.1 path on 256-bit vectors; a final dword gather compacts the two
   per-lane 12-byte results into a contiguous 24 bytes. */
__attribute__((target("avx2")))
static size_t decode_bulk_avx2(const char* src, size_t len, char* dst)
{
	const __m256i lut_lo = _mm256_setr_epi8(
		0x15, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
		0x11, 0x11, 0x13, 0x1A, 0x1B, 0x1B, 0x1B, 0x1A,
		0x15, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
		0x11, 0x11, 0x13, 0x1A, 0x1B, 0x1B, 0x1B, 0x1A);
	const __m256i lut_hi = _mm256_setr_epi8(
		0x10, 0x10, 0x01, 0x02, 0x04, 0x08, 0x04, 0x08,
		0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
		0x10, 0x10, 0x01, 0x02, 0x04, 0x08, 0x04, 0x08,
		0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10);
	const __m256i lut_roll = _mm256_setr_epi8(
		0, 16, 19, 4, -65, -65, -71, -71, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 16, 19, 4, -65, -65, -71, -71, 0, 0, 0, 0, 0, 0, 0, 0);
	const __m256i mask_0f = _mm256_set1_epi8(0x0f);
	const __m256i mask_2f = _mm256_set1_epi8(0x2f);
	const __m256i pack = _mm256_setr_epi8(
		2, 1, 0, 6, 5, 4, 10, 9, 8, 14, 13, 12, -1, -1, -1, -1,
		2, 1, 0, 6, 5, 4, 10, 9, 8, 14, 13, 12, -1, -1, -1, -1);
	const __m256i gather = _mm256_setr_epi32(0, 1, 2, 4, 5, 6, 7, 7);
	const char* s = src;
	char* d = dst;

	while (len >= 32)
	{
		__m256i v = _mm256_loadu_si256((const __m256i*)s);
		__m256i hi_nib = _mm256_and_si256(_mm256_srli_epi32(v, 4), mask_0f);
		__m256i lo_nib = _mm256_and_si256(v, mask_0f);
		__m256i lo = _mm256_shuffle_epi8(lut_lo, lo_nib);
		__m256i hi = _mm256_shuffle_epi8(lut_hi, hi_nib);

		if (!_mm256_testz_si256(lo, hi))
			break;

		__m256i eq2f = _mm256_cmpeq_epi8(v, mask_2f);
		__m256i roll =
			_mm256_shuffle_epi8(lut_roll, _mm256_add_epi8(eq2f, hi_nib));
		__m256i vals = _mm256_add_epi8(v, roll);

		__m256i ab = _mm256_maddubs_epi16(vals, _mm256_set1_epi32(0x01400140));
		__m256i merged = _mm256_madd_epi16(ab, _mm256_set1_epi32(0x00011000));
		__m256i shuffled = _mm256_shuffle_epi8(merged, pack);
		__m256i out = _mm256_permutevar8x32_epi32(shuffled, gather);

		unsigned char tmp[32];
		_mm256_storeu_si256((__m256i*)tmp, out);
		memcpy(d, tmp, 24);

		s += 32;
		d += 24;
		len -= 32;
	}
	return (size_t)(s - src);
}

size_t base64_decode_bulk_simd(const char* src, size_t len, void* dst)
{
	if (__builtin_cpu_supports("avx2"))
		return decode_bulk_avx2(src, len, (char*)dst);
	if (__builtin_cpu_supports("sse4.1"))
		return decode_bulk_sse41(src, len, (char*)dst);
	return 0;
}
