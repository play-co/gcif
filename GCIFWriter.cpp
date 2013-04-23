#include "GCIFWriter.hpp"
#include "ImageWriter.hpp"
#include "ImageMaskWriter.hpp"
#include "ImageLZWriter.hpp"
#include "ImageCMWriter.hpp"
using namespace cat;


// Default knobs for the normal compression levels

static const int COMPRESS_LEVELS = 3;
static const GCIFKnobs DEFAULT_KNOBS[COMPRESS_LEVELS] = {
	{ 0, 0, false },
	{ 0, 8, true },
	{ 0, 20, true }
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

