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
#include "decoder/MappedFile.hpp"
using namespace cat;

#include "optionparser.h"
#include "encoder/lodepng.h"

#ifdef CAT_ENABLE_LIBPNG
#include <png.h>
#endif

//#define CAT_BENCH_ONE /* Benchmark on one thread one file at a time to determine where in a benchmark there is a problem */


#ifdef CAT_ENABLE_LIBPNG

//helper function for load_png_from_memory
struct png_input_data {
	unsigned char *bits;
};

void png_image_bytes_read(png_structp png_ptr, png_bytep data, png_size_t length) {
	png_input_data *a = (png_input_data*)png_get_io_ptr(png_ptr);

	memcpy(data, a->bits, length);
	a->bits += length;
}

static void readpng2_error_handler(png_structp png_ptr, 
                                   png_const_charp msg)
{
	jmp_buf *jbuf;
  
    CAT_WARN("libpng") << "PNG image is corrupted.  Error=" << msg;
  
    jbuf = (jmp_buf*)png_get_error_ptr(png_ptr);
  
    longjmp(*jbuf, 1);
}

unsigned char *load_png_from_memory(unsigned char *bits, int *width, int *height, int *channels) {
	jmp_buf jbuf;

	//create png struct
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, &jbuf, readpng2_error_handler, NULL);

	if (!png_ptr) {
		return NULL;
	}

	//create png info struct
	png_infop info_ptr = png_create_info_struct(png_ptr);

	if (!info_ptr) {
		png_destroy_read_struct(&png_ptr, (png_infopp) NULL, (png_infopp) NULL);
		return NULL;
	}

	//create png info struct
	png_infop end_info = png_create_info_struct(png_ptr);

	if (!end_info) {
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
		return NULL;
	}

	if (setjmp(jbuf)) {
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return NULL;
	}

	png_input_data inputData = {bits + 8};
	png_set_read_fn(png_ptr, &inputData, png_image_bytes_read);
	//let libpng know you already read the first 8 bytes
	png_set_sig_bytes(png_ptr, 8);
	// read all the info up to the image data
	png_read_info(png_ptr, info_ptr);
	int bit_depth, color_type;
	png_uint_32 twidth, theight;
	// get info about png
	png_get_IHDR(png_ptr, info_ptr, &twidth, &theight, &bit_depth, &color_type,
	             NULL, NULL, NULL);

	// If color type would be paletted,
	if (color_type & PNG_COLOR_TYPE_PALETTE) {
		// Convert to RGB to be OpenGL compatible
		png_set_palette_to_rgb(png_ptr);
	}

	// If color type would be grayscale and bit depth is low,
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
		// Bump it up to be OpenGL compatible
		png_set_expand_gray_1_2_4_to_8(png_ptr);
	}

	//update width and height based on png info
	*width = twidth;
	*height = theight;
	// Update the png info struct.
	png_read_update_info(png_ptr, info_ptr);
	*channels = (int)png_get_channels(png_ptr, info_ptr);
	int rowbytes = png_get_rowbytes(png_ptr, info_ptr);
	// Allocate the image_data as a big block, to be given to opengl
	unsigned char  *image_data = (unsigned char *) malloc(rowbytes * (*height));

	if (!image_data) {
		//clean up memory and close stuff
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return NULL;
	}

	//row_pointers is for pointing to image_data for reading the png with libpng
	png_bytep *row_pointers = (png_bytep *)malloc((*height) * sizeof(png_bytep));

	if (!row_pointers) {
		//clean up memory and close stuff
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		free(image_data);
		return NULL;
	}

	// set the individual row_pointers to point at the correct offsets of image_data
	int i;

	for (i = 0; i < *height; ++i) {
		row_pointers[i] = image_data + i * rowbytes;
	}

	//read the png into image_data through row_pointers
	png_read_image(png_ptr, row_pointers);
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	free(row_pointers);
	return image_data;
}

#endif






//// Commands

static int compress(const char *filename, const char *outfile, int compress_level, int strip_transparent_color) {
	vector<unsigned char> image;
	unsigned xsize, ysize;

	CAT_WARN("main") << "Reading input PNG image file: " << filename;

	unsigned error = lodepng::decode(image, xsize, ysize, filename);

	if (error) {
		CAT_WARN("main") << "PNG read error " << error << ": " << lodepng_error_text(error);
		return error;
	}

	CAT_WARN("main") << "Encoding image: " << outfile;

	int err;

	if ((err = gcif_write(&image[0], xsize, ysize, outfile, compress_level, strip_transparent_color))) {
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

	lodepng_encode_file(outfile, (const unsigned char*)image.rgba, image.xsize, image.ysize, LCT_RGBA, 8);

	free(image.rgba);

	return GCIF_RE_OK;
}

#include <sys/stat.h>

static int benchfile(string filename) {
	vector<unsigned char> image;
	unsigned xsize = 0, ysize = 0;

	const int compress_level = 9999;

	double t0 = Clock::ref()->usec();

	unsigned error = lodepng::decode(image, xsize, ysize, filename);

	double t1 = Clock::ref()->usec();

	if (error) {
		CAT_WARN("main") << "PNG read error " << error << ": " << lodepng_error_text(error) << " for " << filename;
		return 0;
		//return error;
	}

	int err;

	string benchfile = filename + ".lz.gci";
	const char *cbenchfile = benchfile.c_str();

	const int strip_transparent_color = 1;

#ifdef CAT_BENCH_ONE
	CAT_WARN("main") << "Compressing: " << filename;
#endif
	if ((err = gcif_write(&image[0], xsize, ysize, cbenchfile, compress_level, strip_transparent_color))) {
		CAT_WARN("main") << "Error while compressing the image: " << gcif_write_errstr(err) << " for " << filename;
		return err;
	}

	double t2 = Clock::ref()->usec();

#ifdef CAT_BENCH_ONE
	CAT_WARN("main") << "Decompressing: " << filename;
#endif
	GCIFImage outimage;
	if ((err = gcif_read_file(cbenchfile, &outimage))) {
		CAT_WARN("main") << "Error while decompressing the image: " << gcif_read_errstr(err) << " for " << filename;
		return err;
	}

	double t3 = Clock::ref()->usec();

	for (u32 ii = 0; ii < xsize * ysize * 4; ii += 4) {
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

#ifdef CAT_BENCH_ONE
	cpu_count = 1;
#endif

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
	unsigned xsize, ysize;

	const int compress_level = 9999;

	double t0 = Clock::ref()->usec();

	unsigned error = lodepng::decode(image, xsize, ysize, filename);

	double t1 = Clock::ref()->usec();

	if (error) {
		CAT_WARN("main") << "PNG read error " << error << ": " << lodepng_error_text(error) << " for " << filename;
		return 0;
		//return error;
	}

	int err;

	string benchfile = filename + ".gci";
	const char *cbenchfile = benchfile.c_str();

	const int strip_transparent_color = 1;

	if ((err = gcif_write(&image[0], xsize, ysize, cbenchfile, compress_level, strip_transparent_color))) {
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

	for (u32 ii = 0; ii < xsize * ysize * 4; ii += 4) {
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





static int testfile(string filename) {
	vector<unsigned char> image;
	unsigned xsize, ysize;

	const int compress_level = 9999;

	double t0 = Clock::ref()->usec();

	unsigned error = lodepng::decode(image, xsize, ysize, filename);

	double t1 = Clock::ref()->usec();

	if (error) {
		CAT_WARN("main") << "PNG read error " << error << ": " << lodepng_error_text(error) << " for " << filename;
		return 0;
		//return error;
	}

	int err;

	string benchfile = filename + ".gci";
	const char *cbenchfile = benchfile.c_str();

	const int strip_transparent_color = 1;

	if ((err = gcif_write(&image[0], xsize, ysize, cbenchfile, compress_level, strip_transparent_color))) {
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

	for (u32 ii = 0; ii < xsize * ysize * 4; ii += 4) {
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
	const char *cfilename = filename.c_str();
	stat(cfilename, &png);
	stat(cbenchfile, &gci);

	u32 pngBytes = png.st_size;
	u32 gciBytes = gci.st_size;
	double gcipngrat = pngBytes / (double)gciBytes;

	double pngtime = t1 - t0;
	double gcitime = t3 - t2;
	double gcipngtime = pngtime / gcitime;

	CAT_WARN("main") << filename << " => " << gcipngrat << "x smaller than PNG and decompresses " << gcipngtime << "x faster";

	free(outimage.rgba);

	return GCIF_RE_OK;
}




static int profileit(const char *filename) {
	CAT_WARN("main") << "Decoding input GCIF image file hard: " << filename;

	int err;

	Clock *clock = Clock::ref();


	const int ITERATIONS = 100;

	{
		MappedFile _file;
		MappedView _fileView;

		if CAT_UNLIKELY(!_file.OpenRead(filename)) {
			return GCIF_RE_FILE;
		}

		if CAT_UNLIKELY(!_fileView.Open(&_file)) {
			return GCIF_RE_FILE;
		}

		u8 * CAT_RESTRICT fileData = _fileView.MapView();
		if CAT_UNLIKELY(!fileData) {
			return GCIF_RE_FILE;
		}

		int fileLen = _fileView.GetLength();

		CAT_WARN("main") << "Read " << filename << " : " << fileLen << " bytes";



		double t0 = clock->usec();

		for (int ii = 0; ii < ITERATIONS; ++ii) {
			GCIFImage image;
			if ((err = gcif_read_memory(fileData, fileLen, &image))) {
				CAT_WARN("main") << "Error while decompressing the image: " << gcif_read_errstr(err);
				return err;
			}

			free(image.rgba);
		}

		double t1 = clock->usec();

		CAT_WARN("main") << "GCIF takes average of " << (t1 - t0) / ITERATIONS << " usec / read";
	}

#ifdef CAT_ENABLE_LIBPNG

	{
		MappedFile _file;
		MappedView _fileView;

		const char *pngfile = "original.png";

		if CAT_UNLIKELY(!_file.OpenRead(pngfile)) {
			return GCIF_RE_FILE;
		}

		if CAT_UNLIKELY(!_fileView.Open(&_file)) {
			return GCIF_RE_FILE;
		}

		u8 * CAT_RESTRICT fileData = _fileView.MapView();
		if CAT_UNLIKELY(!fileData) {
			return GCIF_RE_FILE;
		}

		int fileLen = _fileView.GetLength();

		CAT_WARN("main") << "Read " << pngfile << " : " << fileLen << " bytes";




		double t0 = clock->usec();

		for (int ii = 0; ii < ITERATIONS; ++ii) {
			int xsize, ysize, channels;

			void *a = load_png_from_memory(fileData, &xsize, &ysize, &channels);

			if (!a) {
				CAT_WARN("main") << "Error while loading with libpng";
				return 1;
			}

			free(a);
		}

		double t1 = clock->usec();

		CAT_WARN("main") << "PNG takes average of " << (t1 - t0) / ITERATIONS << " usec / read";
	}

#endif

	return GCIF_RE_OK;
}


//// Command-line parameter parsing

enum  optionIndex { UNKNOWN, HELP, L0, L1, L2, L3, VERBOSE, SILENT, COMPRESS, DECOMPRESS, TEST, BENCHMARK, PROFILE, REPLACE, NOSTRIP };
const option::Descriptor usage[] =
{
  {UNKNOWN, 0,"" , ""    ,option::Arg::None, "USAGE: ./gcif [options] [output file path]\n\n"
                                             "Options:" },
  {HELP,    0,"h", "help",option::Arg::None, "  --[h]elp  \tPrint usage and exit." },
  {VERBOSE,0,"v" , "verbose",option::Arg::None, "  --[v]erbose \tVerbose console output" },
  {L0,0,"0" , "faster",option::Arg::None, "  -0 \tCompression level 0 : Faster" },
  {L1,0,"1" , "better",option::Arg::None, "  -1 \tCompression level 1 : Better" },
  {L2,0,"2" , "harder",option::Arg::None, "  -2 \tCompression level 2 : Harder" },
  {L3,0,"3" , "stronger",option::Arg::None, "  -3 \tCompression level 3 : Stronger (default)" },
  {SILENT,0,"s" , "silent",option::Arg::None, "  --[s]ilent \tNo console output (even on errors)" },
  {COMPRESS,0,"c" , "compress",option::Arg::Optional, "  --[c]ompress <input PNG file path> \tCompress the given .PNG image." },
  {DECOMPRESS,0,"d" , "decompress",option::Arg::Optional, "  --[d]ecompress <input GCI file path> \tDecompress the given .GCI image" },
  {TEST,0,"t" , "test",option::Arg::Optional, "  --[t]est <input PNG file path> \tTest compression to verify it is lossless" },
  {BENCHMARK,0,"b" , "benchmark",option::Arg::Optional, "  --[b]enchmark <test set path> \tTest compression ratio and decompression speed for a whole directory at once" },
  {PROFILE,0,"p" , "profile",option::Arg::Optional, "  --[p]rofile <input GCI file path> \tDecode same GCI file 100x to enhance profiling of decoder" },
  {REPLACE,0,"r" , "replace",option::Arg::Optional, "  --[r]eplace <directory path> \tCompress all images in the given directory, replacing the original if the GCIF version is smaller without changing file name" },
  {NOSTRIP,0,"n" , "nostrip",option::Arg::Optional, "  --[n]ostrip \tDo not strip RGB color data from fully-transparent pixels.  The default is to remove this color data.  Saving it can be useful in some rare cases" },
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

	int strip_transparent_color = 1; // default
	if (options[NOSTRIP]) {
		strip_transparent_color = 0;
	}

	int compression_level = 999999; // default

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

			if ((err = compress(inFilePath, outFilePath, compression_level, strip_transparent_color))) {
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
		if (parse.nonOptionsCount() != 1) {
			CAT_WARN("main") << "Input error: Please provide input file path";
		} else {
			const char *inFilePath = parse.nonOption(0);
			int err;

			if ((err = testfile(inFilePath))) {
				CAT_INFO("main") << "Error during conversion [retcode:" << err << "]";
				return err;
			}

			return 0;
		}
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

