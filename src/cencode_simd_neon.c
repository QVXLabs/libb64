/*
cencode_simd_neon.c - arm64 NEON bulk base64 encode (internal).

Built by CMake only on aarch64; provides base64_encode_bulk_simd. NEON is
the mandatory baseline on aarch64, so no runtime check is needed.
*/

#include "b64_internal.h"

#include <arm_neon.h>

static const uint8_t b64_alphabet[64] = {
	'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
	'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
	'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
	'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/',
};

/* NEON: encode whole 48-byte chunks -> 64 chars. vld3 deinterleaves each
   3-byte triple into a/b/c lanes; vqtbl4 maps the four 6-bit index vectors
   to the alphabet; vst4 re-interleaves. Returns input bytes consumed (a
   multiple of 48). */
static size_t encode_bulk_neon(const unsigned char* src, size_t len, char* dst)
{
	const uint8x16x4_t lut = vld1q_u8_x4(b64_alphabet);
	const unsigned char* s = src;
	char* d = dst;

	while (len >= 48)
	{
		uint8x16x3_t v = vld3q_u8(s);
		uint8x16_t a = v.val[0], b = v.val[1], c = v.val[2];
		uint8x16x4_t o;
		o.val[0] = vshrq_n_u8(a, 2);
		o.val[1] = vorrq_u8(vshlq_n_u8(vandq_u8(a, vdupq_n_u8(0x03)), 4),
		                    vshrq_n_u8(b, 4));
		o.val[2] = vorrq_u8(vshlq_n_u8(vandq_u8(b, vdupq_n_u8(0x0f)), 2),
		                    vshrq_n_u8(c, 6));
		o.val[3] = vandq_u8(c, vdupq_n_u8(0x3f));
		o.val[0] = vqtbl4q_u8(lut, o.val[0]);
		o.val[1] = vqtbl4q_u8(lut, o.val[1]);
		o.val[2] = vqtbl4q_u8(lut, o.val[2]);
		o.val[3] = vqtbl4q_u8(lut, o.val[3]);
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
