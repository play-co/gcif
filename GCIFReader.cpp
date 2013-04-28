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

#include "GCIFReader.hpp"
#include "ImageReader.hpp"
#include "ImageMaskReader.hpp"
#include "ImageLZReader.hpp"
#include "ImageCMReader.hpp"
using namespace cat;


int gcif_read(const char *input_file_path, GCIFImage *image) {
	int err;

	// Initialize image reader
	ImageReader reader;
	if ((err = reader.init(input_file_path))) {
		return err;
	}

	// Fill in image width and height
	ImageHeader *header = reader.getImageHeader();
	image->width = header->width;
	image->height = header->height;

	const int MASK_COUNT = ImageCMReader::MASK_COUNT;
	ImageMaskReader maskReaders[MASK_COUNT];
	for (int ii = 0; ii < MASK_COUNT; ++ii) {
		if ((err = maskReaders[ii].read(reader))) {
			return err;
		}
		maskReaders[ii].dumpStats();
	}

	// 2D-LZ Exact Match
	ImageLZReader imageLZReader;
	if ((err = imageLZReader.read(reader))) {
		return err;
	}
	imageLZReader.dumpStats();

	// Context Modeling Decompression
	ImageCMReader imageCMReader;
	if ((err = imageCMReader.read(reader, maskReaders, imageLZReader, image))) {
		return err;
	}
	imageCMReader.dumpStats();

	// Verify hash
	if (!reader.finalizeCheckHash()) {
		return RE_BAD_HASH;
	}

	return 0;
}


const char *gcif_read_errstr(int err) {
	switch (err) {
		case RE_OK:			// No problemo
			return "No problemo";

		case RE_FILE:		// File access error
			return "File access error";
		case RE_BAD_HEAD:	// File header is bad
			return "File header is bad";
		case RE_BAD_DATA:	// File data is bad
			return "File data is bad";
		case RE_BAD_DIMS:	// Bad image dimensions
			return "Bad image dimensions";

		case RE_MASK_CODES:	// Mask codelen read failed
			return "Mask codelen read failed";
		case RE_MASK_DECI:	// Mask decode init failed
			return "Mask decode init failed";
		case RE_MASK_LZ:	// Mask LZ decode failed
			return "Mask LZ decode failed";

		case RE_LZ_CODES:	// LZ codelen read failed
			return "LZ codelen read failed";
		case RE_LZ_BAD:		// Bad data in LZ section
			return "Bad data in LZ section";

		case RE_CM_CODES:	// CM codelen read failed
			return "CM codelen read failed";

		case RE_BAD_HASH:	// Image hash does not match
			return "Image hash does not match";

		default:
			break;
	}

	return "Unknown error code";
}

