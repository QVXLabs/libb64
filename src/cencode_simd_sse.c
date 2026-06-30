/*
cencode_simd_sse.c - x86 SSE bulk base64 encode (internal).

Built by CMake only on GNU/Clang x86 targets; provides
base64_encode_bulk_simd. Uses the Muła/Lemire kernel, runtime-gated by
__builtin_cpu_supports so a single binary runs on any x86. The SSE floor is
SSE4.1.
*/

#include "b64_internal.h"

#include <immintrin.h>
#include <stdint.h>

/* Cache-bypassing (non-temporal) stores pay off only once the output stops
   fitting the last-level cache; below this the normal cached store wins.
   Threshold is in input bytes; encode output is 4/3 of it. */
#define B64_NT_ENCODE_MIN (6u << 20)

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
	/* dst advances 16/iter, so 16-byte alignment is loop-invariant. */
	const int use_nt = len >= B64_NT_ENCODE_MIN && ((uintptr_t)d & 15) == 0;

	for (; len >= 16; s += 12, d += 16, len -= 12)
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

		if (use_nt)
			_mm_stream_si128((__m128i*)d, out);
		else
			_mm_storeu_si128((__m128i*)d, out);
	}
	if (use_nt)
		_mm_sfence();  /* make the streamed stores visible before we return */
	return (size_t)(s - src);
}

/* AVX2: 32 input bytes (24 consumed) -> 32 output chars per iteration. A
   dword permute moves each 128-bit lane's 12 source bytes into the same
   relative position so the SSE per-lane shuffle/extract applies to both. */
__attribute__((target("avx2")))
static size_t encode_bulk_avx2(const unsigned char* src, size_t len, char* dst)
{
	const __m256i perm = _mm256_setr_epi32(0, 1, 2, 3, 3, 4, 5, 6);
	const __m256i shuf = _mm256_set_epi8(
		10, 11, 9, 10, 7, 8, 6, 7, 4, 5, 3, 4, 1, 2, 0, 1,
		10, 11, 9, 10, 7, 8, 6, 7, 4, 5, 3, 4, 1, 2, 0, 1);
	const __m256i lut = _mm256_setr_epi8(
		65, 71, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -19, -16, 0, 0,
		65, 71, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -19, -16, 0, 0);
	const unsigned char* s = src;
	char* d = dst;
	/* dst advances 32/iter, so 32-byte alignment is loop-invariant. */
	const int use_nt = len >= B64_NT_ENCODE_MIN && ((uintptr_t)d & 31) == 0;

	for (; len >= 32; s += 24, d += 32, len -= 24)
	{
		__m256i in = _mm256_loadu_si256((const __m256i*)s);
		in = _mm256_permutevar8x32_epi32(in, perm);
		in = _mm256_shuffle_epi8(in, shuf);

		__m256i t0 = _mm256_and_si256(in, _mm256_set1_epi32(0x0fc0fc00));
		__m256i t1 = _mm256_mulhi_epu16(t0, _mm256_set1_epi32(0x04000040));
		__m256i t2 = _mm256_and_si256(in, _mm256_set1_epi32(0x003f03f0));
		__m256i t3 = _mm256_mullo_epi16(t2, _mm256_set1_epi32(0x01000010));
		__m256i idx = _mm256_or_si256(t1, t3);

		__m256i reduced = _mm256_subs_epu8(idx, _mm256_set1_epi8(51));
		__m256i greater = _mm256_cmpgt_epi8(idx, _mm256_set1_epi8(25));
		reduced = _mm256_sub_epi8(reduced, greater);
		__m256i out = _mm256_add_epi8(idx,
		                              _mm256_shuffle_epi8(lut, reduced));

		if (use_nt)
			_mm256_stream_si256((__m256i*)d, out);
		else
			_mm256_storeu_si256((__m256i*)d, out);
	}
	if (use_nt)
		_mm_sfence();  /* make the streamed stores visible before we return */
	return (size_t)(s - src);
}

static size_t encode_bulk_none(const unsigned char* src, size_t len, char* dst)
{
	(void)src; (void)len; (void)dst;
	return 0;  /* no SSE4.1: the scalar core does everything */
}

typedef size_t (*encode_fn)(const unsigned char*, size_t, char*);

static size_t encode_resolve(const unsigned char*, size_t, char*);
static encode_fn encode_impl = encode_resolve;

/* First call picks the kernel for this CPU and patches encode_impl; every
   later call dispatches straight through it, with no per-call
   __builtin_cpu_supports. The store races benignly -- all racers resolve to
   the same pointer and an aligned pointer write is atomic on x86. */
static size_t encode_resolve(const unsigned char* src, size_t len, char* dst)
{
	encode_fn fn = encode_bulk_none;
	if (__builtin_cpu_supports("avx2"))
		fn = encode_bulk_avx2;
	else if (__builtin_cpu_supports("sse4.1"))
		fn = encode_bulk_sse41;
	encode_impl = fn;
	return fn(src, len, dst);
}

size_t base64_encode_bulk_simd(const unsigned char* src, size_t len, char* dst)
{
	return encode_impl(src, len, dst);
}
