#include <iostream>
#include <vector>
using namespace std;

#include "Log.hpp"
#include "Clock.hpp"

#include "GCIFReader.hpp"
#include "GCIFWriter.hpp"

using namespace cat;

#include "lodepng.h"
#include "optionparser.h"


//// Commands

static int compress(const char *filename, const char *outfile, int compress_level) {
	vector<unsigned char> image;
	unsigned width, height;

	CAT_WARN("main") << "Reading input PNG image file: " << filename;

	unsigned error = lodepng::decode(image, width, height, filename);

	if (error) {
		CAT_WARN("main") << "PNG read error " << error << ": " << lodepng_error_text(error);
		return error;
	}

	CAT_WARN("main") << "Encoding image: " << outfile;

	int err;

	if ((err = gcif_write(&image[0], width, height, outfile, compress_level))) {
		CAT_WARN("main") << "Error while compressing the image: " << gcif_write_errstr(err);
		return err;
	}

	CAT_WARN("main") << "Success.";
	return 0;
}


static int decompress(const char *filename, const char *outfile) {
	CAT_WARN("main") << "Decoding input GCIF image file: " << filename;

	int err;

	GCIFImage image;
	if ((err = gcif_read(filename, &image))) {
		CAT_WARN("main") << "Error while decompressing the image: " << gcif_read_errstr(err);
		return err;
	}

	CAT_WARN("main") << "Writing output PNG image file: " << outfile;

	lodepng_encode_file(outfile, (const unsigned char*)image.rgba, image.width, image.height, LCT_RGBA, 8);

	delete []image.rgba;

	CAT_WARN("main") << "Success.";
	return 0;
}

#include "ImageWriter.hpp"
#include "ImageMaskWriter.hpp"
#include "ImageLZWriter.hpp"
#include "ImageCMWriter.hpp"

struct BenchStats {
	int x;
};

#include <sys/stat.h>

static int benchfile(BenchStats &stats, string filename) {
	vector<unsigned char> image;
	unsigned width, height;

	const int compress_level = 9999;

	double t0 = Clock::ref()->usec();

	unsigned error = lodepng::decode(image, width, height, filename);

	double t1 = Clock::ref()->usec();

	if (error) {
		CAT_WARN("main") << "PNG read error " << error << ": " << lodepng_error_text(error) << " for " << filename;
		return 0;
		//return error;
	}

	int err;

	static const char *BENCHFILE = "benchmark_temp.gci";

	if ((err = gcif_write(&image[0], width, height, BENCHFILE, compress_level))) {
		CAT_WARN("main") << "Error while compressing the image: " << gcif_write_errstr(err) << " for " << filename;
		return err;
	}

	double t2 = Clock::ref()->usec();

	GCIFImage outimage;
	if ((err = gcif_read(BENCHFILE, &outimage))) {
		CAT_WARN("main") << "Error while decompressing the image: " << gcif_read_errstr(err) << " for " << filename;
		return err;
	}

	double t3 = Clock::ref()->usec();

	for (u32 ii = 0; ii < width * height * 4; ++ii) {
		if (outimage.rgba[ii] != image[ii]) {
			CAT_WARN("main") << "Output image does not match input image for " << filename;
			break;
		}
	}

	struct stat png, gci;
	stat(filename.c_str(), &png);
	stat(BENCHFILE, &gci);

	u32 pngBytes = png.st_size;
	u32 gciBytes = gci.st_size;
	double gcipngrat = pngBytes / (double)gciBytes;
	double gciover = width * height * 4 / (double)gciBytes;

	double pngtime = t1 - t0;
	double gcitime = t3 - t2;
	double gcipngtime = gcitime / pngtime;

	CAT_WARN("main") << filename << " => " << gcipngrat << "x smaller than PNG and decompresses " << gcipngtime << "x faster";

	return 0;
}

#ifdef CAT_COMPILER_MSVC
#include "msvc/dirent.h"
#else
#include <dirent.h>
#endif

static int benchmark(const char *path) {
	DIR *dir;
	struct dirent *ent;

	if ((dir = opendir (path)) == NULL) {
		return -1;
	}

	BenchStats stats;

	/* print all the files and directories within directory */
	while ((ent = readdir (dir)) != NULL) {
		if (ent->d_name[0] != '.') {
			int err;

			if ((err = benchfile(stats, string(path) + "/" + ent->d_name))) {
				return err;
			}
		}
	}

	closedir (dir);

	return 0;
}


static int profileit(const char *filename) {
	CAT_WARN("main") << "Decoding input GCIF image file hard: " << filename;

	int err;

	for (int ii = 0; ii < 20; ++ii) {
		GCIFImage image;
		if ((err = gcif_read(filename, &image))) {
			CAT_WARN("main") << "Error while decompressing the image: " << gcif_read_errstr(err);
			return err;
		}
	}

	CAT_WARN("main") << "Success.";
	return 0;
}


//// Command-line parameter parsing

enum  optionIndex { UNKNOWN, HELP, L0, L1, L2, VERBOSE, SILENT, COMPRESS, DECOMPRESS, TEST, BENCHMARK, PROFILE };
const option::Descriptor usage[] =
{
  {UNKNOWN, 0,"" , ""    ,option::Arg::None, "USAGE: ./gcif [options] [output file path]\n\n"
                                             "Options:" },
  {HELP,    0,"h", "help",option::Arg::None, "  --[h]elp  \tPrint usage and exit." },
  {VERBOSE,0,"v" , "verbose",option::Arg::None, "  --[v]erbose \tVerbose console output" },
  {L0,0,"0" , "0",option::Arg::None, "  -0 \tCompression level 0 : Faster" },
  {L1,0,"1" , "1",option::Arg::None, "  -1 \tCompression level 1 : Better" },
  {L2,0,"2" , "2",option::Arg::None, "  -2 \tCompression level 2 : Harder (default)" },
  {SILENT,0,"s" , "silent",option::Arg::None, "  --[s]ilent \tNo console output (even on errors)" },
  {COMPRESS,0,"c" , "compress",option::Arg::Optional, "  --[c]ompress <input PNG file path> \tCompress the given .PNG image." },
  {DECOMPRESS,0,"d" , "decompress",option::Arg::Optional, "  --[d]ecompress <input GCI file path> \tDecompress the given .GCI image" },
  {TEST,0,"t" , "test",option::Arg::Optional, "  --[t]est <input PNG file path> \tTest compression to verify it is lossless" },
  {BENCHMARK,0,"b" , "benchmark",option::Arg::Optional, "  --[b]enchmark <test set path> \tTest compression ratio and decompression speed for a whole directory at once" },
  {PROFILE,0,"p" , "profile",option::Arg::Optional, "  --[p]rofile <input GCI file path> \tDecode same GCI file 100x to enhance profiling of decoder" },
  {UNKNOWN, 0,"" ,  ""   ,option::Arg::None, "\nExamples:\n"
                                             "  ./gcif -tv ./original.png\n"
                                             "  ./gcif -c ./original.png test.gci\n"
                                             "  ./gcif -d ./test.gci decoded.png" },
  {0,0,0,0,0,0}
};

int processParameters(option::Parser &parse, option::Option options[]) {
	if (parse.error()) {
		CAT_FATAL("main") << "Error parsing arguments [retcode:1]";
		return 1;
	}

	Log::ref()->SetThreshold(LVL_INFO);

	if (options[SILENT]) {
		Log::ref()->SetThreshold(LVL_SILENT);
	}

	if (options[VERBOSE]) {
		Log::ref()->SetThreshold(LVL_INANE);
	}

	int compression_level = 2;
	if (options[L0]) {
		compression_level = 0;
	} else if (options[L1]) {
		compression_level = 1;
	} else if (options[L2]) {
		compression_level = 2;
	}

	if (options[COMPRESS]) {
		if (parse.nonOptionsCount() != 2) {
			CAT_WARN("main") << "Input error: Please provide input and output file paths";
		} else {
			const char *inFilePath = parse.nonOption(0);
			const char *outFilePath = parse.nonOption(1);
			int err;

			if ((err = compress(inFilePath, outFilePath, compression_level))) {
				CAT_INFO("main") << "Error during conversion [retcode:" << err << "]";
				return err;
			}

			return 0;
		}
	} else if (options[DECOMPRESS]) {
		if (parse.nonOptionsCount() != 2) {
			CAT_WARN("main") << "Input error: Please provide input and output file paths";
		} else {
			const char *inFilePath = parse.nonOption(0);
			const char *outFilePath = parse.nonOption(1);
			int err;

			if ((err = decompress(inFilePath, outFilePath))) {
				CAT_INFO("main") << "Error during conversion [retcode:" << err << "]";
				return err;
			}

			return 0;
		}
	} else if (options[TEST]) {
		CAT_FATAL("main") << "TODO";
	} else if (options[BENCHMARK]) {
		if (parse.nonOptionsCount() != 1) {
			CAT_WARN("main") << "Input error: Please provide input directory path";
		} else {
			const char *inFilePath = parse.nonOption(0);
			int err;

			if ((err = benchmark(inFilePath))) {
				CAT_INFO("main") << "Error during conversion [retcode:" << err << "]";
				return err;
			}

			return 0;
		}
	} else if (options[PROFILE]) {
		if (parse.nonOptionsCount() != 1) {
			CAT_WARN("main") << "Input error: Please provide input file path";
		} else {
			const char *inFilePath = parse.nonOption(0);
			int err;

			if ((err = profileit(inFilePath))) {
				CAT_INFO("main") << "Error during conversion [retcode:" << err << "]";
				return err;
			}

			return 0;
		}
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

