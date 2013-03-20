#include <iostream>
#include <vector>
using namespace std;

#include "Log.hpp"
#include "Clock.hpp"
#include "EndianNeutral.hpp"
using namespace cat;

#include "lodepng.h"
#include "optionparser.h"

class MonoConverter {
	vector<unsigned char> image;
	unsigned width, height;
	vector<unsigned short> symbols;

public:
	bool compress(const char *filename) {
		unsigned error = lodepng::decode(image, width, height, filename);

		if (error) {
			CAT_WARN("main") << "Decoder error " << error << ": " << lodepng_error_text(error);
			return false;
		}

		if ((width & 7) | (height & 7)) {
			CAT_WARN("main") << "Image dimensions must be an even multiple of 8x8";
			return false;
		}

		// Convert to monochrome image
		u32 bufferStride = (width + 31) >> 5;
		u32 bufferSize = bufferStride * height;
		u32 *buffer = new u32[bufferSize];
		u32 *writer = buffer;
		const unsigned char *reader = (const unsigned char*)&image[0] + 3;

		for (int ii = 0; ii < height; ++ii) {
			for (int jj = 0, jjlen = width >> 5; jj < jjlen; ++jj) {
				u32 bits = (reader[0] == 0);
				bits = (bits << 1) | (reader[4] == 0);
				bits = (bits << 1) | (reader[8] == 0);
				bits = (bits << 1) | (reader[12] == 0);
				bits = (bits << 1) | (reader[16] == 0);
				bits = (bits << 1) | (reader[20] == 0);
				bits = (bits << 1) | (reader[24] == 0);
				bits = (bits << 1) | (reader[28] == 0);
				bits = (bits << 1) | (reader[32] == 0);
				bits = (bits << 1) | (reader[36] == 0);
				bits = (bits << 1) | (reader[40] == 0);
				bits = (bits << 1) | (reader[44] == 0);
				bits = (bits << 1) | (reader[48] == 0);
				bits = (bits << 1) | (reader[52] == 0);
				bits = (bits << 1) | (reader[56] == 0);
				bits = (bits << 1) | (reader[60] == 0);
				bits = (bits << 1) | (reader[64] == 0);
				bits = (bits << 1) | (reader[68] == 0);
				bits = (bits << 1) | (reader[72] == 0);
				bits = (bits << 1) | (reader[76] == 0);
				bits = (bits << 1) | (reader[80] == 0);
				bits = (bits << 1) | (reader[84] == 0);
				bits = (bits << 1) | (reader[88] == 0);
				bits = (bits << 1) | (reader[92] == 0);
				bits = (bits << 1) | (reader[96] == 0);
				bits = (bits << 1) | (reader[100] == 0);
				bits = (bits << 1) | (reader[104] == 0);
				bits = (bits << 1) | (reader[108] == 0);
				bits = (bits << 1) | (reader[112] == 0);
				bits = (bits << 1) | (reader[116] == 0);
				bits = (bits << 1) | (reader[120] == 0);
				bits = (bits << 1) | (reader[124] == 0);

				*writer++ = bits;
				reader += 128;
			}

			u32 ctr = width & 31;
			if (ctr) {
				u32 bits = 0;
				while (ctr--) {
					bits = (bits << 1) | (reader[0] == 0);
					reader += 4;
				}
				*writer++ = bits;
			}
		}

		// Encode majority delta:

		{
			// Walk backwards from the end
			u32 *lagger = buffer + bufferSize - bufferStride;
			int hctr = height;
			while (--hctr) {
				u32 cb = 0;
				u32 cbabove = 0;

				for (int jj = 0; jj < bufferStride; ++jj) {
					u32 above = lagger[jj - (int)bufferStride];
					u32 now = lagger[jj];

					u32 above_shifted = (above << 1) | cbabove;
					u32 above_or_mask = above_shifted | above;
					u32 above_and_mask = above_shifted & above;
					u32 left = (now << 1) | cb;

					// left and at least one above, or both above
					u32 majority = (left & above_or_mask) | above_and_mask;

					// delta with prediction
					u32 mdelta = now ^ left;

					cb = now >> 31;
					cbabove = above >> 31;

					lagger[jj] = mdelta;
				}

				lagger -= bufferStride;
			}

			// First line
			u32 cb = 0;
			for (int jj = 0; jj < bufferStride; ++jj) {
				u32 oldv = lagger[jj];
				lagger[jj] = oldv ^ ((oldv << 1) | cb);
				cb = oldv >> 31;
			}
		}

		// Convert to image:

		vector<unsigned char> output;
		u8 bits = 0, bitCount = 0;

		for (int ii = 0; ii < height; ++ii) {
			for (int jj = 0; jj < width; ++jj) {
				u32 set = (buffer[ii * bufferStride + jj / 32] >> (31 - (jj & 31))) & 1;
				bits <<= 1;
				bits |= set;
				if (++bitCount >= 8) {
					output.push_back(bits);
					bits = 0;
					bitCount = 0;
				}
			}
		}

		lodepng_encode_file("output.png", (const unsigned char*)&output[0], width, height, LCT_GREY, 1);

		delete []buffer;

		return true;
	}
};

enum  optionIndex { UNKNOWN, HELP, VERBOSE, SILENT, COMPRESS, DECOMPRESS, TEST };
const option::Descriptor usage[] =
{
  {UNKNOWN, 0,"" , ""    ,option::Arg::None, "USAGE: gcif_mono [options] [output file path]\n\n"
                                             "Options:" },
  {HELP,    0,"h", "help",option::Arg::None, "  --[h]elp  \tPrint usage and exit." },
  {VERBOSE,0,"v" , "verbose",option::Arg::None, "  --[v]erbose \tVerbose console output" },
  {SILENT,0,"s" , "silent",option::Arg::None, "  --[s]ilent \tNo console output (even on errors)" },
  {COMPRESS,0,"c" , "compress",option::Arg::Optional, "  --[c]ompress <input PNG file path> \tCompress the given .PNG image." },
  {DECOMPRESS,0,"d" , "decompress",option::Arg::Optional, "  --[d]ecompress <input GCI file path> \tDecompress the given .GCI image" },
  {TEST,0,"t" , "test",option::Arg::Optional, "  --[t]est <input PNG file path> \tTest compression to verify it is lossless" },
  {UNKNOWN, 0,"" ,  ""   ,option::Arg::None, "\nExamples:\n"
                                             "  gcif_mono -tv ./original.png\n"
                                             "  gcif_mono -c ./original.png test.gci\n"
                                             "  gcif_mono -d ./test.gci decoded.png" },
  {0,0,0,0,0,0}
};

int processParameters(option::Parser &parse, option::Option options[]) {
	if (parse.error()) {
		CAT_FATAL("main") << "Error parsing arguments [retcode:1]";
		return 1;
	}

	if (options[SILENT]) {
		Log::ref()->SetThreshold(LVL_SILENT);
	}

	if (options[VERBOSE]) {
		Log::ref()->SetThreshold(LVL_INANE);
	}

	if (options[COMPRESS]) {

		const char *filePath = parse.nonOption(0);
		MonoConverter converter;
		if (!converter.compress(filePath)) {
			CAT_INFO("main") << "Error during conversion [retcode:2]";
			return 2;
		}

		return 0;

	} else if (options[DECOMPRESS]) {
		CAT_FATAL("main") << "TODO";
	} else if (options[TEST]) {
		CAT_FATAL("main") << "TODO";
	}

	option::printUsage(std::cout, usage);
	return 0;
}

int main(int argc, const char *argv[]) {

	Clock::ref()->OnInitialize();

	if (argc > 0) {
		--argc;
		++argv;
	}

	option::Stats  stats(usage, argc, argv);
	option::Option *options = new option::Option[stats.options_max];
	option::Option *buffer = new option::Option[stats.buffer_max];
	option::Parser parse(usage, argc, argv, options, buffer);

	int retval = processParameters(parse, options);

	delete []options;
	delete []buffer;

	Clock::ref()->OnFinalize();

	return retval;
}

