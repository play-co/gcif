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

#ifndef GCIF_WRITER_H
#define GCIF_WRITER_H


#ifdef __cplusplus
extern "C" {
#endif


// When API functions return an int, it's all about this:
enum WriteErrors {
	GCIF_WE_OK,			// No problemo

	GCIF_WE_BAD_PARAMS,	// Bad parameters passed to gcif_write
	GCIF_WE_BAD_DIMS,	// Image dimensions are invalid
	GCIF_WE_FILE,		// Unable to access file
	GCIF_WE_BUG			// Internal error
};

// Returns an error string for a return value from gcif_write()
const char *gcif_write_errstr(int err);


/*
 * gcif_write()
 *
 * rgba: Pass in the 8-bit/channel RGBA raster data in OpenGL RGBA8888 format, row-first, stride = width
 * width: Pixels per row
 * height: Pixels per column
 * output_file_path: File write location
 * compression_level:
 * 		0 = Faster
 * 		1 = Better
 * 		2 = Harder
 * 		3 = Stronger
 */
int gcif_write(const void *rgba, int width, int height, const char *output_file_path, int compression_level);


// Extra twiddly knobs to eke out better performance if you prefer
struct GCIFKnobs {
	// Seed used for any randomized selections, may help discover improvements
	int bump;				// 0

	//// Image Mask writer

	int mask_minColorRat;	// 20: Minimum color pixel compression ratio
	int mask_huffThresh;	// 60: Minimum post-LZ bytes to compress the data

	//// Image 2D-LZ writer

	int lz_huffThresh;		// 15: Minimum zones to compress the data
	int lz_minScore;		// 12: Minimum score to add an LZ match
	int lz_nonzeroCoeff;	// 4: Number of times nonzeroes worth over zeroes
	int lz_tableBits;		// 18: Hash table bits

	//// Image Mono writer

	int mono_revisitCount;	// 4096: Number of pixels to revisit

	//// Image CM writer

	bool cm_sortFilters;		// false: L1 sort filter options before entropy
	bool cm_designFilters;		// true: Try out custom filter taps
	bool cm_disableEntropy;		// false: Disable entropy testing (faster)
	int cm_maxEntropySkip;		// 4: Max filter error to skip entropy test
	int cm_filterSelectFuzz;	// 256: Top L1 norm scored count for entropy test
	int cm_revisitCount;		// 4096: Number of pixels to revisit
	int cm_chaosThresh;			// 4000: Min # of chaos pixels to use 8-level tables
	float cm_minTapQuality;		// 1.3: Min coverage improvement to accept one
	bool cm_scanlineFilters;	// true: Try doing scanline filters
};

/*
 * Same as gcif_write() except the compression level is replaced with the
 * knobs structure which gives you full control over the available options
 * controlling how the compressor works.
 */
int gcif_write_ex(const void *rgba, int width, int height, const char *output_file_path, const GCIFKnobs *knobs);


#ifdef __cplusplus
};
#endif


#endif // GCIF_WRITER_H
