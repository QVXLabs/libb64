/*
cdecode_simd_neon.c - ARM NEON bulk base64 decode (internal).

Gated on __ARM_NEON like the encoder: NEON when the build enables it
(baseline on aarch64, -mfpu=neon on ARMv7-A), else scalar.

Translate/validate uses the Muła/Lemire nibble LUTs (vqtbl1q on aarch64,
vtbl2 on ARMv7). NEON has no maddubs, so the 16 six-bit values are packed
to 12 bytes with plain shifts/ORs on 16- then 32-bit reinterpretations
(the same arithmetic the SSE maddubs+madd performs).
*/

#include "b64_internal.h"

#if defined(__aarch64__) || defined(__ARM_NEON) || defined(__ARM_NEON__) \
	|| (defined(_MSC_VER) && defined(_M_ARM64))

#if defined(_MSC_VER) && defined(_M_ARM64)
#  include <arm64_neon.h>
#  include <string.h>  /* cl.exe has no __builtin_memcpy */
#else
#  include <arm_neon.h>
#endif

/* 16-entry byte lookup: result[i] = tbl[idx[i]]; indices >= 16 yield 0.
   Used both for the constant nibble LUTs (load them with vld1q_u8) and for
   the final pack (table = the data vector). */
static inline uint8x16_t neon_lut16(uint8x16_t tbl, uint8x16_t idx)
{
#if defined(__aarch64__) || (defined(_MSC_VER) && defined(_M_ARM64))
	return vqtbl1q_u8(tbl, idx);
#else
	uint8x8x2_t t;
	t.val[0] = vget_low_u8(tbl);
	t.val[1] = vget_high_u8(tbl);
	return vcombine_u8(vtbl2_u8(t, vget_low_u8(idx)),
	                   vtbl2_u8(t, vget_high_u8(idx)));
#endif
}

static size_t decode_bulk_neon(const char* src, size_t len, char* dst)
{
	static const uint8_t lut_lo[16] = {
		0x15, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
		0x11, 0x11, 0x13, 0x1A, 0x1B, 0x1B, 0x1B, 0x1A};
	static const uint8_t lut_hi[16] = {
		0x10, 0x10, 0x01, 0x02, 0x04, 0x08, 0x04, 0x08,
		0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10};
	static const uint8_t lut_roll[16] = {
		0, 16, 19, 4, (uint8_t)-65, (uint8_t)-65, (uint8_t)-71,
		(uint8_t)-71, 0, 0, 0, 0, 0, 0, 0, 0};
	static const uint8_t pack_idx[16] = {
		2, 1, 0, 6, 5, 4, 10, 9, 8, 14, 13, 12, 0xff, 0xff, 0xff, 0xff};
	const char* s = src;
	char* d = dst;

	for (; len >= 16; s += 16, d += 12, len -= 16)
	{
		uint8x16_t v = vld1q_u8((const uint8_t*)s);
		uint8x16_t hi_nib = vshrq_n_u8(v, 4);
		uint8x16_t lo_nib = vandq_u8(v, vdupq_n_u8(0x0f));
		uint8x16_t lo = neon_lut16(vld1q_u8(lut_lo), lo_nib);
		uint8x16_t hi = neon_lut16(vld1q_u8(lut_hi), hi_nib);

		/* (lo & hi) != 0 in any lane => a non-alphabet byte; bail to scalar */
		uint64x2_t err = vreinterpretq_u64_u8(vandq_u8(lo, hi));
		if (vgetq_lane_u64(err, 0) | vgetq_lane_u64(err, 1))
			break;

		uint8x16_t eq2f = vceqq_u8(v, vdupq_n_u8(0x2f));
		uint8x16_t roll =
			neon_lut16(vld1q_u8(lut_roll), vaddq_u8(eq2f, hi_nib));
		uint8x16_t vals = vaddq_u8(v, roll);

		/* combine each pair of sextets -> 12-bit, then each pair of those
		   -> a 24-bit value per output triple */
		uint16x8_t x = vreinterpretq_u16_u8(vals);
		uint16x8_t b12 = vorrq_u16(
			vshlq_n_u16(vandq_u16(x, vdupq_n_u16(0x003f)), 6),
			vshrq_n_u16(x, 8));
		uint32x4_t y = vreinterpretq_u32_u16(b12);
		uint32x4_t w = vorrq_u32(
			vshlq_n_u32(vandq_u32(y, vdupq_n_u32(0x00000fff)), 12),
			vshrq_n_u32(y, 16));

		/* take the big-endian low 3 bytes of each 24-bit word -> 12 bytes */
		uint8x16_t packed =
			neon_lut16(vreinterpretq_u8_u32(w), vld1q_u8(pack_idx));
		unsigned char tmp[16];
		vst1q_u8(tmp, packed);
#if defined(_MSC_VER) && defined(_M_ARM64)
		memcpy(d, tmp, 12);
#else
		__builtin_memcpy(d, tmp, 12);
#endif
	}
	return (size_t)(s - src);
}

size_t base64_decode_bulk_simd(const char* src, size_t len, void* dst)
{
	return decode_bulk_neon(src, len, (char*)dst);
}

#else /* ARM without NEON enabled: scalar */

size_t base64_decode_bulk_simd(const char* src, size_t len, void* dst)
{
	(void)src;
	(void)len;
	(void)dst;
	return 0;
}

#endif
