/* The dispatched (SIMD) encode path must be byte-identical to the scalar
   core across sizes, chunkings and data — the SIMD safety net. On scalar-only
   builds both pointers run the same code, so this still passes. */

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "b64_internal.h"
#include "b64_test_util.h"

extern "C" {
#include <b64/cencode.h>
}

namespace {

typedef size_t (*encode_block_fn)(const void*, size_t, char*,
                                  base64_encodestate*);

std::string encode_oneshot(const std::string& in, encode_block_fn block)
{
	base64_encodestate s;
	base64_init_encodestate(&s);
	std::vector<char> out(base64_encode_length(in.size(), &s) + 1, '\0');
	size_t n = block(in.data(), in.size(), out.data(), &s);
	n += base64_encode_blockend(out.data() + n, &s);
	return std::string(out.data(), n);
}

std::string encode_chunked(const std::string& in, size_t chunk,
                           encode_block_fn block)
{
	base64_encodestate s;
	base64_init_encodestate(&s);
	std::vector<char> out(base64_encode_length(in.size(), &s) + 1, '\0');
	size_t n = 0;
	for (size_t off = 0; off < in.size(); off += chunk)
	{
		size_t len = std::min(chunk, in.size() - off);
		n += block(in.data() + off, len, out.data() + n, &s);
	}
	n += base64_encode_blockend(out.data() + n, &s);
	return std::string(out.data(), n);
}

} // namespace

TEST(SimdEquivalence, EncodeOneShotMatchesScalar)
{
	for (size_t n = 0; n <= 600; ++n)
	{
		std::string in = b64test::pattern(n);
		EXPECT_EQ(encode_oneshot(in, base64_encode_block),
		          encode_oneshot(in, base64_encode_block_scalar)) << "n=" << n;
	}
}

// Unaligned input and output buffers must produce identical output. The
// SIMD path uses unaligned loads/stores; malloc'd test data above happens to
// be 16-byte aligned, so this offsets both pointers to exercise that.
TEST(SimdEquivalence, EncodeUnalignedMatchesScalar)
{
	const size_t n = 257;
	for (size_t off = 0; off < 16; ++off)
	{
		std::vector<char> inbuf(off + n);
		for (size_t i = 0; i < n; ++i)
			inbuf[off + i] = static_cast<char>((i * 73u + 41u) & 0xff);
		const char* in = inbuf.data() + off;  /* unaligned by `off` */

		base64_encodestate ss;
		base64_init_encodestate(&ss);
		const size_t cap = base64_encode_length(n, &ss) + 1;

		base64_encodestate s1;
		base64_init_encodestate(&s1);
		std::vector<char> o1(off + cap, '\0');
		size_t a = base64_encode_block(in, n, o1.data() + off, &s1);
		a += base64_encode_blockend(o1.data() + off + a, &s1);

		base64_encodestate s2;
		base64_init_encodestate(&s2);
		std::vector<char> o2(cap, '\0');
		size_t b = base64_encode_block_scalar(in, n, o2.data(), &s2);
		b += base64_encode_blockend(o2.data() + b, &s2);

		EXPECT_EQ(std::string(o1.data() + off, a),
		          std::string(o2.data(), b)) << "off=" << off;
	}
}

TEST(SimdEquivalence, EncodeChunkedMatchesScalar)
{
	const std::string in = b64test::pattern(300);
	for (size_t chunk : {size_t(1), size_t(2), size_t(3), size_t(5),
	                     size_t(16), size_t(48), size_t(49), size_t(64)})
	{
		EXPECT_EQ(encode_chunked(in, chunk, base64_encode_block),
		          encode_chunked(in, chunk, base64_encode_block_scalar))
			<< "chunk=" << chunk;
	}
}
