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
	}

	struct decoder
	{
        enum { BUFFERSIZE = 65536 };

		base64_decodestate _state;
		int _buffersize;

		decoder(int buffersize_in = BUFFERSIZE)
		: _buffersize(buffersize_in)
		{
			base64_init_decodestate(&_state);
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
			char* code = new char[N];
			/* Decoded output is at most ~3/4 of the input; size it that way
			   instead of a full N. */
			char* plaintext = new char[base64_decode_maxlength(
				static_cast<size_t>(N))];
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

			delete [] code;
			delete [] plaintext;
		}
	};

} // namespace base64



#endif // BASE64_DECODE_H

