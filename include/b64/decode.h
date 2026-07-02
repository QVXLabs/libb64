// :mode=c++:
/*
decode.h - c++ wrapper for a base64 decoding algorithm

This is part of the libb64 project, and has been placed in the public domain.
For details, see http://sourceforge.net/projects/libb64
*/
#ifndef BASE64_DECODE_H
#define BASE64_DECODE_H

#include <iostream>
#include <new>

#include "alloc.h"

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
		// Scratch-buffer allocator (B64_MEM_SHORT), resolved at construction so
		// the streaming path needs no per-call check. Defaults to new[]/delete[].
		b64_allocator _alloc;

	private:
		// Tag so the builder can reach a non-deprecated constructor.
		struct builder_tag {};
		decoder(int buffersize_in, const b64_allocator& alloc_in, builder_tag)
		// clamp: a non-positive size would turn into a huge size_t
		: _buffersize(buffersize_in < 1 ? 1 : buffersize_in), _alloc(alloc_in)
		{
			base64_init_decodestate(&_state);
		}
		friend class decoder_builder;

	public:
		// Deprecated: construct via base64::decoder_builder().build().
		[[deprecated("use base64::decoder_builder().build()")]]
		decoder(int buffersize_in = BUFFERSIZE,
		        const b64_allocator* alloc_in = 0)
		: decoder(buffersize_in,
		          (alloc_in && alloc_in->realloc_fn)
		              ? *alloc_in : default_cpp_allocator(),
		          builder_tag())
		{
		}

		char* alloc_scratch(size_t n)
		{
			return static_cast<char*>(
				_alloc.realloc_fn(_alloc.ctx, 0, n, B64_MEM_SHORT));
		}

		void free_scratch(char* p)
		{
			_alloc.realloc_fn(_alloc.ctx, p, 0, B64_MEM_SHORT);
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
			if (!code)
				throw std::bad_alloc();
			/* Decoded output is at most ~3/4 of the input; size it that way
			   instead of a full N. */
			char* plaintext = alloc_scratch(base64_decode_maxlength(
				static_cast<size_t>(N)));
			if (!plaintext)
			{
				free_scratch(code);
				throw std::bad_alloc();
			}
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

	// Builder for decoder -- the supported way to configure one.
	class decoder_builder
	{
		int _buffersize;
		b64_allocator _alloc;

	public:
		decoder_builder()
			: _buffersize(decoder::BUFFERSIZE), _alloc(default_cpp_allocator())
		{
		}

		decoder_builder& buffer_size(int n) { _buffersize = n; return *this; }

		// Scratch allocator: a realloc-style callback plus its context. A null
		// callback restores the new[]/delete[] default.
		decoder_builder& realloc(b64_realloc_fn fn, void* ctx = 0)
		{
			_alloc = base64_allocator(fn ? fn : b64_new_delete_realloc, ctx);
			return *this;
		}

		decoder build() const
		{
			return decoder(_buffersize, _alloc, decoder::builder_tag());
		}
	};

} // namespace base64



#endif // BASE64_DECODE_H

