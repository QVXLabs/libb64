/* Tests for the C++ wrappers (base64::encoder/decoder), stream + block. */

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

#include <b64/encode.h>
#include <b64/decode.h>

namespace {

std::string stream_encode(const std::string& in)
{
	base64::encoder e;
	std::istringstream is(in);
	std::ostringstream os;
	e.encode(is, os);
	return os.str();
}

std::string stream_decode(const std::string& in)
{
	base64::decoder d;
	std::istringstream is(in);
	std::ostringstream os;
	d.decode(is, os);
	return os.str();
}

} // namespace

TEST(CppWrapper, StreamEncodeKnownVector)
{
	EXPECT_EQ(stream_encode("hello world"), "aGVsbG8gd29ybGQ=");
}

TEST(CppWrapper, StreamRoundTrip)
{
	const std::string in = "The quick brown fox jumps over the lazy dog";
	EXPECT_EQ(stream_decode(stream_encode(in)), in);
}

TEST(CppWrapper, BlockApiRoundTrip)
{
	base64::encoder e;
	const std::string in = "abcdefg";
	std::vector<char> enc(64, '\0');
	std::streamsize en = e.encode(in.data(),
	                              static_cast<std::streamsize>(in.size()),
	                              enc.data());
	en += e.encode_end(enc.data() + en);

	base64::decoder d;
	std::vector<char> dec(64, '\0');
	std::streamsize dn = d.decode(enc.data(), en, dec.data());
	EXPECT_EQ(std::string(dec.data(), dn), in);
}

// Non-positive length is a no-op (not a huge size_t over-read).
TEST(CppWrapper, NonPositiveLengthIsNoOp)
{
	char out[16];

	base64::encoder e;
	EXPECT_EQ(e.encode("abc", 0, out), 0);
	EXPECT_EQ(e.encode("abc", -5, out), 0);

	base64::decoder d;
	EXPECT_EQ(d.decode("YWJj", 0, out), 0);
	EXPECT_EQ(d.decode("YWJj", -5, out), 0);
}

// Regression: the decoder constructor was empty (uninitialized _state); it
// now calls base64_init_decodestate(), so a fresh decoder works.
TEST(CppWrapper, DecoderInitializedByConstructor_Regression)
{
	base64::decoder d;
	const std::string enc = "aGVsbG8gd29ybGQ=";  // "hello world"
	std::vector<char> out(64, '\0');
	std::streamsize n = d.decode(enc.data(),
	                             static_cast<std::streamsize>(enc.size()),
	                             out.data());
	EXPECT_EQ(std::string(out.data(), n), "hello world");
}
