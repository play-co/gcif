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
		return "IOno:GCIF_WE_BUG";
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

		40,			// pal_huffThresh
		0.1f,		// pal_sympalThresh
		0.6f,		// pal_filterCoverThresh
		0.05f,		// pal_filterIncThresh
		{5,3,1,1},	// pal_awards
		true,		// pal_enableLZ

		true,		// rgba_fastMode
		0,			// rgba_revisitCount
		512,		// rgba_lzPrematchLimit
		0,			// rgba_lzInmatchLimit
		0.8f,		// rgba_filterCoverThresh
		0.005f,		// rgba_filterIncThresh
		{5,3,1,1},	// rgba_awards
		true,		// rgba_enableLZ

		0.1f,		// alpha_sympalThresh
		0.6f,		// alpha_filterCoverThresh
		0.05f,		// alpha_filterIncThresh
		{5,3,1,1},	// alpha_awards
		false,		// alpha_enableLZ

		0.1f,		// sf_sympalThresh
		0.6f,		// sf_filterCoverThresh
		0.05f,		// sf_filterIncThresh
		{5,3,1,1},	// sf_awards
		false,		// sf_enableLZ

		0.1f,		// cf_sympalThresh
		0.6f,		// cf_filterCoverThresh
		0.05f,		// cf_filterIncThresh
		{5,3,1,1},	// cf_awards
		false,		// cf_enableLZ

		0.1f,		// spal_sympalThresh
		0.6f,		// spal_filterCoverThresh
		0.05f,		// spal_filterIncThresh
		{5,3,1,1},	// spal_awards
		true,		// spal_enableLZ
	
		0,			// mono_revisitCount
		2070,		// mono_lzPrematchLimit
		512,		// mono_lzInmatchLimit
	},
	{	// L1 Better
		0,			// Bump

		40,			// mask_minColorRat
		60,			// mask_huffThresh

		40,			// pal_huffThresh
		0.1f,		// pal_sympalThresh
		0.6f,		// pal_filterCoverThresh
		0.05f,		// pal_filterIncThresh
		{5,3,1,1},	// pal_awards
		true,		// pal_enableLZ

		false,		// rgba_fastMode
		0,			// rgba_revisitCount
		2070,		// rgba_lzPrematchLimit
		512,		// rgba_lzInmatchLimit
		0.8f,		// rgba_filterCoverThresh
		0.005f,		// rgba_filterIncThresh
		{5,3,1,1},	// rgba_awards
		true,		// rgba_enableLZ

		0.1f,		// alpha_sympalThresh
		0.6f,		// alpha_filterCoverThresh
		0.05f,		// alpha_filterIncThresh
		{5,3,1,1},	// alpha_awards
		true,		// alpha_enableLZ

		0.1f,		// sf_sympalThresh
		0.6f,		// sf_filterCoverThresh
		0.05f,		// sf_filterIncThresh
		{5,3,1,1},	// sf_awards
		true,		// sf_enableLZ

		0.1f,		// cf_sympalThresh
		0.6f,		// cf_filterCoverThresh
		0.05f,		// cf_filterIncThresh
		{5,3,1,1},	// cf_awards
		true,		// cf_enableLZ

		0.1f,		// spal_sympalThresh
		0.6f,		// spal_filterCoverThresh
		0.05f,		// spal_filterIncThresh
		{5,3,1,1},	// spal_awards
		true,		// spal_enableLZ
	
		0,			// mono_revisitCount
		2070,		// mono_lzPrematchLimit
		512,		// mono_lzInmatchLimit
	},
	{	// L2 Harder
		0,			// Bump

		40,			// mask_minColorRat
		60,			// mask_huffThresh

		40,			// pal_huffThresh
		0.1f,		// pal_sympalThresh
		0.6f,		// pal_filterCoverThresh
		0.05f,		// pal_filterIncThresh
		{5,3,1,1},	// pal_awards
		true,		// pal_enableLZ

		false,		// rgba_fastMode
		0,			// rgba_revisitCount
		2070,		// rgba_lzPrematchLimit
		512,		// rgba_lzInmatchLimit
		0.8f,		// rgba_filterCoverThresh
		0.005f,		// rgba_filterIncThresh
		{5,3,1,1},	// rgba_awards
		true,		// rgba_enableLZ

		0.1f,		// alpha_sympalThresh
		0.6f,		// alpha_filterCoverThresh
		0.05f,		// alpha_filterIncThresh
		{5,3,1,1},	// alpha_awards
		true,		// alpha_enableLZ

		0.1f,		// sf_sympalThresh
		0.6f,		// sf_filterCoverThresh
		0.05f,		// sf_filterIncThresh
		{5,3,1,1},	// sf_awards
		true,		// sf_enableLZ

		0.1f,		// cf_sympalThresh
		0.6f,		// cf_filterCoverThresh
		0.05f,		// cf_filterIncThresh
		{5,3,1,1},	// cf_awards
		true,		// cf_enableLZ

		0.1f,		// spal_sympalThresh
		0.6f,		// spal_filterCoverThresh
		0.05f,		// spal_filterIncThresh
		{5,3,1,1},	// spal_awards
		true,		// spal_enableLZ
	
		0,			// mono_revisitCount
		2070,		// mono_lzPrematchLimit
		512,		// mono_lzInmatchLimit
	},
	{	// L3 Stronger
		0,			// Bump

		40,			// mask_minColorRat
		60,			// mask_huffThresh

		40,			// pal_huffThresh
		0.1f,		// pal_sympalThresh
		0.6f,		// pal_filterCoverThresh
		0.05f,		// pal_filterIncThresh
		{5,3,1,1},	// pal_awards
		true,		// pal_enableLZ

		false,		// rgba_fastMode
		4096,		// rgba_revisitCount
		2070,		// rgba_lzPrematchLimit
		512,		// rgba_lzInmatchLimit
		0.8f,		// rgba_filterCoverThresh
		0.005f,		// rgba_filterIncThresh
		{5,3,1,1},	// rgba_awards
		true,		// rgba_enableLZ

		0.1f,		// alpha_sympalThresh
		0.6f,		// alpha_filterCoverThresh
		0.05f,		// alpha_filterIncThresh
		{5,3,1,1},	// alpha_awards
		true,		// alpha_enableLZ

		0.1f,		// sf_sympalThresh
		0.6f,		// sf_filterCoverThresh
		0.05f,		// sf_filterIncThresh
		{5,3,1,1},	// sf_awards
		true,		// sf_enableLZ

		0.1f,		// cf_sympalThresh
		0.6f,		// cf_filterCoverThresh
		0.05f,		// cf_filterIncThresh
		{5,3,1,1},	// cf_awards
		true,		// cf_enableLZ

		0.1f,		// spal_sympalThresh
		0.6f,		// spal_filterCoverThresh
		0.05f,		// spal_filterIncThresh
		{5,3,1,1},	// spal_awards
		true,		// spal_enableLZ
	
		4096,		// mono_revisitCount
		2070,		// mono_lzPrematchLimit
		512,		// mono_lzInmatchLimit
	}
};


void stripTransparentRGB(const u8 *rgba, int xsize, int ysize, SmartArray<u8> &image) {
	image.resize(xsize * ysize * 4);

	u8 *p = image.get();
	for (int y = 0; y < ysize; ++y) {
		for (int x = 0; x < xsize; ++x) {
			if (rgba[3] == 0) {
				*(u32*)p = 0;
			} else {
				*(u32*)p = *(u32*)rgba;
			}
			rgba += 4;
			p += 4;
		}
	}
}


extern "C" int gcif_write_ex(const void *pixels, int xsize, int ysize, const char *output_file_path, const GCIFKnobs *knobs, int strip_transparent_color) {
	// Validate input
	if (!pixels || xsize < 0 || ysize < 0 || !output_file_path || !*output_file_path) {
		return GCIF_WE_BAD_PARAMS;
	}

	int err;

	// Select RGBA data from input pixels
	SmartArray<u8> image;
	const u8 *rgba = reinterpret_cast<const u8*>( pixels );

	// If stripping RGB color data from fully-transparent pixels,
	if (strip_transparent_color) {
		// Make a copy of the image and strip out the RGB information from fully-transparent pixels
		stripTransparentRGB(rgba, xsize, ysize, image);
		rgba = image.get();
	}

	// Initialize image writer
	ImageWriter writer;
	if ((err = writer.init(xsize, ysize))) {
		return err;
	}

	// Small Palette
	SmallPaletteWriter smallPaletteWriter;
	if ((err = smallPaletteWriter.init(rgba, xsize, ysize, knobs))) {
		return err;
	}

	smallPaletteWriter.writeHead(writer);

	// If small palette mode is enabled,
	if (smallPaletteWriter.enabled()) {
		// If not just a single color,
		if (!smallPaletteWriter.isSingleColor()) {
			const int pack_x = smallPaletteWriter.getPackX();
			const int pack_y = smallPaletteWriter.getPackY();
			const u8 *pack_image = smallPaletteWriter.get();

			// Dominant Color Mask
			ImageMaskWriter imageMaskWriter;
			if ((err = imageMaskWriter.init(pack_image, 1, pack_x, pack_y, knobs))) {
				return err;
			}

			imageMaskWriter.write(writer);
			imageMaskWriter.dumpStats();

			// Small Palette Compression
			if ((err = smallPaletteWriter.compress(imageMaskWriter))) {
				return err;
			}

			smallPaletteWriter.writeTail(writer);
		}

		smallPaletteWriter.dumpStats();
	} else {
		// Dominant Color Mask
		ImageMaskWriter imageMaskWriter;
		if ((err = imageMaskWriter.init(rgba, 4, xsize, ysize, knobs))) {
			return err;
		}

		imageMaskWriter.write(writer);
		imageMaskWriter.dumpStats();

		// Global Palette
		ImagePaletteWriter imagePaletteWriter;
		if ((err = imagePaletteWriter.init(rgba, xsize, ysize, knobs, imageMaskWriter))) {
			return err;
		}

		imagePaletteWriter.write(writer);
		imagePaletteWriter.dumpStats();

		if (!imagePaletteWriter.enabled()) {
			// Context Modeling Decompression
			ImageRGBAWriter imageRGBAWriter;
			if ((err = imageRGBAWriter.init(rgba, xsize, ysize, imageMaskWriter, knobs))) {
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

extern "C" int gcif_write(const void *rgba, int xsize, int ysize, const char *output_file_path, int compression_level, int strip_transparent_color) {
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
	return gcif_write_ex(rgba, xsize, ysize, output_file_path, knobs, strip_transparent_color);
}
