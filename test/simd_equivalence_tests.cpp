/* The dispatched (SIMD) encode path must be byte-identical to the scalar
   core across sizes, chunkings and data — the SIMD safety net. On scalar-only
   builds both pointers run the same code, so this still passes. */

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
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

namespace {

typedef size_t (*decode_block_fn)(const char*, size_t, void*,
                                  base64_decodestate*);

std::string decode_oneshot(const std::string& enc, decode_block_fn block)
{
	base64_decodestate s;
	base64_init_decodestate(&s);
	std::vector<char> out(base64_decode_maxlength(enc.size()) + 1, '\0');
	size_t n = block(enc.data(), enc.size(), out.data(), &s);
	return std::string(out.data(), n);
}

std::string decode_chunked(const std::string& enc, size_t chunk,
                           decode_block_fn block)
{
	base64_decodestate s;
	base64_init_decodestate(&s);
	std::vector<char> out(base64_decode_maxlength(enc.size()) + 1, '\0');
	size_t n = 0;
	for (size_t off = 0; off < enc.size(); off += chunk)
	{
		size_t len = std::min(chunk, enc.size() - off);
		n += block(enc.data() + off, len, out.data() + n, &s);
	}
	return std::string(out.data(), n);
}

} // namespace

TEST(SimdEquivalence, DecodeOneShotMatchesScalar)
{
	for (size_t n = 0; n <= 600; ++n)
	{
		std::string enc = b64test::encode(b64test::pattern(n));
		EXPECT_EQ(decode_oneshot(enc, base64_decode_block),
		          decode_oneshot(enc, base64_decode_block_scalar)) << "n=" << n;
	}
}

TEST(SimdEquivalence, DecodeChunkedMatchesScalar)
{
	const std::string enc = b64test::encode(b64test::pattern(300));
	for (size_t chunk : {size_t(1), size_t(2), size_t(4), size_t(7),
	                     size_t(16), size_t(17), size_t(64)})
	{
		EXPECT_EQ(decode_chunked(enc, chunk, base64_decode_block),
		          decode_chunked(enc, chunk, base64_decode_block_scalar))
			<< "chunk=" << chunk;
	}
}

// Whitespace/padding force the SIMD path to bail to scalar mid-stream; the
// result must still equal the scalar-only decode.
TEST(SimdEquivalence, DecodeWithWhitespaceMatchesScalar)
{
	std::string wrapped = b64test::encode(b64test::pattern(400), 76);
	EXPECT_EQ(decode_oneshot(wrapped, base64_decode_block),
	          decode_oneshot(wrapped, base64_decode_block_scalar));

	const std::string spaced = "Z m 9 v Ym Fy\n\n";
	EXPECT_EQ(decode_oneshot(spaced, base64_decode_block),
	          decode_oneshot(spaced, base64_decode_block_scalar));
}

namespace {

// Sprinkle whitespace into a base64 string at pseudo-random positions, so the
// pruning path meets every alignment of skipped bytes within a vector.
std::string sprinkle_ws(const std::string& enc, unsigned seed)
{
	static const char ws[] = {' ', '\t', '\n', '\r'};
	std::string out;
	unsigned x = seed;
	for (char c : enc)
	{
		out += c;
		x = x * 1103515245u + 12345u;
		if ((x >> 29) < 3u)            /* insert with ~3/8 probability */
			out += ws[(x >> 16) & 3u];
	}
	return out;
}

} // namespace

// The SIMD whitespace-pruning decode must equal the scalar core across wrap
// widths (incl. ones that aren't a multiple of 4 or 16) and lengths that land
// on every FIFO/partial-group boundary.
TEST(SimdEquivalence, DecodePrunedWrappedMatchesScalar)
{
	const size_t widths[] = {1, 2, 3, 4, 7, 16, 19, 20, 32, 64, 76, 77, 128};
	for (size_t width : widths)
		for (size_t n = 0; n <= 410; ++n)
		{
			std::string wrapped = b64test::encode(b64test::pattern(n), width);
			EXPECT_EQ(decode_oneshot(wrapped, base64_decode_block),
			          decode_oneshot(wrapped, base64_decode_block_scalar))
				<< "width=" << width << " n=" << n;
		}
}

TEST(SimdEquivalence, DecodeSprinkledWhitespaceMatchesScalar)
{
	const unsigned seeds[] = {1u, 7u, 13u, 99u, 12345u};
	for (size_t n = 0; n <= 300; ++n)
	{
		std::string enc = b64test::encode(b64test::pattern(n));
		for (unsigned seed : seeds)
		{
			std::string dirty = sprinkle_ws(enc, seed);
			EXPECT_EQ(decode_oneshot(dirty, base64_decode_block),
			          decode_oneshot(dirty, base64_decode_block_scalar))
				<< "n=" << n << " seed=" << seed;
		}
	}
}

// Streaming wrapped input in arbitrary chunks exercises the prune -> scalar
// handoff and the mid-group decoder state carried between calls.
TEST(SimdEquivalence, DecodeWrappedChunkedMatchesScalar)
{
	const size_t chunks[] = {1, 2, 3, 5, 7, 16, 17, 31, 32, 48, 64, 77};
	std::string wrapped = b64test::encode(b64test::pattern(500), 76);
	for (size_t chunk : chunks)
		EXPECT_EQ(decode_chunked(wrapped, chunk, base64_decode_block),
		          decode_chunked(wrapped, chunk, base64_decode_block_scalar))
			<< "chunk=" << chunk;
}

TEST(SimdEquivalence, DecodeUnalignedMatchesScalar)
{
	const std::string enc = b64test::encode(b64test::pattern(257));
	for (size_t off = 0; off < 16; ++off)
	{
		std::vector<char> inbuf(off + enc.size());
		std::memcpy(inbuf.data() + off, enc.data(), enc.size());
		const char* in = inbuf.data() + off;  /* unaligned by `off` */
		const size_t cap = base64_decode_maxlength(enc.size()) + 1;

		base64_decodestate s1;
		base64_init_decodestate(&s1);
		std::vector<char> o1(off + cap, '\0');
		size_t a = base64_decode_block(in, enc.size(), o1.data() + off, &s1);

		base64_decodestate s2;
		base64_init_decodestate(&s2);
		std::vector<char> o2(cap, '\0');
		size_t b = base64_decode_block_scalar(in, enc.size(), o2.data(), &s2);

		EXPECT_EQ(std::string(o1.data() + off, a),
		          std::string(o2.data(), b)) << "off=" << off;
	}
}
