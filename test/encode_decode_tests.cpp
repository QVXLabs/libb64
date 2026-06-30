/*
Functional tests for the libb64 v2.0.0 C API: known vectors, round-trips,
chunked (stateful) encode/decode, and the buffer-length helpers.
*/

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "b64_test_util.h"

using b64test::encode;
using b64test::decode;
using b64test::pattern;

namespace {

struct Vector { const char* plain; const char* encoded; };

// RFC 4648 section 10 test vectors.
const Vector kRfc4648[] = {
	{"",       ""},
	{"f",      "Zg=="},
	{"fo",     "Zm8="},
	{"foo",    "Zm9v"},
	{"foob",   "Zm9vYg=="},
	{"fooba",  "Zm9vYmE="},
	{"foobar", "Zm9vYmFy"},
};

// Assorted well-known vectors (Wikipedia / common references).
const Vector kClassic[] = {
	{"M",                                           "TQ=="},
	{"Ma",                                          "TWE="},
	{"Man",                                         "TWFu"},
	{"su",                                          "c3U="},
	{"sure",                                        "c3VyZQ=="},
	{"sure.",                                       "c3VyZS4="},
	{"asure.",                                      "YXN1cmUu"},
	{"easure.",                                     "ZWFzdXJlLg=="},
	{"leasure.",                                    "bGVhc3VyZS4="},
	{"pleasure.",                                   "cGxlYXN1cmUu"},
	{"Hello, World!",                               "SGVsbG8sIFdvcmxkIQ=="},
	{"The quick brown fox jumps over the lazy dog",
	 "VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZw=="},
};

} // namespace

TEST(Encode, Rfc4648Vectors)
{
	for (const Vector& v : kRfc4648)
		EXPECT_EQ(encode(v.plain), v.encoded) << "plain=\"" << v.plain << "\"";
}

TEST(Encode, ClassicVectors)
{
	for (const Vector& v : kClassic)
		EXPECT_EQ(encode(v.plain), v.encoded) << "plain=\"" << v.plain << "\"";
}

TEST(Decode, Rfc4648Vectors)
{
	for (const Vector& v : kRfc4648)
		EXPECT_EQ(decode(v.encoded), v.plain) << "encoded=\"" << v.encoded << "\"";
}

TEST(Decode, ClassicVectors)
{
	for (const Vector& v : kClassic)
		EXPECT_EQ(decode(v.encoded), v.plain) << "encoded=\"" << v.encoded << "\"";
}

TEST(RoundTrip, AllByteValues)
{
	std::string in;
	in.reserve(256);
	for (int i = 0; i < 256; ++i)
		in.push_back(static_cast<char>(i));
	EXPECT_EQ(decode(encode(in)), in);
}

TEST(RoundTrip, AllLengthsUpTo512)
{
	// Covers every residue class mod 3 (the three encoder steps) and the
	// padding cases, across a wide range of lengths.
	for (size_t n = 0; n <= 512; ++n)
	{
		std::string in = pattern(n);
		EXPECT_EQ(decode(encode(in)), in) << "length=" << n;
	}
}

TEST(RoundTrip, EmbeddedNulBytes)
{
	std::string in("a\0b\0\0c", 6);
	EXPECT_EQ(decode(encode(in)), in);
	EXPECT_EQ(in.size(), 6u);
}

TEST(Length, EncodeLengthMatchesActualOutput)
{
	for (size_t n = 0; n <= 200; ++n)
	{
		base64_encodestate s;
		base64_init_encodestate(&s);
		size_t predicted = base64_encode_length(n, &s);
		EXPECT_EQ(encode(pattern(n)).size(), predicted) << "length=" << n;
	}
}

TEST(Length, DecodeMaxlengthIsUpperBound)
{
	for (size_t n = 0; n <= 200; ++n)
	{
		std::string enc = encode(pattern(n));
		size_t bound = base64_decode_maxlength(enc.size());
		EXPECT_GE(bound, n) << "plain length=" << n;
	}
}

// Encoding the input in arbitrary chunks must equal encoding it all at once;
// this exercises the step_A/step_B/step_C state machine across calls.
TEST(Chunked, EncodeMatchesOneShot)
{
	const std::string in = pattern(257);
	const std::string expected = encode(in);

	for (size_t chunk = 1; chunk <= 8; ++chunk)
	{
		base64_encodestate s;
		base64_init_encodestate(&s);
		std::vector<char> out(base64_encode_length(in.size(), &s) + 1, '\0');
		size_t n = 0;
		for (size_t off = 0; off < in.size(); off += chunk)
		{
			size_t len = std::min(chunk, in.size() - off);
			n += base64_encode_block(in.data() + off, len, out.data() + n, &s);
		}
		n += base64_encode_blockend(out.data() + n, &s);
		EXPECT_EQ(std::string(out.data(), n), expected) << "chunk=" << chunk;
	}
}

namespace {

// Encode with line wrapping, sizing the buffer generously (independent of
// the length helper) so a miscount surfaces as a failed expectation below
// rather than as undefined behaviour.
std::string encode_wrapped(const std::string& in, size_t cpl, size_t* predicted)
{
	base64_encodestate s;
	base64_init_encodestate(&s);
	s.chars_per_line = cpl;
	*predicted = base64_encode_length(in.size(), &s);
	std::vector<char> out(*predicted + 64, '\0');
	size_t n = base64_encode_block(in.data(), in.size(), out.data(), &s);
	n += base64_encode_blockend(out.data() + n, &s);
	return std::string(out.data(), n);
}

} // namespace

// Line wrapping (2780725 "fix a few bugs in line breaking algorithm"): with
// chars_per_line = N, no output line exceeds N characters, removing the
// newlines reproduces the unwrapped encoding, base64_encode_length predicts
// the wrapped size exactly, and the wrapped text still decodes (newlines are
// skipped by the decoder).
TEST(LineWrap, WrapsCorrectlyAndRoundTrips)
{
	for (size_t cpl : {size_t(1), size_t(4), size_t(16), size_t(64), size_t(76)})
	{
		for (size_t n : {size_t(0), size_t(1), size_t(3), size_t(10),
		                 size_t(48), size_t(57), size_t(120)})
		{
			std::string in = pattern(n);
			size_t predicted = 0;
			std::string wrapped = encode_wrapped(in, cpl, &predicted);

			std::string stripped;
			size_t run = 0, maxrun = 0;
			for (char c : wrapped)
			{
				if (c == '\n') { run = 0; }
				else { stripped += c; if (++run > maxrun) maxrun = run; }
			}

			const std::string tag =
				"cpl=" + std::to_string(cpl) + " n=" + std::to_string(n);
			EXPECT_EQ(wrapped.size(), predicted) << tag;   // helper sizing
			EXPECT_EQ(stripped, encode(in)) << tag;        // same payload
			EXPECT_LE(maxrun, cpl) << tag;                 // no over-long line
			EXPECT_EQ(decode(wrapped), in) << tag;         // round-trips
		}
	}
}

// Likewise, decoding in arbitrary chunks must equal one-shot decoding.
TEST(Chunked, DecodeMatchesOneShot)
{
	const std::string in = pattern(257);
	const std::string enc = encode(in);

	for (size_t chunk = 1; chunk <= 8; ++chunk)
	{
		base64_decodestate s;
		base64_init_decodestate(&s);
		std::vector<char> out(base64_decode_maxlength(enc.size()) + 1, '\0');
		size_t n = 0;
		for (size_t off = 0; off < enc.size(); off += chunk)
		{
			size_t len = std::min(chunk, enc.size() - off);
			n += base64_decode_block(enc.data() + off, len, out.data() + n, &s);
		}
		EXPECT_EQ(std::string(out.data(), n), in) << "chunk=" << chunk;
	}
}
