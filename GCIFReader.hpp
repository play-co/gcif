/*
	Copyright (c) 2013 Christopher A. Taylor.  All rights reserved.

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

	RE_CM_CODES,	// CM codelen read failed

	RE_BAD_HASH,	// Image hash does not match
};


struct GCIFImage {
	unsigned char *rgba;	// RGBA pixels
	int width, height;		// Number of pixels
};


int gcif_read(const char *input_file_path, GCIFImage *image);

const char *gcif_read_errstr(int err);


#endif // GCIF_READER_HPP

