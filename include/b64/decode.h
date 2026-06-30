// :mode=c++:
/*
decode.h - c++ wrapper for a base64 decoding algorithm

This is part of the libb64 project, and has been placed in the public domain.
For details, see http://sourceforge.net/projects/libb64
*/
#ifndef BASE64_DECODE_H
#define BASE64_DECODE_H

#include <iostream>

namespace base64
{
	extern "C"
	{
		#include "cdecode.h"
		#include "alloc.h"
	}

	struct decoder
	{
        enum { BUFFERSIZE = 65536 };

		base64_decodestate _state;
		int _buffersize;
		// Optional customer allocator for the streaming scratch buffers
		// (B64_MEM_SHORT). Null => plain new[]/delete[].
		const b64_allocator* _alloc;

		decoder(int buffersize_in = BUFFERSIZE,
		        const b64_allocator* alloc_in = 0)
		: _buffersize(buffersize_in), _alloc(alloc_in)
		{
			base64_init_decodestate(&_state);
		}

		char* alloc_scratch(size_t n)
		{
			return (_alloc && _alloc->realloc_fn)
				? static_cast<char*>(
					_alloc->realloc_fn(_alloc->ctx, 0, n, B64_MEM_SHORT))
				: new char[n];
		}

		void free_scratch(char* p)
		{
			if (_alloc && _alloc->realloc_fn)
				_alloc->realloc_fn(_alloc->ctx, p, 0, B64_MEM_SHORT);
			else
				delete[] p;
		}

		int decode(char value_in)
		{
			return base64_decode_value(value_in);
		}

		std::streamsize decode(const char* code_in, const std::streamsize length_in, char* plaintext_out)
		{
			if (length_in <= 0)
				return 0;
			return static_cast<std::streamsize>(base64_decode_block(
				code_in, static_cast<size_t>(length_in),
				plaintext_out, &_state));
		}

		void decode(std::istream& istream_in, std::ostream& ostream_in)
		{
			base64_init_decodestate(&_state);
			//
			const int N = _buffersize;
			char* code = alloc_scratch(static_cast<size_t>(N));
			/* Decoded output is at most ~3/4 of the input; size it that way
			   instead of a full N. */
			char* plaintext = alloc_scratch(base64_decode_maxlength(
				static_cast<size_t>(N)));
			std::streamsize codelength;
			std::streamsize plainlength;

			do
			{
				istream_in.read((char*)code, N);
				codelength = istream_in.gcount();
				plainlength = decode(code, codelength, plaintext);
				ostream_in.write((const char*)plaintext, plainlength);
			}
			while (istream_in.good() && codelength > 0);
			//
			base64_init_decodestate(&_state);

			free_scratch(code);
			free_scratch(plaintext);
		}
	};

} // namespace base64



#endif // BASE64_DECODE_H

