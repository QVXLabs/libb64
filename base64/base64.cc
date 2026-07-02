/*
base64.cc - c++ source to a base64 reference encoder and decoder

This is part of the libb64 project, and has been placed in the public domain.
For details, see http://sourceforge.net/projects/libb64
*/

#include <b64/encode.h>
#include <b64/decode.h>

#include <iostream>
#include <fstream>
#include <string>

#include <stdlib.h>

// Function which prints the usage of this executable
void usage()
{
	std::cerr<< \
		"base64: Encodes and Decodes files using base64\n" \
		"Usage: base64 [-e|-d] [input] [output]\n" \
		"   Where [-e] will encode the input file into the output file,\n" \
		"         [-d] will decode the input file into the output file, and\n" \
		"         [input] and [output] are the input and output files, respectively.\n";
}
// Function which prints the usage of this executable, plus a short message
void usage(const std::string& message)
{
	usage();
	std::cerr<<"Incorrect invocation of base64:\n";
	std::cerr<<message<<std::endl;
}

int main(int argc, char** argv)
{
	// Quick check for valid arguments
	if (argc == 1)
	{
		usage();
		exit(-1);
	}
	if (argc != 4)
	{
		usage("Wrong number of arguments!");
		return 1;
	}

	// Validate the mode before opening (and truncating) the output file.
	std::string choice = argv[1];
	if (choice != "-e" && choice != "-d")
	{
		usage("Please specify -d or -e as first argument!");
		return 1;
	}

	std::string input = argv[2];
	std::string output = argv[3];
	// Refuse to clobber the input with itself (best-effort path compare).
	if (input == output)
	{
		usage("Input and output must be different files!");
		return 1;
	}

	// Open both streams in binary mode: when encoding the input may contain
	// zeros and other non-text bytes, and when decoding the output may.
	std::ifstream instream(input.c_str(),
		std::ios_base::in | std::ios_base::binary);
	if (!instream.is_open())
	{
		usage("Could not open input file!");
		return 1;
	}
	std::ofstream outstream(output.c_str(),
		std::ios_base::out | std::ios_base::binary);
	if (!outstream.is_open())
	{
		usage("Could not open output file!");
		return 1;
	}

	if (choice == "-d")
	{
		base64::decoder D = base64::decoder_builder().build();
		D.decode(instream, outstream);
	}
	else
	{
		base64::encoder E = base64::encoder_builder().build();
		E.encode(instream, outstream);
	}

	// Surface write failures (e.g. disk full) as a non-zero exit.
	outstream.flush();
	if (!outstream)
	{
		std::cerr << "base64: error writing output file!\n";
		return 1;
	}

	return 0;
}

