#ifndef GCIF_WRITER_HPP
#define GCIF_WRITER_HPP


// When API functions return an int, it's all about this:
enum WriteErrors {
	WE_OK,			// No problemo

	WE_BAD_DIMS,	// Image dimensions are invalid
	WE_FILE,		// Unable to access file
	WE_BUG			// Internal error
};


int gcif_write(const void *rgba, int width, int height, const char *output_file_path);

const char *gcif_write_errstr(int err);


#endif // GCIF_WRITER_HPP

