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

#include "ImageReader.hpp"
#include "EndianNeutral.hpp"
#include "GCIFReader.h"
using namespace cat;


//// ImageReader

void ImageReader::clear() {
	_words = 0;
}

u32 ImageReader::refill() {
	u64 bits = _bits;
	int bitsLeft = _bitsLeft;

	CAT_DEBUG_ENFORCE(bitsLeft < 32);

	if (_wordsLeft > 0) {
		--_wordsLeft;

		u32 nextWord = getLE(*_words++);

		bits |= (u64)nextWord << (32 - bitsLeft);
		bitsLeft += 32;

		_bits = bits;
		_bitsLeft = bitsLeft;
	} else {
		if (bitsLeft <= 0) {
			_eof = true;
		}
	}

	return bits >> 32;
}

#ifdef CAT_COMPILE_MMAP

int ImageReader::init(const char * CAT_RESTRICT path) {

	// Map file for reading

	if CAT_UNLIKELY(!_file.OpenRead(path)) {
		return GCIF_RE_FILE;
	}

	if CAT_UNLIKELY(!_fileView.Open(&_file)) {
		return GCIF_RE_FILE;
	}

	u8 * CAT_RESTRICT fileData = _fileView.MapView();
	if CAT_UNLIKELY(!fileData) {
		return GCIF_RE_FILE;
	}

	// Run from memory

	return init(fileData, _fileView.GetLength());
}

#endif // CAT_COMPILE_MMAP

int ImageReader::init(const void * CAT_RESTRICT buffer, long fileSize) {
	const int MIN_FILE_WORDS = 2; // Enough for header

	clear();

	const u32 * CAT_RESTRICT words = reinterpret_cast<const u32 *>( buffer );
	const u32 fileWords = fileSize / sizeof(u32);

	// Validate file length
	if CAT_UNLIKELY(fileWords < MIN_FILE_WORDS) {
		return GCIF_RE_BAD_HEAD;
	}

	// Setup bit reader
	_words = words;
	_wordsLeft = fileWords;
	_wordCount = _wordsLeft;

	_eof = false;

	_bits = 0;
	_bitsLeft = 0;

	// Validate magic
	u32 magic = readWord();
	if CAT_UNLIKELY(magic != HEAD_MAGIC) {
		return GCIF_RE_BAD_HEAD;
	}

	// Any input data here is OK
	_header.xsize = readBits(MAX_X_BITS);
	_header.ysize = readBits(MAX_Y_BITS);

	return GCIF_RE_OK;
}

