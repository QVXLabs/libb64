/*
Regression tests for historical libb64 bugs. Each test references the
commit / report that fixed the defect so the guarantee is documented.
*/

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "b64_test_util.h"

using b64test::decode;

extern "C" {
#include <b64/cencode.h>
#include <b64/cdecode.h>
}

// 430cbdc / cc9baa0 (Gabriel Kihlman): base64_decode_value used `>` instead
// of `>=` for the upper bound, so the byte one past the table ('{' = 123,
// index 80 in an 80-entry table) read one element out of bounds. The
// decoding table spans '+'(43)..'z'(122); anything outside must yield -1.
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

// A boundary character embedded in otherwise-valid input must be skipped,
// not corrupt the output or read past the table.
TEST(Regression, DecodeBoundaryCharInStream)
{
	EXPECT_EQ(decode("Zm9v{"), "foo");
	EXPECT_EQ(decode("Z{m}9~v"), "foo");
}

// 96ce1a3 (Jakub Wilk, SF bug #2): the decoder fragment was a signed char,
// which mishandled high values; it is now an int. Decoding every byte value
// round-trips exactly, which would fail under the sign bug.
TEST(Regression, DecoderHandlesAllFragmentValues)
{
	std::string in;
	for (int i = 0; i < 256; ++i)
		in.push_back(static_cast<char>(i));
	EXPECT_EQ(decode(b64test::encode(in)), in);
}

// High-bit input bytes (0x80..0xFF) become negative signed chars; they must
// be rejected by base64_decode_value rather than indexing the table.
TEST(Regression, DecodeValueHighBitBytes)
{
	for (int b = 0x80; b <= 0xFF; ++b)
		EXPECT_EQ(base64_decode_value(static_cast<signed char>(b)), -1)
			<< "byte=0x" << std::hex << b;
}

// The padding character '=' maps to the sentinel -2 in the table (distinct
// from -1 for "invalid"); both are treated as "skip" by the block decoder.
TEST(Regression, DecodeValueEqualsSentinel)
{
	EXPECT_EQ(base64_decode_value('='), -2);
}

// integer-overflows patches (d54125c) / v2.0.0 base64_encode_length: the
// length helper must detect size_t overflow and return 0 rather than wrap
// to a too-small value (which would cause a heap overflow in callers).
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
