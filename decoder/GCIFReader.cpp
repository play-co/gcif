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
#include "ImageCMReader.hpp"
#include "EndianNeutral.hpp"
using namespace cat;

static int gcif_read(ImageReader &reader, GCIFImage *image) {
	int err;

	// Fill in image width and height
	ImageHeader *header = reader.getImageHeader();
	image->width = header->width;
	image->height = header->height;

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

	// Context Modeling Decompression
	ImageCMReader imageCMReader;
	if ((err = imageCMReader.read(reader, maskReader, imageLZReader, image))) {
		return err;
	}
	imageCMReader.dumpStats();

	// Verify hash
	if (!reader.finalizeCheckHash()) {
		return GCIF_RE_BAD_HASH;
	}

	return GCIF_RE_OK;
}

#ifdef CAT_COMPILE_MMAP

extern "C" int gcif_read_file(const char *input_file_path_in, GCIFImage *image_out) {
	int err;

	// Initialize image data
	image_out->rgba = 0;
	image_out->width = -1;
	image_out->height = -1;

	// Initialize image reader
	ImageReader reader;
	if ((err = reader.init(input_file_path_in))) {
		return err;
	}

	return gcif_read(reader, image_out);
}

#endif // CAT_COMPILE_MMAP

extern "C" int gcif_sig_cmp(const void *file_data_in, long file_size_bytes_in) {
	// Validate length
	if (file_size_bytes_in / sizeof(u32) < ImageReader::HEAD_WORDS) {
		return GCIF_RE_BAD_HEAD;
	}

	// Validate signature
	u32 sig = getLE(*reinterpret_cast<const u32 *>( file_data_in ));
	if (sig != ImageReader::HEAD_MAGIC) {
		return GCIF_RE_BAD_HEAD;
	}

	return GCIF_RE_OK;
}

extern "C" int gcif_read_memory(const void *file_data_in, long file_size_bytes_in, GCIFImage *image_out) {
	int err;

	// Initialize image data
	image_out->rgba = 0;
	image_out->width = -1;
	image_out->height = -1;

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

		case GCIF_RE_CM_CODES:	// CM codelen read failed
			return "Corrupted:GCIF_RE_CM_CODES";

		case GCIF_RE_BAD_HASH:	// Image hash does not match
			return "Corrupted:GCIF_RE_BAD_HASH";

		default:
			break;
	}

	return "(Unknown error code)";
}

