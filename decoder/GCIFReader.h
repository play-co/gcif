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

#ifndef GCIF_READER_H
#define GCIF_READER_H

#ifdef __cplusplus
extern "C" {
#endif


// When API functions return an int, it's all about this:
enum GCIFReaderErrors {
	GCIF_RE_OK,			// No error

	GCIF_RE_FILE,		// File access error
	GCIF_RE_BAD_HEAD,	// File header is bad
	GCIF_RE_BAD_DIMS,	// Bad image dimensions
	GCIF_RE_BAD_DATA,	// File data is bad

	GCIF_RE_MASK_CODES,	// Mask codelen read failed
	GCIF_RE_MASK_DECI,	// Mask decode init failed
	GCIF_RE_MASK_LZ,	// Mask LZ decode failed

	GCIF_RE_LZ_CODES,	// LZ codelen read failed
	GCIF_RE_LZ_BAD,		// Bad data in LZ section

	GCIF_RE_CM_CODES,	// CM codelen read failed

	GCIF_RE_BAD_HASH,	// Image hash does not match
};

// Returns a string representation of the above error codes
const char *gcif_read_errstr(int err);


// Return data
typedef struct _GCIFImage {
	unsigned char *rgba;	// RGBA pixels; free with "delete []rgba" when done
	int width, height;		// Dimensions in pixels
} GCIFImage;


// Compiled optionally
#ifdef CAT_COMPILE_MMAP

/*
	gcif_read_file

	Read from the given file path using blocking memory-mapped file I/O.

	This function is thread-safe, meaning you can read two images simultaneously.

	On success it returns GCIF_RE_OK.  Otherwise it returns a failure code from
	the table above.  A string version of the failure code can be retrieved by
	calling gcif_read_errstr().

	On failure, the GCIFImage output can be safely ignored.
	On success, you are responsible for freeing the image memory by calling
	the gcif_free_image() function below.
*/
int gcif_read_file(const char *input_file_path_in, GCIFImage *image_out);

#endif // CAT_COMPILE_MMAP


/*
	gcif_read_memory

	Read the image from the given memory buffer.

	See: gcif_read_file() above for more important usage information.
*/
int gcif_read_memory(const void *file_data_in, long file_size_bytes_in, GCIFImage *image_out);

/*
 * Returns GCIF_RE_OK if the data is for a GCIF file.
 */
int gcif_sig_cmp(const void *file_data_in, long file_size_bytes_in);

/*
	gcif_free_image

	Frees memory associated with the given image file.

	This is safe to call when the read function fails.
*/
void gcif_free_image(const void *rgba);


#ifdef __cplusplus
};
#endif


#endif // GCIF_READER_H

