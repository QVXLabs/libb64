// :mode=c++:
/*
encode.h - c++ wrapper for a base64 encoding algorithm

This is part of the libb64 project, and has been placed in the public domain.
For details, see http://sourceforge.net/projects/libb64
*/
#ifndef BASE64_ENCODE_H
#define BASE64_ENCODE_H

#include <iostream>

namespace base64
{
	extern "C"
	{
#include "cencode.h"
#include "alloc.h"
	}

	struct encoder
	{
		enum { BUFFERSIZE = 65536 };

		base64_encodestate _state;
		int _buffersize;
		// Optional customer allocator for the streaming scratch buffers
		// (B64_MEM_SHORT). Null => plain new[]/delete[].
		const b64_allocator* _alloc;

		encoder(int buffersize_in = BUFFERSIZE,
		        const b64_allocator* alloc_in = 0)
			: _buffersize(buffersize_in), _alloc(alloc_in)
		{
			base64_init_encodestate(&_state);
		}

		char* alloc_scratch(size_t n)
		{
			if (_alloc && _alloc->realloc_fn)
				return static_cast<char*>(
					_alloc->realloc_fn(_alloc->ctx, 0, n, B64_MEM_SHORT));
			return new char[n];
		}

		void free_scratch(char* p)
		{
			if (_alloc && _alloc->realloc_fn)
				_alloc->realloc_fn(_alloc->ctx, p, 0, B64_MEM_SHORT);
			else
				delete[] p;
		}

		int encode(char value_in)
		{
			return base64_encode_value(value_in);
		}

		std::streamsize encode(const char* code_in, const std::streamsize length_in, char* plaintext_out)
		{
			if (length_in <= 0)
				return 0;
			return static_cast<std::streamsize>(base64_encode_block(
				code_in, static_cast<size_t>(length_in),
				plaintext_out, &_state));
		}

		std::streamsize encode_end(char* plaintext_out)
		{
			return static_cast<std::streamsize>(
				base64_encode_blockend(plaintext_out, &_state));
		}

		void encode(std::istream& istream_in, std::ostream& ostream_in)
		{
			const int N = _buffersize;
			char* plaintext = alloc_scratch(static_cast<size_t>(N));
			/* Size the output to what N input bytes actually encode to at the
			   current line width (base64_encode_length accounts for wrapping
			   newlines); the small constant covers the mid-stream carry. The
			   old 2*N was both wasteful when unwrapped and too small for very
			   narrow line widths. */
			char* code = alloc_scratch(
				base64_encode_length(static_cast<size_t>(N), &_state) + 16);
			std::streamsize plainlength;
			std::streamsize codelength;

			do
			{
				istream_in.read(plaintext, N);
				plainlength = istream_in.gcount();
				//
				codelength = encode(plaintext, plainlength, code);
				ostream_in.write(code, codelength);
			} while(istream_in.good() && plainlength > 0);

			codelength = encode_end(code);
			ostream_in.write(code, codelength);
			//
			base64_init_encodestate(&_state);

			free_scratch(code);
			free_scratch(plaintext);
		}
	};

} // namespace base64

#endif // BASE64_ENCODE_H

