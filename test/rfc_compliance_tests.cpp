/*
RFC 4648: the encoder is strict/canonical; the decoder is deliberately
lenient (skips non-alphabet bytes, doesn't validate padding or canonical
trailing bits). These pin the encoder and characterise the leniency.
*/

#include <gtest/gtest.h>

#include <string>

#include "b64_test_util.h"

using b64test::encode;
using b64test::decode;

// --- Encoder: compliant (RFC 4648 sections 4 & 3.2) ------------------------

// Standard alphabet uses '+' and '/' (section 4), not the URL-safe '-'/'_'.
TEST(Rfc4648, EncoderUsesStandardAlphabet)
{
	EXPECT_EQ(encode(std::string("\xff\xff\xff", 3)), "////");
	EXPECT_EQ(encode(std::string("\xfb\x00\x00", 3)), "+wAA");
	EXPECT_EQ(encode(std::string("\xfb\xff\xbf", 3)), "+/+/");
}

// Output is padded with '=' to a multiple of four characters (section 3.2).
TEST(Rfc4648, EncoderEmitsCanonicalPadding)
{
	EXPECT_EQ(encode("f").size() % 4, 0u);
	EXPECT_EQ(encode("f"),  "Zg==");   // 2 pad chars
	EXPECT_EQ(encode("fo"), "Zm8=");   // 1 pad char
	EXPECT_EQ(encode("foo"), "Zm9v");  // 0 pad chars
	EXPECT_EQ(encode(std::string("\x00", 1)), "AA==");
}

// --- Decoder: lenient (documented deviations from strict RFC 4648) ---------

// RFC 4648 §3.3: non-alphabet chars MUST be rejected; libb64 skips them.
TEST(Rfc4648, DecoderSkipsNonAlphabetCharacters_Lenient)
{
	EXPECT_EQ(decode("Zm@9v!"), "foo");
	EXPECT_EQ(decode("Z*m*9*v"), "foo");
}

// Whitespace/newlines skipped (MIME-friendly; strict §3.3 would reject).
TEST(Rfc4648, DecoderSkipsWhitespace_Lenient)
{
	EXPECT_EQ(decode("Zm9v\r\n"), "foo");
	EXPECT_EQ(decode("Z m 9 v"), "foo");
	EXPECT_EQ(decode("\tZm9v\n"), "foo");
}

// Padding optional: '=' is skipped, so unpadded/mispadded input still
// decodes (strict §3.2 requires correct padding).
TEST(Rfc4648, DecoderTreatsPaddingAsOptional_Lenient)
{
	EXPECT_EQ(decode("Zg"),     "f");   // no padding
	EXPECT_EQ(decode("Zg=="),   "f");   // canonical
	EXPECT_EQ(decode("Zm8"),    "fo");
	EXPECT_EQ(decode("Zm9vYg"), "foob");
}

// §3.5: a strict decoder MAY reject non-zero trailing bits; libb64 masks
// them, so several final chars decode to the same byte.
TEST(Rfc4648, DecoderAcceptsNonCanonicalTrailingBits_Lenient)
{
	// 'Zg==' is canonical for "f"; 'Zh=='/'Zi==' carry non-zero junk bits.
	EXPECT_EQ(decode("Zg=="), "f");
	EXPECT_EQ(decode("Zh=="), "f");
	EXPECT_EQ(decode("Zi=="), "f");
}
