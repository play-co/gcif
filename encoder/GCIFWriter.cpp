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

#include "GCIFWriter.h"
#include "ImageWriter.hpp"
#include "ImageMaskWriter.hpp"
#include "ImageLZWriter.hpp"
#include "ImagePaletteWriter.hpp"
#include "ImageRGBAWriter.hpp"
#include "SmallPaletteWriter.hpp"
using namespace cat;


extern "C" const char *gcif_write_errstr(int err) {
	switch (err) {
	case GCIF_WE_OK:			// No error
		return "OK";
	case GCIF_WE_BAD_DIMS:	// Image dimensions are invalid
		return "Bad image dimensions:GCIF_WE_BAD_DIMS";
	case GCIF_WE_FILE:		// Unable to access file
		return "File access error:GCIF_WE_FILE";
	case GCIF_WE_BUG:		// Internal error
		return "Iunno:GCIF_WE_BUG";
	default:
		break;
	}

	return "Unknown error code";
}


// Default knobs for the normal compression levels

static const int COMPRESS_LEVELS = 4;
static const GCIFKnobs DEFAULT_KNOBS[COMPRESS_LEVELS] = {
	{	// L0 Faster
		0,			// Bump

		40,			// mask_minColorRat
		60,			// mask_huffThresh

		15,			// lz_huffThresh
		16,			// lz_minScore
		4,			// lz_nonzeroCoeff
		18,			// lz_tableBits

		0,			// mono_revisitCount

		true,		// cm_disableEntropy
		4,			// cm_maxEntropySkip
		0,			// cm_filterSelectFuzz
		0,			// cm_revisitCount
		4000,		// cm_chaosThresh
		1.3f,		// cm_minTapQuality
	},
	{	// L1 Better
		0,			// Bump

		40,			// mask_minColorRat
		60,			// mask_huffThresh

		15,			// lz_huffThresh
		16,			// lz_minScore
		4,			// lz_nonzeroCoeff
		18,			// lz_tableBits

		0,			// mono_revisitCount

		false,		// cm_disableEntropy
		4,			// cm_maxEntropySkip
		64,			// cm_filterSelectFuzz
		0,			// cm_revisitCount
		4000,		// cm_chaosThresh
		1.3f,		// cm_minTapQuality
	},
	{	// L2 Harder
		0,			// Bump

		40,			// mask_minColorRat
		60,			// mask_huffThresh

		15,			// lz_huffThresh
		16,			// lz_minScore
		4,			// lz_nonzeroCoeff
		18,			// lz_tableBits

		0,			// mono_revisitCount

		false,		// cm_disableEntropy
		0,			// cm_maxEntropySkip
		256,		// cm_filterSelectFuzz
		0,			// cm_revisitCount
		4000,		// cm_chaosThresh
		1.3f,		// cm_minTapQuality
	},
	{	// L3 Stronger
		0,			// Bump

		40,			// mask_minColorRat
		60,			// mask_huffThresh

		15,			// lz_huffThresh
		16,			// lz_minScore
		4,			// lz_nonzeroCoeff
		19,			// lz_tableBits

		4096,			// mono_revisitCount

		false,		// cm_disableEntropy
		0,			// cm_maxEntropySkip
		272,		// cm_filterSelectFuzz
		4096,		// cm_revisitCount
		4000,		// cm_chaosThresh
		1.3f,		// cm_minTapQuality
	}
};


extern "C" int gcif_write_ex(const void *rgba, int size_x, int size_y, const char *output_file_path, const GCIFKnobs *knobs) {
	// Validate input
	if (!rgba || size_x < 0 || size_y < 0 || !output_file_path || !*output_file_path) {
		return GCIF_WE_BAD_PARAMS;
	}

	const u8 *image = reinterpret_cast<const u8*>( rgba );
	int err;

	// Initialize image writer
	ImageWriter writer;
	if ((err = writer.init(size_x, size_y))) {
		return err;
	}

	// Small Palette
	SmallPaletteWriter imageSmallPalette;
	if ((err = imageSmallPalette.init(image, size_x, size_y, knobs))) {
		return err;
	}

	imageSmallPalette.write(writer);
	imageSmallPalette.dumpStats();

	// If small palette mode is enabled,
	if (imageSmallPalette.enabled()) {
		// If not just a single color,
		if (!imageSmallPalette.isSingleColor()) {
			const int pack_x = imageSmallPalette.getPackX();
			const int pack_y = imageSmallPalette.getPackY();
			const u8 *pack_image = imageSmallPalette.get();

			// Dominant Color Mask
			ImageMaskWriter imageMaskWriter;
			if ((err = imageMaskWriter.init(pack_image, 1, pack_x, pack_y, knobs))) {
				return err;
			}

			imageMaskWriter.write(writer);
			imageMaskWriter.dumpStats();

			// 2D-LZ Exact Match
			ImageLZWriter imageLZWriter;
			if ((err = imageLZWriter.init(pack_image, 1, pack_x, pack_y, knobs))) {
				return err;
			}

			imageLZWriter.write(writer);
			imageLZWriter.dumpStats();

			// Global Palette
			ImagePaletteWriter imagePaletteWriter;
			if ((err = imagePaletteWriter.init(pack_image, 1, pack_x, pack_y, knobs, imageMaskWriter, imageLZWriter))) {
				return err;
			}

			imagePaletteWriter.write(writer);
			imagePaletteWriter.dumpStats();

			CAT_DEBUG_ENFORCE(imagePaletteWriter.enabled());
		}
	} else {
		// Dominant Color Mask
		ImageMaskWriter imageMaskWriter;
		if ((err = imageMaskWriter.init(image, 4, size_x, size_y, knobs))) {
			return err;
		}

		imageMaskWriter.write(writer);
		imageMaskWriter.dumpStats();

		// 2D-LZ Exact Match
		ImageLZWriter imageLZWriter;
		if ((err = imageLZWriter.init(image, 4, size_x, size_y, knobs))) {
			return err;
		}

		imageLZWriter.write(writer);
		imageLZWriter.dumpStats();

		// Global Palette
		ImagePaletteWriter imagePaletteWriter;
		if ((err = imagePaletteWriter.init(image, 4, size_x, size_y, knobs, imageMaskWriter, imageLZWriter))) {
			return err;
		}

		imagePaletteWriter.write(writer);
		imagePaletteWriter.dumpStats();

		if (!imagePaletteWriter.enabled()) {
			// Context Modeling Decompression
			ImageRGBAWriter imageRGBAWriter;
			if ((err = imageRGBAWriter.init(image, size_x, size_y, imageMaskWriter, imageLZWriter, knobs))) {
				return err;
			}

			imageRGBAWriter.write(writer);
			imageRGBAWriter.dumpStats();
		}
	}

	// Finalize file
	writer.finalize();

	// Write it out
	if ((err = writer.write(output_file_path))) {
		return err;
	}

	return GCIF_WE_OK;
}

extern "C" int gcif_write(const void *rgba, int size_x, int size_y, const char *output_file_path, int compression_level) {
	// Error on invalid input
	if (compression_level < 0) {
		return GCIF_WE_BAD_PARAMS;
	}

	// Limit to the available options
	if (compression_level >= COMPRESS_LEVELS) {
		compression_level = COMPRESS_LEVELS - 1;
	}

	const GCIFKnobs *knobs = &DEFAULT_KNOBS[compression_level];

	// Run with selected knobs
	return gcif_write_ex(rgba, size_x, size_y, output_file_path, knobs);
}
