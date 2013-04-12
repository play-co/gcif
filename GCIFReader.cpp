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

	// 2D-LZ Exact Match
	ImageLZReader imageLZReader;
	if ((err = imageLZReader.read(reader))) {
		return err;
	}

	imageLZReader.dumpStats();

	// Fully-Transparent Alpha Mask
	ImageMaskReader imageMaskReader;
	if ((err = imageMaskReader.read(reader))) {
		return err;
	}

	imageMaskReader.dumpStats();

	// Context Modeling Decompression
	ImageCMReader imageCMReader;
	if ((err = imageCMReader.read(reader, imageMaskReader, imageLZReader, image))) {
		return err;
	}

	imageCMReader.dumpStats();

	// Verify hash
	if (!reader.finalizeCheckHash()) {
		return 1000;
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

		default:
			break;
	}

	return "Unknown error code";
}

