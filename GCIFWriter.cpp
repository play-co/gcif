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

#include "GCIFWriter.hpp"
#include "ImageWriter.hpp"
#include "ImageMaskWriter.hpp"
#include "ImageLZWriter.hpp"
#include "ImageCMWriter.hpp"
using namespace cat;


// Default knobs for the normal compression levels

static const int COMPRESS_LEVELS = 4;
static const GCIFKnobs DEFAULT_KNOBS[COMPRESS_LEVELS] = {
	{	// L0 Faster
		0,			// Bump

		15,			// lz_huffThresh
		12,			// lz_minScore
		4,			// lz_nonzeroCoeff
		18,			// lz_tableBits

		false,		// cm_sortFilters
		false,		// cm_designFilters
		true,		// cm_disableEntropy
		4,			// cm_maxEntropySkip
		0,			// cm_filterSelectFuzz
		0,			// cm_revisitCount
		4000,		// cm_chaosThresh
		1.3f,		// cm_minTapQuality
		false		// cm_scanlineFilters
	},
	{	// L1 Better
		0,			// Bump

		15,			// lz_huffThresh
		12,			// lz_minScore
		4,			// lz_nonzeroCoeff
		18,			// lz_tableBits

		false,		// cm_sortFilters
		false,		// cm_designFilters
		false,		// cm_disableEntropy
		4,			// cm_maxEntropySkip
		64,			// cm_filterSelectFuzz
		0,			// cm_revisitCount
		4000,		// cm_chaosThresh
		1.3f,		// cm_minTapQuality
		false		// cm_scanlineFilters
	},
	{	// L2 Harder
		0,			// Bump

		15,			// lz_huffThresh
		12,			// lz_minScore
		4,			// lz_nonzeroCoeff
		18,			// lz_tableBits

		false,		// cm_sortFilters
		true,		// cm_designFilters
		false,		// cm_disableEntropy
		0,			// cm_maxEntropySkip
		256,		// cm_filterSelectFuzz
		0,			// cm_revisitCount
		4000,		// cm_chaosThresh
		1.3f,		// cm_minTapQuality
		true		// cm_scanlineFilters
	},
	{	// L3 Stronger
		0,			// Bump

		15,			// lz_huffThresh
		12,			// lz_minScore
		4,			// lz_nonzeroCoeff
		18,			// lz_tableBits

		false,		// cm_sortFilters
		true,		// cm_designFilters
		false,		// cm_disableEntropy
		0,			// cm_maxEntropySkip
		272,		// cm_filterSelectFuzz
		4096,		// cm_revisitCount
		4000,		// cm_chaosThresh
		1.3f,		// cm_minTapQuality
		true		// cm_scanlineFilters
	}
};


int gcif_write_ex(const void *rgba, int width, int height, const char *output_file_path, const GCIFKnobs *knobs) {
	// Validate input
	if (!rgba || width < 0 || height < 0 || !output_file_path || !*output_file_path) {
		return WE_BAD_PARAMS;
	}

	const u8 *image = reinterpret_cast<const u8*>( rgba );
	int err;

	// Initialize image writer
	ImageWriter writer;
	if ((err = writer.init(width, height))) {
		return err;
	}

	// Fully-Transparent Alpha Mask
	ImageMaskWriter imageMaskWriter;
	if ((err = imageMaskWriter.initFromRGBA(image, width, height, knobs))) {
		return err;
	}

	imageMaskWriter.write(writer);
	imageMaskWriter.dumpStats();

	// 2D-LZ Exact Match
	ImageLZWriter imageLZWriter;
	if ((err = imageLZWriter.initFromRGBA(image, width, height, knobs))) {
		return err;
	}

	imageLZWriter.write(writer);
	imageLZWriter.dumpStats();

	// Context Modeling Decompression
	ImageCMWriter imageCMWriter;
	if ((err = imageCMWriter.initFromRGBA(image, width, height, imageMaskWriter, imageLZWriter, knobs))) {
		return err;
	}

	imageCMWriter.write(writer);
	imageCMWriter.dumpStats();

	// Finalize file
	if ((err = writer.finalizeAndWrite(output_file_path))) {
		return err;
	}

	return WE_OK;
}

int gcif_write(const void *rgba, int width, int height, const char *output_file_path, int compression_level) {
	// Error on invalid input
	if (compression_level < 0) {
		return WE_BAD_PARAMS;
	}

	// Limit to the available options
	if (compression_level >= COMPRESS_LEVELS) {
		compression_level = COMPRESS_LEVELS - 1;
	}

	// Run with selected knobs
	const GCIFKnobs *knobs = &DEFAULT_KNOBS[compression_level];
	return gcif_write_ex(rgba, width, height, output_file_path, knobs);
}


const char *gcif_write_errstr(int err) {
	switch (err) {
		case WE_OK:			// No error
			return "No errors";
		case WE_BAD_DIMS:	// Image dimensions are invalid
			return "Image dimensions are invalid";
		case WE_FILE:		// Unable to access file
			return "Unable to access the file";
		case WE_BUG:		// Internal error
			return "Internal error";
		default:
			break;
	}

	return "Unknown error code";
}

