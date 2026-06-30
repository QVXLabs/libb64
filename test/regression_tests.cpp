/* Regression tests for historical libb64 bugs (commit refs inline). */

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "b64_test_util.h"

using b64test::decode;

extern "C" {
#include <b64/cencode.h>
#include <b64/cdecode.h>
}

// 430cbdc/cc9baa0: decode_value upper bound was `>` not `>=`, so '{'
// (index 80 in the 80-entry table) read out of bounds. Outside the
// alphabet must yield -1.
TEST(Regression, DecodeValueUpperBoundOffByOne)
{
	EXPECT_EQ(base64_decode_value('z'), 51);   // last valid alphabet entry
	EXPECT_EQ(base64_decode_value('{'), -1);   // one past the table: was OOB
	EXPECT_EQ(base64_decode_value('|'), -1);
	EXPECT_EQ(base64_decode_value('~'), -1);
	EXPECT_EQ(base64_decode_value(static_cast<signed char>(127)), -1);
}

// Same bug family, lower bound: bytes below '+' (43) must be rejected.
TEST(Regression, DecodeValueLowerBound)
{
	EXPECT_EQ(base64_decode_value('+'), 62);   // first table entry
	EXPECT_EQ(base64_decode_value('*'), -1);   // 42, just below the table
	EXPECT_EQ(base64_decode_value(' '), -1);
	EXPECT_EQ(base64_decode_value('\0'), -1);
}

// A boundary char embedded in valid input must be skipped, not read OOB.
TEST(Regression, DecodeBoundaryCharInStream)
{
	EXPECT_EQ(decode("Zm9v{"), "foo");
	EXPECT_EQ(decode("Z{m}9~v"), "foo");
}

// 96ce1a3 (Jakub Wilk): decoder fragment was signed char (mishandled high
// values), now int. All-byte round-trip would fail under the old bug.
TEST(Regression, DecoderHandlesAllFragmentValues)
{
	std::string in;
	for (int i = 0; i < 256; ++i)
		in.push_back(static_cast<char>(i));
	EXPECT_EQ(decode(b64test::encode(in)), in);
}

// High-bit bytes (0x80..0xFF) are negative as signed char; must be
// rejected, not used to index the table.
TEST(Regression, DecodeValueHighBitBytes)
{
	for (int b = 0x80; b <= 0xFF; ++b)
		EXPECT_EQ(base64_decode_value(static_cast<signed char>(b)), -1)
			<< "byte=0x" << std::hex << b;
}

// '=' maps to the sentinel -2 (vs -1 for invalid); both are skipped.
TEST(Regression, DecodeValueEqualsSentinel)
{
	EXPECT_EQ(base64_decode_value('='), -2);
}

// d54125c / v2.0.0: base64_encode_length must detect size_t overflow and
// return 0, not wrap to a too-small value (heap overflow in callers).
TEST(Regression, EncodeLengthOverflowGuard)
{
	base64_encodestate s;
	base64_init_encodestate(&s);

	// Huge plaintext length: 4/3 expansion overflows size_t -> must be 0.
	EXPECT_EQ(base64_encode_length(SIZE_MAX, &s), 0u);
	EXPECT_EQ(base64_encode_length(SIZE_MAX - 10, &s), 0u);

	// A comfortably-representable length must still return a real size.
	EXPECT_GT(base64_encode_length(SIZE_MAX / 8, &s), 0u);
	EXPECT_EQ(base64_encode_length(3, &s), 4u);
}

// The overflow guard must also hold when line wrapping is enabled: with a
// non-zero chars_per_line the pre-fix code wrapped on `plain_len + 2` and
// returned 1 instead of the 0 error sentinel.
TEST(Regression, EncodeLengthOverflowGuardWrapped)
{
	base64_encodestate s;
	base64_init_encodestate(&s);
	s.chars_per_line = 76;
	EXPECT_EQ(base64_encode_length(SIZE_MAX, &s), 0u);
	EXPECT_EQ(base64_encode_length(SIZE_MAX - 1, &s), 0u);
}

// base64_encode_value must not index its 64-entry table with a negative
// signed char; out-of-range (and negative) inputs map to '='.
TEST(Regression, EncodeValueRejectsOutOfRange)
{
	EXPECT_EQ(base64_encode_value(0), 'A');
	EXPECT_EQ(base64_encode_value(63), '/');
	EXPECT_EQ(base64_encode_value(static_cast<signed char>(-1)), '=');
	EXPECT_EQ(base64_encode_value(64), '=');
	EXPECT_EQ(base64_encode_value(static_cast<signed char>(-128)), '=');
}

// base64_decode_maxlength under-sized by 1 for input length ≡ 3 (mod 4): the
// coroutine decoder's speculative non-advancing store wrote one byte past a
// buffer sized exactly by the helper. Decode unpadded inputs (which include
// the length-≡3 case) into an exactly-sized buffer and require correct output;
// an ASan build proves there is no out-of-bounds write.
TEST(Regression, DecodeMaxlengthCoversSpeculativeStore)
{
	EXPECT_GE(base64_decode_maxlength(3), 3u);
	for (size_t n = 0; n <= 300; ++n)
	{
		const std::string in = b64test::pattern(n);
		std::string enc = b64test::encode(in);
		const size_t last = enc.find_last_not_of('=');
		enc.erase(last == std::string::npos ? 0 : last + 1);  // strip padding
		std::vector<char> out(base64_decode_maxlength(enc.size()));  // exact
		base64_decodestate s;
		base64_init_decodestate(&s);
		const size_t m = base64_decode_block(enc.data(), enc.size(),
		                                     out.data(), &s);
		EXPECT_EQ(std::string(out.data(), m), in) << "n=" << n;
	}
}
