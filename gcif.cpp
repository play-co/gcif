#include <iostream>
#include <vector>
using namespace std;

#include "Log.hpp"
#include "Clock.hpp"
#include "ImageMaskWriter.hpp"
#include "ImageMaskReader.hpp"
using namespace cat;

#include "lodepng.h"
#include "optionparser.h"


class Converter {
public:
	bool compress(const char *filename, const char *outfile) {
		vector<unsigned char> image;
		unsigned width, height;

		unsigned error = lodepng::decode(image, width, height, filename);

		if (error) {
			CAT_WARN("main") << "Decoder error " << error << ": " << lodepng_error_text(error);
			return false;
		}

		if ((width & 7) | (height & 7)) {
			CAT_WARN("main") << "Image dimensions must be an even multiple of 8x8";
			return false;
		}

		// Generate ImageMask
		ImageMaskWriter imageMaskWriter;
		if (!imageMaskWriter.initFromRGBA(&image[0], width, height)) {
			CAT_WARN("main") << "Unable to generate image mask";
			return false;
		}

		ImageWriter writer;
		if (writer.init(width, height) != WE_OK) {
			CAT_WARN("main") << "Unable to initialize image writer";
			return false;
		}

		imageMaskWriter.write(writer);
		if (writer.finalizeAndWrite(outfile) != WE_OK) {
			CAT_WARN("main") << "Unable to finalize and write image mask";
			return false;
		}

		CAT_WARN("main") << "Write success!";
		return true;
	}

	bool decompress(const char *filename, const char *outfile) {
		ImageReader reader;

		int err;

		if ((err = reader.init(filename))) {
			CAT_WARN("main") << "Unable to read file: " << ImageReader::ErrorString(err);
			return false;
		}

		ImageMaskReader maskReader;
		if ((err = maskReader.read(reader))) {
			CAT_WARN("main") << "Unable to read mask: " << ImageReader::ErrorString(err);
			return false;
		}

		if (!reader.finalizeCheckHash()) {
			CAT_WARN("main") << "Hash mismatch";
			return false;
		}

/*
		CAT_WARN("main") << "Writing output image file: " << outfile;
		// Convert to image:

		vector<unsigned char> output;
		u8 bits = 0, bitCount = 0;

		for (int ii = 0; ii < _height; ++ii) {
			for (int jj = 0; jj < _width; ++jj) {
				u32 set = (_image[ii * _stride + jj / 32] >> (31 - (jj & 31))) & 1;
				bits <<= 1;
				bits |= set;
				if (++bitCount >= 8) {
					output.push_back(bits);
					bits = 0;
					bitCount = 0;
				}
			}
		}

		lodepng_encode_file(outfile, (const unsigned char*)&output[0], _width, _height, LCT_GREY, 1);
*/

		CAT_WARN("main") << "Read success!";
		return true;
	}
};


//// Command-line parameter parsing

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
		if (parse.nonOptionsCount() != 2) {
			CAT_WARN("main") << "Input error: Please provide input and output file paths";
		} else {
			const char *inFilePath = parse.nonOption(0);
			const char *outFilePath = parse.nonOption(1);
			Converter converter;

			if (!converter.compress(inFilePath, outFilePath)) {
				CAT_INFO("main") << "Error during conversion [retcode:2]";
				return 2;
			}

			return 0;
		}
	} else if (options[DECOMPRESS]) {
		if (parse.nonOptionsCount() != 2) {
			CAT_WARN("main") << "Input error: Please provide input and output file paths";
		} else {
			const char *inFilePath = parse.nonOption(0);
			const char *outFilePath = parse.nonOption(1);
			Converter converter;

			if (!converter.decompress(inFilePath, outFilePath)) {
				CAT_INFO("main") << "Error during conversion [retcode:3]";
				return 3;
			}

			return 0;
		}
	} else if (options[TEST]) {
		CAT_FATAL("main") << "TODO";
	}

	option::printUsage(std::cout, usage);
	return 0;
}


//// Entrypoint

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

