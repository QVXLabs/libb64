/*
RFC 4648 compliance tests.

The encoder is strict/canonical (standard alphabet, correct padding). The
decoder is deliberately LENIENT: it skips any byte outside the alphabet and
does not validate padding or the trailing-bit canonical form. The tests
below pin the encoder's compliant behaviour and *characterise* the decoder's
leniency, citing the RFC clause each case relates to, so any future change in
strictness is caught here.
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

// RFC 4648 section 3.3 says implementations MUST reject characters outside
// the alphabet unless the embedding spec says otherwise. libb64 SKIPS them.
TEST(Rfc4648, DecoderSkipsNonAlphabetCharacters_Lenient)
{
	EXPECT_EQ(decode("Zm@9v!"), "foo");
	EXPECT_EQ(decode("Z*m*9*v"), "foo");
}

// Line breaks / whitespace are skipped (MIME, RFC 2045, expects this; strict
// RFC 4648 without an embedding spec would reject them).
TEST(Rfc4648, DecoderSkipsWhitespace_Lenient)
{
	EXPECT_EQ(decode("Zm9v\r\n"), "foo");
	EXPECT_EQ(decode("Z m 9 v"), "foo");
	EXPECT_EQ(decode("\tZm9v\n"), "foo");
}

// Padding is treated as optional: '=' maps to a skip, so an unpadded or
// over-/under-padded string still decodes. Strict RFC 4648 section 3.2/3.3
// would require correct padding.
TEST(Rfc4648, DecoderTreatsPaddingAsOptional_Lenient)
{
	EXPECT_EQ(decode("Zg"),     "f");   // no padding
	EXPECT_EQ(decode("Zg=="),   "f");   // canonical
	EXPECT_EQ(decode("Zm8"),    "fo");
	EXPECT_EQ(decode("Zm9vYg"), "foob");
}

// RFC 4648 section 3.5: a strict decoder MAY reject non-zero bits in the
// final (partial) quantum. libb64 masks them off, so several distinct final
// characters all decode to the same byte.
TEST(Rfc4648, DecoderAcceptsNonCanonicalTrailingBits_Lenient)
{
	// 'Zg==' is canonical for "f"; 'Zh=='/'Zi==' carry non-zero junk bits.
	EXPECT_EQ(decode("Zg=="), "f");
	EXPECT_EQ(decode("Zh=="), "f");
	EXPECT_EQ(decode("Zi=="), "f");
}
