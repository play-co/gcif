/*
	Copyright (c) 2013 Game Closure.  All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	* Redistributions of source code must retain the above copyright notice,
	  this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	* Neither the name of GCIF nor the names of its contributors may be used
	  to endorse or promote products derived from this software without
	  specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#include <iostream>
#include <vector>
using namespace std;

#include "encoder/Log.hpp"
#include "encoder/Clock.hpp"
#include "decoder/Enforcer.hpp"

#include "decoder/GCIFReader.h"
#include "encoder/GCIFWriter.h"
using namespace cat;

#include "encoder/lodepng.h"
#include "optionparser.h"


//// Commands

static int compress(const char *filename, const char *outfile, int compress_level) {
	vector<unsigned char> image;
	unsigned size_x, size_y;

	CAT_WARN("main") << "Reading input PNG image file: " << filename;

	unsigned error = lodepng::decode(image, size_x, size_y, filename);

	if (error) {
		CAT_WARN("main") << "PNG read error " << error << ": " << lodepng_error_text(error);
		return error;
	}

	CAT_WARN("main") << "Encoding image: " << outfile;

	int err;

	if ((err = gcif_write(&image[0], size_x, size_y, outfile, compress_level))) {
		CAT_WARN("main") << "Error while compressing the image: " << gcif_write_errstr(err);
		return err;
	}

	return GCIF_WE_OK;
}


static int decompress(const char *filename, const char *outfile) {
	CAT_WARN("main") << "Decoding input GCIF image file: " << filename;

	int err;

	GCIFImage image;
	if ((err = gcif_read_file(filename, &image))) {
		CAT_WARN("main") << "Error while decompressing the image: " << gcif_read_errstr(err);
		return err;
	}

	CAT_WARN("main") << "Writing output PNG image file: " << outfile;

	lodepng_encode_file(outfile, (const unsigned char*)image.rgba, image.size_x, image.size_y, LCT_RGBA, 8);

	free(image.rgba);

	return GCIF_RE_OK;
}

#include <sys/stat.h>

static int benchfile(string filename) {
	vector<unsigned char> image;
	unsigned size_x = 0, size_y = 0;

	const int compress_level = 9999;

	double t0 = Clock::ref()->usec();

	unsigned error = lodepng::decode(image, size_x, size_y, filename);

	double t1 = Clock::ref()->usec();

	if (error) {
		CAT_WARN("main") << "PNG read error " << error << ": " << lodepng_error_text(error) << " for " << filename;
		return 0;
		//return error;
	}

	int err;

	string benchfile = filename + ".gci";
	const char *cbenchfile = benchfile.c_str();

	if ((err = gcif_write(&image[0], size_x, size_y, cbenchfile, compress_level))) {
		CAT_WARN("main") << "Error while compressing the image: " << gcif_write_errstr(err) << " for " << filename;
		return err;
	}

	double t2 = Clock::ref()->usec();

	GCIFImage outimage;
	if ((err = gcif_read_file(cbenchfile, &outimage))) {
		CAT_WARN("main") << "Error while decompressing the image: " << gcif_read_errstr(err) << " for " << filename;
		return err;
	}

	double t3 = Clock::ref()->usec();

	for (u32 ii = 0; ii < size_x * size_y * 4; ii += 4) {
		if (image[ii + 3] == 0) {
			if (*(u32*)&outimage.rgba[ii] != 0) {
				CAT_WARN("main") << "Output image does not match input image for " << filename << " at " << ii << " (on transparency)";
				break;
			}
		} else {
			if (*(u32*)&outimage.rgba[ii] != *(u32*)&image[ii]) {
				CAT_WARN("main") << "Output image does not match input image for " << filename << " at " << ii;
				break;
			}
		}
	}

	struct stat png, gci;
	stat(filename.c_str(), &png);
	stat(cbenchfile, &gci);

	u32 pngBytes = png.st_size;
	u32 gciBytes = gci.st_size;
	double gcipngrat = pngBytes / (double)gciBytes;

	double pngtime = t1 - t0;
	double gcitime = t3 - t2;
	double gcipngtime = pngtime / gcitime;

	CAT_WARN("main") << filename << " => " << gcipngrat << "x smaller than PNG and decompresses " << gcipngtime << "x faster";

//	free(outimage.rgba);

	return GCIF_RE_OK;
}

#include "encoder/Thread.hpp"
#include "encoder/WaitableFlag.hpp"
#include "encoder/Mutex.hpp"
#include "encoder/SystemInfo.hpp"

class BenchThread : public Thread {
	vector<string> *_files;
	Mutex *_lock;
	WaitableFlag _ready;
	volatile bool _abort, _shutdown;

public:
	void init(vector<string> *files, Mutex *lock) {
		_files = files;
		_lock = lock;
		_shutdown = false;
		_abort = false;
	}

	virtual bool Entrypoint(void *param) {
		while (!_abort) {
			if (_ready.Wait()) {
				string filename;
				bool hasFile;

				while (!_abort) {
					hasFile = false;
					{
						AutoMutex alock(*_lock);

						if (_files->size() > 0) {
							hasFile = true;
							filename = _files->back();
							_files->pop_back();
						}
					}

					if (hasFile) {
						CAT_WARN("TEST") << filename;
						benchfile(filename);
					} else {
						break;
					}
				}

				if (_shutdown) {
					break;
				}
			}
		}

		return true;
	}

	void wake() {
		_ready.Set();
	}

	void abort() {
		_abort = true;
	}

	void shutdown() {
		_shutdown = true;
	}
};

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

	const int MAX_CPU_COUNT = 64;
	int cpu_count = SystemInfo::ref()->GetProcessorCount();
	if (cpu_count > MAX_CPU_COUNT) {
		cpu_count = MAX_CPU_COUNT;
	}
	cpu_count--;
	if (cpu_count < 1) {
		cpu_count = 1;
	}

	BenchThread threads[MAX_CPU_COUNT];
	vector<string> files;
	Mutex lock;

	for (int ii = 0; ii < cpu_count; ++ii) {
		threads[ii].init(&files, &lock);
		CAT_ENFORCE(threads[ii].StartThread());
	}

	while ((ent = readdir (dir)) != NULL) {
		const char *name = ent->d_name;
		int namelen = (int)strlen(name);

		if (namelen > 4 &&
			tolower(name[namelen-3]) == 'p' &&
			tolower(name[namelen-2]) == 'n' &&
			tolower(name[namelen-1]) == 'g') {
			string filename = string(path) + "/" + name;

			{
				AutoMutex alock(lock);

				files.push_back(filename);
			}

			for (int ii = 0; ii < cpu_count; ++ii) {
				threads[ii].wake();
			}
		}
	}

	closedir(dir);

	for (int ii = 0; ii < cpu_count; ++ii) {
		threads[ii].shutdown();
		threads[ii].wake();
	}

	for (int ii = 0; ii < cpu_count; ++ii) {
		CAT_ENFORCE(threads[ii].WaitForThread());
	}

	return 0;
}




static int replacefile(string filename) {
	vector<unsigned char> image;
	unsigned size_x, size_y;

	const int compress_level = 9999;

	double t0 = Clock::ref()->usec();

	unsigned error = lodepng::decode(image, size_x, size_y, filename);

	double t1 = Clock::ref()->usec();

	if (error) {
		CAT_WARN("main") << "PNG read error " << error << ": " << lodepng_error_text(error) << " for " << filename;
		return 0;
		//return error;
	}

	int err;

	string benchfile = filename + ".gci";
	const char *cbenchfile = benchfile.c_str();

	if ((err = gcif_write(&image[0], size_x, size_y, cbenchfile, compress_level))) {
		CAT_WARN("main") << "Error while compressing the image: " << gcif_write_errstr(err) << " for " << filename;
		return err;
	}

	double t2 = Clock::ref()->usec();

	GCIFImage outimage;
	if ((err = gcif_read_file(cbenchfile, &outimage))) {
		CAT_WARN("main") << "Error while decompressing the image: " << gcif_read_errstr(err) << " for " << filename;
		return err;
	}

	double t3 = Clock::ref()->usec();

	for (u32 ii = 0; ii < size_x * size_y * 4; ++ii) {
		if (outimage.rgba[ii] != image[ii]) {
			CAT_WARN("main") << "Error: Output GCIF image does not match input image for " << filename;
			break;
		}
	}

	struct stat png, gci;
	const char *cfilename = filename.c_str();
	stat(cfilename, &png);
	stat(cbenchfile, &gci);

	u32 pngBytes = png.st_size;
	u32 gciBytes = gci.st_size;
	double gcipngrat = pngBytes / (double)gciBytes;

	double pngtime = t1 - t0;
	double gcitime = t3 - t2;
	double gcipngtime = pngtime / gcitime;

	if (gciBytes <= pngBytes) {
		CAT_WARN("main") << filename << " => Replaced with GCIF : " << gcipngrat << "x smaller than PNG and decompresses " << gcipngtime << "x faster";

		unlink(cfilename);
		rename(cbenchfile, cfilename);
	} else {
		CAT_WARN("main") << filename << " => Keeping PNG for this file";
		unlink(cbenchfile);
	}

	free(outimage.rgba);

	return GCIF_RE_OK;
}



class ReplaceThread : public Thread {
	vector<string> *_files;
	Mutex *_lock;
	volatile bool _shutdown;

public:
	void init(vector<string> *files, Mutex *lock) {
		_files = files;
		_lock = lock;
		_shutdown = false;
	}

	virtual bool Entrypoint(void *param) {
		do {
			string filename;
			bool hasFile;

			do {
				hasFile = false;
				{
					AutoMutex alock(*_lock);

					if (_files->size() > 0) {
						hasFile = true;
						filename = _files->back();
						_files->pop_back();
					}
				}

				if (hasFile) {
					replacefile(filename);
				} else {
					break;
				}
			} while (hasFile);
		} while (!_shutdown);

		return true;
	}

	void shutdown() {
		_shutdown = true;
	}
};





static int replace(const char *path) {
	DIR *dir;
	struct dirent *ent;

	if ((dir = opendir (path)) == NULL) {
		return -1;
	}

	const int MAX_CPU_COUNT = 64;
	int cpu_count = SystemInfo::ref()->GetProcessorCount();
	if (cpu_count > MAX_CPU_COUNT) {
		cpu_count = MAX_CPU_COUNT;
	}
	cpu_count--;
	if (cpu_count < 1) {
		cpu_count = 1;
	}

	ReplaceThread threads[MAX_CPU_COUNT];
	vector<string> files;
	Mutex lock;

	while ((ent = readdir (dir)) != NULL) {
		const char *name = ent->d_name;
		int namelen = (int)strlen(name);

		if (namelen > 4 &&
			tolower(name[namelen-3]) == 'p' &&
			tolower(name[namelen-2]) == 'n' &&
			tolower(name[namelen-1]) == 'g') {
			string filename = string(path) + "/" + name;

			{
				AutoMutex alock(lock);

				files.push_back(filename);
			}
		}
	}

	closedir(dir);

	for (int ii = 0; ii < cpu_count; ++ii) {
		threads[ii].init(&files, &lock);
		CAT_ENFORCE(threads[ii].StartThread());
		threads[ii].shutdown();
	}

	for (int ii = 0; ii < cpu_count; ++ii) {
		CAT_ENFORCE(threads[ii].WaitForThread());
	}

	return 0;
}










static int profileit(const char *filename) {
	CAT_WARN("main") << "Decoding input GCIF image file hard: " << filename;

	int err;

	for (int ii = 0; ii < 100; ++ii) {
		GCIFImage image;
		if ((err = gcif_read_file(filename, &image))) {
			CAT_WARN("main") << "Error while decompressing the image: " << gcif_read_errstr(err);
			return err;
		}

		free(image.rgba);
	}

	return GCIF_RE_OK;
}


//// Command-line parameter parsing

enum  optionIndex { UNKNOWN, HELP, L0, L1, L2, L3, VERBOSE, SILENT, COMPRESS, DECOMPRESS, /*TEST,*/ BENCHMARK, PROFILE, REPLACE };
const option::Descriptor usage[] =
{
  {UNKNOWN, 0,"" , ""    ,option::Arg::None, "USAGE: ./gcif [options] [output file path]\n\n"
                                             "Options:" },
  {HELP,    0,"h", "help",option::Arg::None, "  --[h]elp  \tPrint usage and exit." },
  {VERBOSE,0,"v" , "verbose",option::Arg::None, "  --[v]erbose \tVerbose console output" },
  {L0,0,"0" , "0",option::Arg::None, "  -0 \tCompression level 0 : Faster" },
  {L1,0,"1" , "1",option::Arg::None, "  -1 \tCompression level 1 : Better" },
  {L2,0,"2" , "2",option::Arg::None, "  -2 \tCompression level 2 : Harder" },
  {L3,0,"3" , "3",option::Arg::None, "  -3 \tCompression level 3 : Stronger (default)" },
  {SILENT,0,"s" , "silent",option::Arg::None, "  --[s]ilent \tNo console output (even on errors)" },
  {COMPRESS,0,"c" , "compress",option::Arg::Optional, "  --[c]ompress <input PNG file path> \tCompress the given .PNG image." },
  {DECOMPRESS,0,"d" , "decompress",option::Arg::Optional, "  --[d]ecompress <input GCI file path> \tDecompress the given .GCI image" },
/*  {TEST,0,"t" , "test",option::Arg::Optional, "  --[t]est <input PNG file path> \tTest compression to verify it is lossless" }, */
  {BENCHMARK,0,"b" , "benchmark",option::Arg::Optional, "  --[b]enchmark <test set path> \tTest compression ratio and decompression speed for a whole directory at once" },
  {PROFILE,0,"p" , "profile",option::Arg::Optional, "  --[p]rofile <input GCI file path> \tDecode same GCI file 100x to enhance profiling of decoder" },
  {REPLACE,0,"r" , "replace",option::Arg::Optional, "  --[r]eplace <directory path> \tCompress all images in the given directory, replacing the original if the GCIF version is smaller without changing file name" },
  {UNKNOWN, 0,"" ,  ""   ,option::Arg::None, "\nExamples:\n"
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

	int compression_level = 999999;
	if (options[L0]) {
		compression_level = 0;
	} else if (options[L1]) {
		compression_level = 1;
	} else if (options[L2]) {
		compression_level = 2;
	} else if (options[L3]) {
		compression_level = 3;
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
/*	} else if (options[TEST]) {
		CAT_FATAL("main") << "TODO";*/
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
	} else if (options[REPLACE]) {
		if (parse.nonOptionsCount() != 1) {
			CAT_WARN("main") << "Input error: Please provide input directory path";
		} else {
			const char *inFilePath = parse.nonOption(0);
			int err;

			if ((err = replace(inFilePath))) {
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

