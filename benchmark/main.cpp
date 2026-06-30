/*
Throughput benchmarks for libb64 v2.0.0 across a range of input sizes and
data types. Each case reports bytes_per_second (of plaintext processed).
*/

#include <benchmark/benchmark.h>

#include <cstdint>
#include <string>
#include <vector>

extern "C" {
#include <b64/cencode.h>
#include <b64/cdecode.h>
}

namespace {

enum class DataType { Binary, Text, Zeros };

// Deterministic data generators so runs are comparable.
std::string make_data(DataType type, size_t n)
{
	std::string s(n, '\0');
	switch (type)
	{
	case DataType::Zeros:
		break;  // already all-zero
	case DataType::Text:
	{
		static const char words[] =
			"the quick brown fox jumps over the lazy dog. ";
		const size_t wl = sizeof(words) - 1;
		for (size_t i = 0; i < n; ++i)
			s[i] = words[i % wl];
		break;
	}
	case DataType::Binary:
	{
		uint64_t x = 0x9e3779b97f4a7c15ull;  // xorshift, full byte range
		for (size_t i = 0; i < n; ++i)
		{
			x ^= x << 13; x ^= x >> 7; x ^= x << 17;
			s[i] = static_cast<char>(x & 0xff);
		}
		break;
	}
	}
	return s;
}

std::string encode(const std::string& in, size_t chars_per_line = 0)
{
	base64_encodestate s;
	base64_init_encodestate(&s);
	s.chars_per_line = chars_per_line;
	std::vector<char> out(base64_encode_length(in.size(), &s) + 1, '\0');
	size_t n = base64_encode_block(in.data(), in.size(), out.data(), &s);
	n += base64_encode_blockend(out.data() + n, &s);
	return std::string(out.data(), n);
}

} // namespace

static void BM_Encode(benchmark::State& state, DataType type)
{
	const size_t n = static_cast<size_t>(state.range(0));
	const std::string in = make_data(type, n);
	base64_encodestate sizing;
	base64_init_encodestate(&sizing);
	std::vector<char> out(base64_encode_length(n, &sizing) + 1);

	for (auto _ : state)
	{
		base64_encodestate s;
		base64_init_encodestate(&s);
		size_t m = base64_encode_block(in.data(), in.size(), out.data(), &s);
		m += base64_encode_blockend(out.data() + m, &s);
		benchmark::DoNotOptimize(out.data());
		benchmark::DoNotOptimize(m);
	}
	state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
	                        static_cast<int64_t>(n));
}

static void BM_Decode(benchmark::State& state, DataType type, size_t cpl)
{
	const size_t n = static_cast<size_t>(state.range(0));
	const std::string enc = encode(make_data(type, n), cpl);
	std::vector<char> out(base64_decode_maxlength(enc.size()) + 1);

	for (auto _ : state)
	{
		base64_decodestate s;
		base64_init_decodestate(&s);
		size_t m = base64_decode_block(enc.data(), enc.size(),
		                               out.data(), &s);
		benchmark::DoNotOptimize(out.data());
		benchmark::DoNotOptimize(m);
	}
	state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
	                        static_cast<int64_t>(n));
}

// 64 B .. 1 GiB
#define B64_SIZES ->RangeMultiplier(16)->Range(64, int64_t{1} << 30)

BENCHMARK_CAPTURE(BM_Encode, binary, DataType::Binary) B64_SIZES;
BENCHMARK_CAPTURE(BM_Encode, text,   DataType::Text)   B64_SIZES;
BENCHMARK_CAPTURE(BM_Encode, zeros,  DataType::Zeros)  B64_SIZES;

BENCHMARK_CAPTURE(BM_Decode, binary, DataType::Binary, 0) B64_SIZES;
BENCHMARK_CAPTURE(BM_Decode, text,   DataType::Text,   0) B64_SIZES;
BENCHMARK_CAPTURE(BM_Decode, zeros,  DataType::Zeros,  0) B64_SIZES;

// MIME-style input with line breaks every 76 chars: the decoder skips the
// newlines, so this shows the cost of wrapped/whitespace-laden input.
BENCHMARK_CAPTURE(BM_Decode, binary_wrapped76, DataType::Binary, 76) B64_SIZES;

// main() comes from the linked benchmark main library (see CMakeLists).
