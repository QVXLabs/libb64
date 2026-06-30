/*
Microbenchmarks for libb64 v2.0.0 encode/decode over a fixed buffer.
*/

#include <benchmark/benchmark.h>

#include <string>
#include <vector>

extern "C" {
#include <b64/cencode.h>
#include <b64/cdecode.h>
}

namespace {

std::string make_input(size_t n)
{
	std::string s;
	s.reserve(n);
	for (size_t i = 0; i < n; ++i)
		s.push_back(static_cast<char>(i * 31u + 7u));
	return s;
}

} // namespace

static void BM_Encode(benchmark::State& state)
{
	const std::string in = make_input(static_cast<size_t>(state.range(0)));
	base64_encodestate sizing;
	base64_init_encodestate(&sizing);
	std::vector<char> out(base64_encode_length(in.size(), &sizing) + 1);

	for (auto _ : state)
	{
		base64_encodestate s;
		base64_init_encodestate(&s);
		size_t n = base64_encode_block(in.data(), in.size(), out.data(), &s);
		n += base64_encode_blockend(out.data() + n, &s);
		benchmark::DoNotOptimize(out.data());
		benchmark::DoNotOptimize(n);
		benchmark::ClobberMemory();
	}
	state.SetBytesProcessed(state.iterations() * in.size());
}
BENCHMARK(BM_Encode)->Arg(1024)->Arg(64 * 1024);

static void BM_Decode(benchmark::State& state)
{
	const std::string in = make_input(static_cast<size_t>(state.range(0)));

	base64_encodestate es;
	base64_init_encodestate(&es);
	std::vector<char> enc(base64_encode_length(in.size(), &es) + 1);
	size_t enc_len = base64_encode_block(in.data(), in.size(),
	                                     enc.data(), &es);
	enc_len += base64_encode_blockend(enc.data() + enc_len, &es);

	std::vector<char> out(base64_decode_maxlength(enc_len) + 1);

	for (auto _ : state)
	{
		base64_decodestate s;
		base64_init_decodestate(&s);
		size_t n = base64_decode_block(enc.data(), enc_len, out.data(), &s);
		benchmark::DoNotOptimize(out.data());
		benchmark::DoNotOptimize(n);
		benchmark::ClobberMemory();
	}
	state.SetBytesProcessed(state.iterations() * in.size());
}
BENCHMARK(BM_Decode)->Arg(1024)->Arg(64 * 1024);

BENCHMARK_MAIN();
