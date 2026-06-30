/*
cencode_simd_sse.c - x86 SSE bulk base64 encode (internal).

Built by CMake only on GNU/Clang x86 targets; provides
base64_encode_bulk_simd. Uses the Muła/Lemire kernel, runtime-gated by
__builtin_cpu_supports so a single binary runs on any x86. The SSE floor is
SSE4.1.
*/

#include "b64_internal.h"

#include <immintrin.h>

/* SSE4.1: 16 input bytes (12 consumed) -> 16 output chars per iteration. The
   last iteration reads 4 bytes it does not consume; they are always within
   the buffer because the loop only runs while >= 16 bytes remain. */
__attribute__((target("sse4.1")))
static size_t encode_bulk_sse41(const unsigned char* src, size_t len,
                                char* dst)
{
	const __m128i shuf =
		_mm_set_epi8(10, 11, 9, 10, 7, 8, 6, 7, 4, 5, 3, 4, 1, 2, 0, 1);
	const __m128i lut =
		_mm_setr_epi8(65, 71, -4, -4, -4, -4, -4, -4,
		              -4, -4, -4, -4, -19, -16, 0, 0);
	const unsigned char* s = src;
	char* d = dst;

	while (len >= 16)
	{
		__m128i in = _mm_loadu_si128((const __m128i*)s);
		in = _mm_shuffle_epi8(in, shuf);

		/* split each 24-bit group into four 6-bit indices */
		__m128i t0 = _mm_and_si128(in, _mm_set1_epi32(0x0fc0fc00));
		__m128i t1 = _mm_mulhi_epu16(t0, _mm_set1_epi32(0x04000040));
		__m128i t2 = _mm_and_si128(in, _mm_set1_epi32(0x003f03f0));
		__m128i t3 = _mm_mullo_epi16(t2, _mm_set1_epi32(0x01000010));
		__m128i indices = _mm_or_si128(t1, t3);

		/* indices (0..63) -> base64 ASCII, branchless */
		__m128i reduced = _mm_subs_epu8(indices, _mm_set1_epi8(51));
		__m128i greater = _mm_cmpgt_epi8(indices, _mm_set1_epi8(25));
		reduced = _mm_sub_epi8(reduced, greater);
		__m128i out = _mm_add_epi8(indices, _mm_shuffle_epi8(lut, reduced));

		_mm_storeu_si128((__m128i*)d, out);
		s += 12;
		d += 16;
		len -= 12;
	}
	return (size_t)(s - src);
}

size_t base64_encode_bulk_simd(const unsigned char* src, size_t len, char* dst)
{
	if (__builtin_cpu_supports("sse4.1"))
		return encode_bulk_sse41(src, len, dst);
	return 0;
}
