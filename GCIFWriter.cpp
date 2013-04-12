#include "GCIFWriter.hpp"
#include "ImageWriter.hpp"
#include "ImageMaskWriter.hpp"
#include "ImageLZWriter.hpp"
#include "ImageCMWriter.hpp"
using namespace cat;


int gcif_write(const void *rgba, int width, int height, const char *output_file_path) {
	const u8 *image = reinterpret_cast<const u8*>( rgba );

	int err;

	// Initialize image writer
	ImageWriter writer;
	if ((err = writer.init(width, height))) {
		return err;
	}

	// Fully-Transparent Alpha Mask
	ImageMaskWriter imageMaskWriter;
	if ((err = imageMaskWriter.initFromRGBA(image, width, height))) {
		return err;
	}

	imageMaskWriter.write(writer);
	imageMaskWriter.dumpStats();

	// 2D-LZ Exact Match
	ImageLZWriter imageLZWriter;
	if ((err = imageLZWriter.initFromRGBA(image, width, height))) {
		return err;
	}

	imageLZWriter.write(writer);
	imageLZWriter.dumpStats();

	// Context Modeling Decompression
	ImageCMWriter imageCMWriter;
	if ((err = imageCMWriter.initFromRGBA(image, width, height, imageMaskWriter, imageLZWriter))) {
		return err;
	}

	imageCMWriter.write(writer);
	imageCMWriter.dumpStats();

	// Finalize file
	if ((err = writer.finalizeAndWrite(output_file_path))) {
		return err;
	}
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

