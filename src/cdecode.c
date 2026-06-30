/*
cdecoder.c - c source to a base64 decoding algorithm implementation

This is part of the libb64 project, and has been placed in the public domain.
For details, see http://sourceforge.net/projects/libb64
*/

#include <b64/cdecode.h>

#include <stdint.h>

#include "b64_internal.h"
#include "cdecode_swar_table.h"

/* Direct-indexed decode table: byte value -> 0..63, -2 for '=', -1 invalid.
   Shared by base64_decode_value and the block loop. */
static const signed char decoding[256] = {
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
	52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,
	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
	15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
	-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
	41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
};

size_t base64_decode_maxlength(size_t encode_len)
{
	return encode_len / 4 * 3 + 2;
}

int base64_decode_value(signed char value_in)
{
	return decoding[(unsigned char)value_in];
}

void base64_init_decodestate(base64_decodestate* state_in)
{
	state_in->step = step_a;
	state_in->plainchar = 0;
}

size_t base64_decode_block_scalar(const char* code_in, const size_t length_in, void* plaintext_out, base64_decodestate* state_in)
{
	const char* codechar = code_in;
	const char* const codeend = code_in + length_in;
	char* plainchar = plaintext_out;
	int fragment;

	*plainchar = state_in->plainchar;

	switch (state_in->step)
	{
		while (1)
		{
	case step_a:
			/* bulk path: decode clean 4-char quads (no whitespace,
			   padding or invalid bytes) 4->3 with one bounds check. */
			while (codechar + 4 <= codeend)
			{
				uint32_t out =
					b64_dec0[(unsigned char)codechar[0]]
					| b64_dec1[(unsigned char)codechar[1]]
					| b64_dec2[(unsigned char)codechar[2]]
					| b64_dec3[(unsigned char)codechar[3]];
				/* bit 24 set iff any char was non-alphabet */
				if (out >> 24)
					break;  /* fall back to the per-char path */
				plainchar[0] = (char)(out >> 16);
				plainchar[1] = (char)(out >> 8);
				plainchar[2] = (char)(out);
				plainchar += 3;
				codechar += 4;
			}
			do {
				if (codechar == codeend)
				{
					state_in->step = step_a;
					state_in->plainchar = *plainchar;
					return (size_t)(plainchar - (char *) plaintext_out);
				}
				fragment = decoding[(unsigned char)*codechar++];
			} while (fragment < 0);
			*plainchar    = (fragment & 0x03f) << 2;
	case step_b:
			do {
				if (codechar == codeend)
				{
					state_in->step = step_b;
					state_in->plainchar = *plainchar;
					return (size_t)(plainchar - (char *) plaintext_out);
				}
				fragment = decoding[(unsigned char)*codechar++];
			} while (fragment < 0);
			*plainchar++ |= (fragment & 0x030) >> 4;
			*plainchar    = (fragment & 0x00f) << 4;
	case step_c:
			do {
				if (codechar == codeend)
				{
					state_in->step = step_c;
					state_in->plainchar = *plainchar;
					return (size_t)(plainchar - (char *) plaintext_out);
				}
				fragment = decoding[(unsigned char)*codechar++];
			} while (fragment < 0);
			*plainchar++ |= (fragment & 0x03c) >> 2;
			*plainchar    = (fragment & 0x003) << 6;
	case step_d:
			do {
				if (codechar == codeend)
				{
					state_in->step = step_d;
					state_in->plainchar = *plainchar;
					return (size_t)(plainchar - (char *) plaintext_out);
				}
				fragment = decoding[(unsigned char)*codechar++];
			} while (fragment < 0);
			*plainchar++   |= (fragment & 0x03f);
		}
	}
	/* control should not reach here */
	return (size_t) (plainchar - (char *) plaintext_out);
}

size_t base64_decode_block(const char* code_in, const size_t length_in, void* plaintext_out, base64_decodestate* state_in)
{
	const char* p = code_in;
	char* o = (char*)plaintext_out;
	size_t rem = length_in;

	/* Alternate SIMD and scalar so clean runs after whitespace/padding (MIME
	   line wrapping) re-engage SIMD instead of falling to scalar for the
	   whole rest. SIMD bulk-decodes whole clean 4-char groups from a step_a
	   boundary; when it blocks, the scalar core (fast quad path) decodes the
	   clean prefix plus the one blocking byte, then SIMD retries. */
	for (size_t consumed; rem; )
	{
		if (state_in->step == step_a && rem >= 16
		    && (consumed = base64_decode_bulk_simd(p, rem, o)) != 0)
		{
			p += consumed;
			o += consumed / 4 * 3;
			rem -= consumed;
			continue;
		}
		/* SIMD blocked: a non-alphabet byte within the next 16 chars, fewer
		   than 16 chars left, or mid-group. Scalar-decode the run of clean
		   chars plus the one blocking byte, then loop to retry SIMD. */
		size_t clean = 0;
		while (clean < rem && decoding[(unsigned char)p[clean]] >= 0)
			clean++;
		size_t take = clean < rem ? clean + 1 : clean;
		o += base64_decode_block_scalar(p, take, o, state_in);
		p += take;
		rem -= take;
	}
	return (size_t)(o - (char*)plaintext_out);
}
