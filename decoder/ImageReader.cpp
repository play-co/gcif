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
		_hash.hashWord(nextWord);

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

int ImageReader::init(const char *path) {

	// Map file for reading

	if CAT_UNLIKELY(!_file.OpenRead(path)) {
		return GCIF_RE_FILE;
	}

	if CAT_UNLIKELY(!_fileView.Open(&_file)) {
		return GCIF_RE_FILE;
	}

	u8 *fileData = _fileView.MapView();
	if CAT_UNLIKELY(!fileData) {
		return GCIF_RE_FILE;
	}

	// Run from memory

	return init(fileData, _fileView.GetLength());
}

int ImageReader::init(const void *buffer, long fileSize) {
	clear();

	const u32 *words = reinterpret_cast<const u32 *>( buffer );
	const u32 fileWords = fileSize / sizeof(u32);

	// Validate header

	FileValidationHash hh;
	hh.init(HEAD_SEED);

	if CAT_UNLIKELY(fileWords < HEAD_WORDS) {
		return GCIF_RE_BAD_HEAD;
	}

	u32 word0 = getLE(words[0]);
	hh.hashWord(word0);

	if CAT_UNLIKELY(HEAD_MAGIC != word0) {
		return GCIF_RE_BAD_HEAD;
	}

	u32 word1 = getLE(words[1]);
	hh.hashWord(word1);

	u32 fastHash = getLE(words[2]);
	hh.hashWord(fastHash);

	u32 goodHash = getLE(words[3]);
	hh.hashWord(goodHash);

	u32 headHash = getLE(words[4]);
	if CAT_UNLIKELY(headHash != hh.final(HEAD_WORDS)) {
		CAT_DEBUG_EXCEPTION();
		return GCIF_RE_BAD_HEAD;
	}

	// Read header

	_header.width = word1 >> 16;
	_header.height = (u16)word1;
	_header.headHash = headHash;
	_header.fastHash = fastHash;
	_header.goodHash = goodHash;

	// Get ready to read words

	_hash.init(DATA_SEED);

	_words = words + HEAD_WORDS;
	_wordsLeft = fileWords - HEAD_WORDS;
	_wordCount = _wordsLeft;

	_eof = false;

	_bits = 0;
	_bitsLeft = 0;

	return GCIF_RE_OK;
}

