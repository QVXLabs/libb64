/*
cencode_simd_neon.c - ARM NEON bulk base64 encode (internal).

Built by CMake on ARM targets. NEON is the mandatory baseline on aarch64,
but only an optional extension on ARMv7-A: this file uses NEON when the
compiler reports it (__ARM_NEON, i.e. the build passed -mfpu=neon or the
target is aarch64) and otherwise returns 0 so the scalar core is used.

The 6-bit index extraction (vld3/shift/mask) is identical for both ISAs;
only the index->ASCII mapping differs: aarch64 uses the 64-entry table
lookup vqtbl4q (AArch64-only), ARMv7 uses the branchless reduced-offset LUT
via vtbl2 (available on both).
*/

#include "b64_internal.h"

#if defined(__aarch64__) || defined(__ARM_NEON) || defined(__ARM_NEON__)

#include <arm_neon.h>

#if defined(__aarch64__)

static const uint8_t b64_alphabet[64] = {
	'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
	'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
	'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
	'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/',
};

static inline uint8x16_t neon_b64_ascii(uint8x16_t idx)
{
	return vqtbl4q_u8(vld1q_u8_x4(b64_alphabet), idx);
}

#else /* ARMv7-A NEON: no q-register 64-entry table lookup */

static inline uint8x16_t neon_b64_ascii(uint8x16_t idx)
{
	/* offset to add to each 6-bit index, indexed by a "reduced" value in
	   0..13 (Muła): 0->'A', 1->'a'-26, 2..11->'0'-52, 12->'+'-62, 13->'/' */
	static const uint8_t off_lut[16] = {
		65, 71, (uint8_t)-4, (uint8_t)-4, (uint8_t)-4, (uint8_t)-4,
		(uint8_t)-4, (uint8_t)-4, (uint8_t)-4, (uint8_t)-4, (uint8_t)-4,
		(uint8_t)-4, (uint8_t)-19, (uint8_t)-16, 0, 0
	};
	uint8x8x2_t lut2;
	uint8x16_t reduced = vqsubq_u8(idx, vdupq_n_u8(51));
	uint8x16_t greater = vcgtq_u8(idx, vdupq_n_u8(25));
	reduced = vsubq_u8(reduced, greater);  /* subtract 0xFF == add 1 */
	lut2.val[0] = vld1_u8(off_lut);
	lut2.val[1] = vld1_u8(off_lut + 8);
	return vaddq_u8(idx, vcombine_u8(vtbl2_u8(lut2, vget_low_u8(reduced)),
	                                 vtbl2_u8(lut2, vget_high_u8(reduced))));
}

#endif

/* Encode whole 48-byte chunks -> 64 chars. Returns input bytes consumed
   (a multiple of 48). */
static size_t encode_bulk_neon(const unsigned char* src, size_t len, char* dst)
{
	const unsigned char* s = src;
	char* d = dst;
	while (len >= 48)
	{
		uint8x16x3_t v = vld3q_u8(s);
		uint8x16_t a = v.val[0], b = v.val[1], c = v.val[2];
		uint8x16x4_t o;
		o.val[0] = neon_b64_ascii(vshrq_n_u8(a, 2));
		o.val[1] = neon_b64_ascii(vorrq_u8(
			vshlq_n_u8(vandq_u8(a, vdupq_n_u8(0x03)), 4), vshrq_n_u8(b, 4)));
		o.val[2] = neon_b64_ascii(vorrq_u8(
			vshlq_n_u8(vandq_u8(b, vdupq_n_u8(0x0f)), 2), vshrq_n_u8(c, 6)));
		o.val[3] = neon_b64_ascii(vandq_u8(c, vdupq_n_u8(0x3f)));
		vst4q_u8((uint8_t*)d, o);
		s += 48;
		d += 64;
		len -= 48;
	}
	return (size_t)(s - src);
}

size_t base64_encode_bulk_simd(const unsigned char* src, size_t len, char* dst)
{
	return encode_bulk_neon(src, len, dst);
}

#else /* ARM without NEON enabled (no -mfpu=neon): scalar fallback */

size_t base64_encode_bulk_simd(const unsigned char* src, size_t len, char* dst)
{
	(void)src;
	(void)len;
	(void)dst;
	return 0;
}

#endif
