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
 * rgba: Pass in the 8-bit/channel RGBA raster data in OpenGL RGBA8888 format, row-first, stride = xsize
 * xsize: Pixels per row
 * ysize: Pixels per column
 * output_file_path: File write location
 * compression_level:
 * 		0 = Faster
 * 		1 = Better
 * 		2 = Harder
 * 		3 = Stronger
 * strip_transparent_color: 0 = No. 1 = Yes, strip RGB data from fully transparent pixels.
 */
int gcif_write(const void *rgba, int xsize, int ysize, const char *output_file_path, int compression_level, int strip_transparent_color);


// Extra twiddly knobs to eke out better performance if you prefer
struct GCIFKnobs {
	// Seed used for any randomized selections, may help discover improvements
	int bump;				// 0

	//// Image Mask writer
	int mask_minColorRat;	// 20: Minimum color pixel compression ratio
	int mask_huffThresh;	// 60: Minimum post-LZ bytes to compress the data

	//// Image Palette writer
	int pal_huffThresh;				// 40: Palette size to start using Huffman encoding
	float pal_sympalThresh;			// 0.1: Percentage of pixels covered by a color before it is chosen as a dedicated filter code
	float pal_filterCoverThresh;	// 0.6: Sufficient coverage ratio of filter scores before stopping filter selection
	float pal_filterIncThresh;		// 0.05: Minimum score improvement required from filters
	int pal_awards[4];				// {5,3,1,1}: Points (1-5) to award for first-fourth place in filter competition per tile
	bool pal_enableLZ;				// true: Enable LZ compression of palette-encoded pixels

	//// Image RGBA writer
	int rgba_lzPrematchLimit;		// 2070: How far to walk the hash chain during LZ match finding on first pixel of a match
	int rgba_lzInmatchLimit;		// 512: How far to walk the hash chain during LZ match finding inside a match (for optimal matching)

	//// Image Mono writer
	int mono_revisitCount;	// 4096: Number of pixels to revisit

	//// Image CM writer
	bool cm_disableEntropy;		// false: Disable entropy testing (faster)
	int cm_maxEntropySkip;		// 4: Max filter error to skip entropy test
	int cm_filterSelectFuzz;	// 256: Top L1 norm scored count for entropy test
	int cm_revisitCount;		// 4096: Number of pixels to revisit
	int cm_chaosThresh;			// 4000: Min # of chaos pixels to use 8-level tables
	float cm_minTapQuality;		// 1.3: Min coverage improvement to accept one
};

/*
 * Same as gcif_write() except the compression level is replaced with the
 * knobs structure which gives you full control over the available options
 * controlling how the compressor works.
 */
int gcif_write_ex(const void *rgba, int xsize, int ysize, const char *output_file_path, const GCIFKnobs *knobs, int strip_transparent_color);


#ifdef __cplusplus
};
#endif


#endif // GCIF_WRITER_H
