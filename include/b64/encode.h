// :mode=c++:
/*
encode.h - c++ wrapper for a base64 encoding algorithm

This is part of the libb64 project, and has been placed in the public domain.
For details, see http://sourceforge.net/projects/libb64
*/
#ifndef BASE64_ENCODE_H
#define BASE64_ENCODE_H

#include <iostream>

#include "alloc.h"

namespace base64
{
	extern "C"
	{
#include "cencode.h"
	}

	struct encoder
	{
		enum { BUFFERSIZE = 65536 };

		base64_encodestate _state;
		int _buffersize;
		// Scratch-buffer allocator (B64_MEM_SHORT), resolved at construction so
		// the streaming path needs no per-call check. Defaults to new[]/delete[].
		b64_allocator _alloc;

	private:
		// Tag so the builder can reach a non-deprecated constructor.
		struct builder_tag {};
		encoder(int buffersize_in, const b64_allocator& alloc_in, builder_tag)
			: _buffersize(buffersize_in), _alloc(alloc_in)
		{
			base64_init_encodestate(&_state);
		}
		friend class encoder_builder;

	public:
		// Deprecated: construct via base64::encoder_builder().build().
		[[deprecated("use base64::encoder_builder().build()")]]
		encoder(int buffersize_in = BUFFERSIZE,
		        const b64_allocator* alloc_in = 0)
			: encoder(buffersize_in,
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

	// Builder for encoder -- the supported way to configure one.
	class encoder_builder
	{
		int _buffersize;
		size_t _chars_per_line;
		b64_allocator _alloc;

	public:
		encoder_builder()
			: _buffersize(encoder::BUFFERSIZE), _chars_per_line(0),
			  _alloc(default_cpp_allocator())
		{
		}

		encoder_builder& buffer_size(int n) { _buffersize = n; return *this; }

		encoder_builder& chars_per_line(size_t n)
		{
			_chars_per_line = n;
			return *this;
		}

		// Scratch allocator: a realloc-style callback plus its context. A null
		// callback restores the new[]/delete[] default.
		encoder_builder& realloc(b64_realloc_fn fn, void* ctx = 0)
		{
			_alloc = base64_allocator(fn ? fn : b64_new_delete_realloc, ctx);
			return *this;
		}

		encoder build() const
		{
			encoder e(_buffersize, _alloc, encoder::builder_tag());
			e._state.chars_per_line = _chars_per_line;
			return e;
		}
	};

} // namespace base64

#endif // BASE64_ENCODE_H

