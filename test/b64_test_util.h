/*
Shared helpers for the libb64 test suite. Thin std::string wrappers over
the v2.0.0 C API so individual tests stay focused on behaviour.
*/

#ifndef B64_TEST_UTIL_H
#define B64_TEST_UTIL_H

#include <cstddef>
#include <string>
#include <vector>

extern "C" {
#include <b64/cencode.h>
#include <b64/cdecode.h>
}

namespace b64test {

// Encode a buffer in one shot. chars_per_line == 0 (the default) means no
// line wrapping; a non-zero value exercises the line-break path.
inline std::string encode(const std::string& in, size_t chars_per_line = 0)
{
	base64_encodestate state;
	base64_init_encodestate(&state);
	state.chars_per_line = chars_per_line;
	std::vector<char> out(base64_encode_length(in.size(), &state) + 1, '\0');
	size_t n = base64_encode_block(in.data(), in.size(), out.data(), &state);
	n += base64_encode_blockend(out.data() + n, &state);
	return std::string(out.data(), n);
}

inline std::string decode(const std::string& in)
{
	base64_decodestate state;
	base64_init_decodestate(&state);
	std::vector<char> out(base64_decode_maxlength(in.size()) + 1, '\0');
	size_t n = base64_decode_block(in.data(), in.size(), out.data(), &state);
	return std::string(out.data(), n);
}

// Deterministic pseudo-random byte string of a given length.
inline std::string pattern(size_t n)
{
	std::string s;
	s.reserve(n);
	for (size_t i = 0; i < n; ++i)
		s.push_back(static_cast<char>((i * 73u + 41u) & 0xff));
	return s;
}

} // namespace b64test

#endif // B64_TEST_UTIL_H
