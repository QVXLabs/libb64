/* Regression tests for historical libb64 bugs (commit refs inline). */

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

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
