/*
gendata.c - generate a deterministic data file for benchmarking libb64.

Usage: b64-gendata <size> <binary|text|zeros> <outfile|->

  size     byte count, optionally suffixed K/M/G (powers of 1024)
  type     binary  pseudo-random full byte range (xorshift64)
           text    repeating ASCII text
           zeros   all 0x00
  outfile  destination path, or "-" for stdout

Data is written in chunks so any size (e.g. 1G) streams without buffering
the whole file in memory. The binary stream matches the benchmark's own
generator so results are reproducible.
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK (1u << 16)

static int parse_size(const char* s, uint64_t* out)
{
	char* end;
	unsigned long long v;
	uint64_t mult = 1;

	/* Require a leading digit. strtoull would otherwise silently accept a
	   leading '-' and wrap it to a huge value; this also rejects '+',
	   whitespace and empty input. */
	if (s[0] < '0' || s[0] > '9')
		return -1;

	v = strtoull(s, &end, 10);
	if (*end != '\0')
	{
		switch (*end)
		{
		case 'k': case 'K': mult = 1024ull; break;
		case 'm': case 'M': mult = 1024ull * 1024; break;
		case 'g': case 'G': mult = 1024ull * 1024 * 1024; break;
		default: return -1;
		}
		if (end[1] != '\0')
			return -1;
	}

	if (v > UINT64_MAX / mult)  /* guard v * mult against overflow */
		return -1;

	*out = (uint64_t) v * mult;
	return 0;
}

int main(int argc, char** argv)
{
	uint64_t total, written = 0;
	const char* type;
	FILE* out;
	unsigned char buf[CHUNK];
	uint64_t x = 0x9e3779b97f4a7c15ull;
	static const char words[] = "the quick brown fox jumps over the lazy dog. ";
	const size_t wl = sizeof(words) - 1;
	size_t wi = 0;

	if (argc != 4)
	{
		fprintf(stderr,
			"usage: %s <size[K|M|G]> <binary|text|zeros> <outfile|->\n",
			argv[0]);
		return 2;
	}
	if (parse_size(argv[1], &total) != 0)
	{
		fprintf(stderr, "%s: invalid size '%s'\n", argv[0], argv[1]);
		return 2;
	}
	type = argv[2];
	if (strcmp(type, "binary") != 0 && strcmp(type, "text") != 0 &&
	    strcmp(type, "zeros") != 0)
	{
		fprintf(stderr, "%s: invalid type '%s'\n", argv[0], type);
		return 2;
	}

	out = (strcmp(argv[3], "-") == 0) ? stdout : fopen(argv[3], "wb");
	if (out == NULL)
	{
		perror(argv[3]);
		return 1;
	}

	while (written < total)
	{
		size_t want = CHUNK;
		size_t i;
		if (total - written < (uint64_t) want)
			want = (size_t) (total - written);

		if (strcmp(type, "zeros") == 0)
		{
			memset(buf, 0, want);
		}
		else if (strcmp(type, "text") == 0)
		{
			for (i = 0; i < want; ++i)
			{
				buf[i] = (unsigned char) words[wi];
				if (++wi == wl)
					wi = 0;
			}
		}
		else /* binary */
		{
			for (i = 0; i < want; ++i)
			{
				x ^= x << 13; x ^= x >> 7; x ^= x << 17;
				buf[i] = (unsigned char) (x & 0xff);
			}
		}

		if (fwrite(buf, 1, want, out) != want)
		{
			perror("fwrite");
			if (out != stdout)
				fclose(out);
			return 1;
		}
		written += want;
	}

	if (out != stdout && fclose(out) != 0)
	{
		perror("fclose");
		return 1;
	}
	return 0;
}
