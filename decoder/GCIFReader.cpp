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

#include "GCIFReader.h"
#include "ImageReader.hpp"
#include "ImageMaskReader.hpp"
#include "ImageLZReader.hpp"
#include "ImagePaletteReader.hpp"
#include "ImageRGBAReader.hpp"
#include "EndianNeutral.hpp"
using namespace cat;

static int gcif_read(ImageReader &reader, GCIFImage *image) {
	int err;

	// Fill in image size_x and size_y
	ImageReader::Header *header = reader.getHeader();
	image->size_x = header->size_x;
	image->size_y = header->size_y;

	// Color mask
	ImageMaskReader maskReader;
	if ((err = maskReader.read(reader))) {
		return err;
	}
	maskReader.dumpStats();

	// 2D-LZ Exact Match
	ImageLZReader imageLZReader;
	if ((err = imageLZReader.read(reader))) {
		return err;
	}
	imageLZReader.dumpStats();

	// Global Palette Decompression
	ImagePaletteReader imagePaletteReader;
	if ((err = imagePaletteReader.read(reader, maskReader, imageLZReader, image))) {
		return err;
	}
	imagePaletteReader.dumpStats();

	if (!imagePaletteReader.enabled()) {
		// RGBA Decompression
		ImageRGBAReader imageRGBAReader;
		if ((err = imageRGBAReader.read(reader, maskReader, imageLZReader, image))) {
			return err;
		}
		imageRGBAReader.dumpStats();
	}

	return GCIF_RE_OK;
}

#ifdef CAT_COMPILE_MMAP

extern "C" int gcif_read_file(const char *input_file_path_in, GCIFImage *image_out) {
	int err;

	// Initialize image data
	image_out->rgba = 0;
	image_out->size_x = -1;
	image_out->size_y = -1;

	// Initialize image reader
	ImageReader reader;
	if ((err = reader.init(input_file_path_in))) {
		return err;
	}

	return gcif_read(reader, image_out);
}

#endif // CAT_COMPILE_MMAP

extern "C" int gcif_get_size(const void *file_data_in, long file_size_bytes_in, int *size_x, int *size_y) {
	// Validate length
	if (file_size_bytes_in < 4) {
		return GCIF_RE_BAD_HEAD;
	}

	// Validate signature
	const u32 *head_word = reinterpret_cast<const u32 *>( file_data_in );
	u32 sig = getLE(head_word[0]);
	if (sig != ImageReader::HEAD_MAGIC) {
		return GCIF_RE_BAD_HEAD;
	}

	// Read size_x, size_y
	u32 word1 = getLE(head_word[1]);
	*size_x = (u16)((word1 >> (32 - ImageReader::MAX_X_BITS)) & ((1 << ImageReader::MAX_X_BITS) - 1));
	*size_y = (u16)((word1 >> (32 - ImageReader::MAX_X_BITS - ImageReader::MAX_Y_BITS)) & ((1 << ImageReader::MAX_Y_BITS) - 1));

	return GCIF_RE_OK;
}

extern "C" int gcif_sig_cmp(const void *file_data_in, long file_size_bytes_in) {
	// Validate length
	if (file_size_bytes_in < 4) {
		return GCIF_RE_BAD_HEAD;
	}

	// Validate signature
	const u32 *head_word = reinterpret_cast<const u32 *>( file_data_in );
	u32 sig = getLE(head_word[0]);
	if (sig != ImageReader::HEAD_MAGIC) {
		return GCIF_RE_BAD_HEAD;
	}

	return GCIF_RE_OK;
}

extern "C" int gcif_read_memory(const void *file_data_in, long file_size_bytes_in, GCIFImage *image_out) {
	int err;

	// Initialize image data
	image_out->rgba = 0;
	image_out->size_x = -1;
	image_out->size_y = -1;

	// Initialize image reader
	ImageReader reader;
	if ((err = reader.init(file_data_in, file_size_bytes_in))) {
		return err;
	}

	return gcif_read(reader, image_out);
}

extern "C" void gcif_free_image(const void *rgba) {
	// If image data was allocated,
	if (rgba) {
		// Free it
		delete []reinterpret_cast<const u8 *>( rgba );
	}
}

extern "C" const char *gcif_read_errstr(int err) {
	switch (err) {
		case GCIF_RE_OK:			// No problemo
			return "OK";

		case GCIF_RE_FILE:		// File access error
			return "File access error:GCIF_RE_FILE";
		case GCIF_RE_BAD_HEAD:	// File header is bad
			return "Wrong file type:GCIF_RE_BAD_HEAD";
		case GCIF_RE_BAD_DATA:	// File data is bad
			return "Corrupted:GCIF_RE_BAD_DATA";
		case GCIF_RE_BAD_DIMS:	// Bad image dimensions
			return "Corrupted:GCIF_RE_BAD_DIMS";

		case GCIF_RE_MASK_CODES:	// Mask codelen read failed
			return "Corrupted:GCIF_RE_MASK_CODES";
		case GCIF_RE_MASK_DECI:	// Mask decode init failed
			return "Corrupted:GCIF_RE_MASK_DECI";
		case GCIF_RE_MASK_LZ:	// Mask LZ decode failed
			return "Corrupted:GCIF_RE_MASK_LZ";

		case GCIF_RE_LZ_CODES:	// LZ codelen read failed
			return "Corrupted:GCIF_RE_LZ_CODES";
		case GCIF_RE_LZ_BAD:		// Bad data in LZ section
			return "Corrupted:GCIF_RE_LZ_BAD";

		case GCIF_RE_BAD_PAL:		// Bad data in Palette section
			return "Corrupted:GCIF_RE_BAD_PAL";

		case GCIF_RE_BAD_MONO:		// Bad data in Mono section
			return "Corrupted:GCIF_RE_BAD_MONO";

		case GCIF_RE_BAD_RGBA:		// Bad data in RGBA section
			return "Corrupted:GCIF_RE_BAD_RGBA";

		default:
			break;
	}

	return "(Unknown error code)";
}

