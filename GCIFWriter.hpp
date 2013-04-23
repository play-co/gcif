#ifndef GCIF_WRITER_HPP
#define GCIF_WRITER_HPP


// When API functions return an int, it's all about this:
enum WriteErrors {
	WE_OK,			// No problemo

	WE_BAD_PARAMS,	// Bad parameters passed to gcif_write
	WE_BAD_DIMS,	// Image dimensions are invalid
	WE_FILE,		// Unable to access file
	WE_BUG			// Internal error
};

/*
 * gcif_write()
 *
 * rgba: Pass in the 8-bit/channel RGBA raster data in OpenGL RGBA8888 format, row-first, stride = width
 * width: Pixels per row
 * height: Pixels per column
 * output_file_path: File write location
 * compression_level:
 * 		0 = Faster: Turns off entropy calculation and custom linear filter taps
 * 		1 = Better: Sets filter select fuzz to 8
 * 		2 = Harder: Sets filter select fuzz to 20
 */
int gcif_write(const void *rgba, int width, int height, const char *output_file_path, int compression_level);

// Returns an error string for a return value from gcif_write()
const char *gcif_write_errstr(int err);


// Extra twiddly knobs to eke out better performance if you prefer
struct GCIFKnobs {
	// Seed used for any randomized selections, may help discover improvements
	int bump;

	// 20 = normal, note that high values actually hurt compression
	int filterSelectFuzz;

	// Toggle custom filter taps
	bool customFilterTaps;
};

/*
 * Same as gcif_write() except the compression level is replaced with the
 * knobs structure which gives you full control over the available options
 * controlling how the compressor works.
 */
int gcif_write_ex(const void *rgba, int width, int height, const char *output_file_path, const GCIFKnobs *knobs);


#endif // GCIF_WRITER_HPP

