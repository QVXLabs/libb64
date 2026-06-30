/* Independent ground-truth check for the scalar table paths: encode then
   decode must reproduce the input. The SIMD-equivalence suite only proves the
   SIMD path equals the scalar core; these tests catch a bug shared by both by
   round-tripping through the scalar core (12-bit encode table, 4-table SWAR
   decode) directly, plus the public dispatched path, across large inputs and
   data shapes the smaller equivalence sweeps don't reach. */

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "b64_internal.h"
#include "b64_test_util.h"

namespace {

// One-shot encode through a chosen block function, with optional wrapping.
std::string encode_with(const std::string& in, size_t chars_per_line,
                        size_t (*block)(const void*, size_t, char*,
                                        base64_encodestate*))
{
	base64_encodestate s;
	base64_init_encodestate(&s);
	s.chars_per_line = chars_per_line;
	std::vector<char> out(base64_encode_length(in.size(), &s) + 1, '\0');
	size_t n = block(in.data(), in.size(), out.data(), &s);
	n += base64_encode_blockend(out.data() + n, &s);
	return std::string(out.data(), n);
}

std::string decode_with(const std::string& enc,
                        size_t (*block)(const char*, size_t, void*,
                                        base64_decodestate*))
{
	base64_decodestate s;
	base64_init_decodestate(&s);
	std::vector<char> out(base64_decode_maxlength(enc.size()) + 1, '\0');
	size_t n = block(enc.data(), enc.size(), out.data(), &s);
	return std::string(out.data(), n);
}

// A few data shapes: pseudo-random binary, printable text, all zeros.
std::string shaped(size_t n, int kind)
{
	std::string s;
	s.reserve(n);
	for (size_t i = 0; i < n; ++i)
	{
		char c;
		if (kind == 0)
			c = static_cast<char>((i * 73u + 41u) & 0xff);
		else if (kind == 1)
			c = static_cast<char>('!' + (i % 94u));
		else
			c = 0;
		s.push_back(c);
	}
	return s;
}

} // namespace

// Scalar core round-trips exactly across sizes 0..4096 and all data shapes.
TEST(RoundTrip, ScalarSmallSizes)
{
	for (int kind = 0; kind < 3; ++kind)
		for (size_t n = 0; n <= 4096; ++n)
		{
			std::string in = shaped(n, kind);
			std::string enc = encode_with(in, 0, base64_encode_block_scalar);
			std::string dec = decode_with(enc, base64_decode_block_scalar);
			ASSERT_EQ(dec, in) << "kind=" << kind << " n=" << n;
		}
}

// Large buffers exercise the bulk loops well past any cache step.
TEST(RoundTrip, ScalarLargeBuffers)
{
	for (int kind = 0; kind < 3; ++kind)
		for (size_t n : {size_t(65537), size_t(1u << 20), size_t(3000001)})
		{
			std::string in = shaped(n, kind);
			std::string enc = encode_with(in, 0, base64_encode_block_scalar);
			std::string dec = decode_with(enc, base64_decode_block_scalar);
			ASSERT_EQ(dec, in) << "kind=" << kind << " n=" << n;
		}
}

// The scalar decoder tolerates line wrapping (per-char path skips newlines),
// so a wrapped encode must still round-trip through the scalar core.
TEST(RoundTrip, ScalarWrapped)
{
	for (size_t width : {size_t(4), size_t(64), size_t(76), size_t(77)})
		for (size_t n : {size_t(0), size_t(1), size_t(1000), size_t(100000)})
		{
			std::string in = shaped(n, 0);
			std::string enc = encode_with(in, width, base64_encode_block_scalar);
			std::string dec = decode_with(enc, base64_decode_block_scalar);
			ASSERT_EQ(dec, in) << "width=" << width << " n=" << n;
		}
}

// The public dispatched path (SIMD where available) round-trips identically.
TEST(RoundTrip, PublicLargeBuffers)
{
	for (int kind = 0; kind < 3; ++kind)
		for (size_t n : {size_t(65537), size_t(1u << 20), size_t(3000001)})
		{
			std::string in = shaped(n, kind);
			ASSERT_EQ(b64test::decode(b64test::encode(in)), in)
				<< "kind=" << kind << " n=" << n;
		}
}
