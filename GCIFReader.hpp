#ifndef GCIF_READER_HPP
#define GCIF_READER_HPP


// When API functions return an int, it's all about this:
enum ReaderErrors {
	RE_OK,			// No error

	RE_FILE,		// File access error
	RE_BAD_HEAD,	// File header is bad
	RE_BAD_DIMS,	// Bad image dimensions
	RE_BAD_DATA,	// File data is bad

	RE_MASK_CODES,	// Mask codelen read failed
	RE_MASK_DECI,	// Mask decode init failed
	RE_MASK_LZ,		// Mask LZ decode failed

	RE_LZ_CODES,	// LZ codelen read failed
	RE_LZ_BAD,		// Bad data in LZ section

	RE_LP_CODES,	// LP codelen read failed
	RE_LP_BAD,		// Bad data in LP section

	RE_CM_CODES		// CM codelen read failed
};


struct GCIFImage {
	unsigned char *rgba;	// RGBA pixels
	int width, height;		// Number of pixels
};


int gcif_read(const char *input_file_path, GCIFImage *image);

const char *gcif_read_errstr(int err);


#endif // GCIF_READER_HPP

