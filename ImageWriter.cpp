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

#include "ImageWriter.hpp"
#include "EndianNeutral.hpp"
#include "MappedFile.hpp"
#include "GCIFWriter.hpp"
using namespace cat;


//// WriteVector

void WriteVector::clear() {
	int words = HEAD_SIZE;
	u32 *ptr = _head;

	// For each rope,
	while (ptr) {
		u32 *nextPtr = *reinterpret_cast<u32**>( ptr + words );

		delete []ptr;

		ptr = nextPtr;
		words <<= 1;
	}

	_head = 0;
	_work = 0;
}

void WriteVector::grow() {
	const int newAllocated = _allocated << 1;

	// If initializing,
	u32 *newWork = new u32[newAllocated + PTR_WORDS];

	// Point current "next" pointer to new workspace
	*reinterpret_cast<u32**>( _work + _allocated ) = newWork;

	// Set "next" pointer to null
	*reinterpret_cast<u32**>( newWork + newAllocated ) = 0;

	// Update class state
	_work = newWork;
	_allocated = newAllocated;
	_used = 0;
}

void WriteVector::init(u32 hashSeed) {
	clear();

	_fastHash.init(hashSeed);
	_goodHash.init(hashSeed);

	u32 *newWork = new u32[HEAD_SIZE + PTR_WORDS];
	_head = _work = newWork;

	_used = 0;
	_allocated = HEAD_SIZE;
	_size = 0;

	// Set "next" pointer to null
	*reinterpret_cast<u32**>( newWork + HEAD_SIZE ) = 0;
}

void WriteVector::write(u32 *target) {
	u32 *ptr = _head;

	// If any data to write at all,
	if (ptr) {
		int words = HEAD_SIZE;
		u32 *nextPtr = *reinterpret_cast<u32**>( ptr + words );

		// For each full rope,
		while (nextPtr) {
			memcpy(target, ptr, words * WORD_BYTES);
			target += words;

			ptr = nextPtr;
			words <<= 1;

			nextPtr = *reinterpret_cast<u32**>( ptr + words );
		}

		// Write final partial rope
		memcpy(target, ptr, _used * WORD_BYTES);
	}
}


//// ImageWriter

int ImageWriter::init(int width, int height) {
	// Validate

	if (width < 0 || height < 0) {
		return WE_BAD_DIMS;
	}

	if ((width % 8) || (height % 8)) {
		return WE_BAD_DIMS;
	}

	if (width > 65535 || height > 65535) {
		return WE_BAD_DIMS;
	}

	// Initialize

	_words.init(ImageReader::DATA_SEED);

	_header.width = static_cast<u16>( width );
	_header.height = static_cast<u16>( height );

	_work = 0;
	_bits = 0;

	return WE_OK;
}

void ImageWriter::writeBitPush(u32 code) {
	const u32 pushWord = _work | code;

	_words.push(pushWord);

	_work = 0;
	_bits = 0;
}

void ImageWriter::writeBitsPush(u32 code, int len, int available) {
	const int shift = len - available;

	CAT_DEBUG_ENFORCE(shift < 32);

	const u32 pushWord = _work | (code >> shift);
	_words.push(pushWord);

	if (shift) {
		_work = code << (32 - shift);
	} else {
		_work = 0;
	}

	_bits = shift;
}

int ImageWriter::finalizeAndWrite(const char *path) {
	MappedFile file;

	// Finalize the bit data

	if (_bits) {
		_words.push(_work);
	}

	u32 fastHash, goodHash;
	_words.finalizeHash(fastHash, goodHash);

	// Calculate file size
	int wordCount = _words.getWordCount();
	int totalBytes = (ImageReader::HEAD_WORDS + wordCount) * sizeof(u32);

	// Map the file

	if (!file.OpenWrite(path, totalBytes)) {
		return WE_FILE;
	}

	MappedView fileView;

	if (!fileView.Open(&file)) {
		return WE_FILE;
	}

	u8 *fileData = fileView.MapView();

	if (!fileData) {
		return WE_FILE;
	}

	u32 *fileWords = reinterpret_cast<u32*>( fileData );

	// Write header

	FileValidationHash hh;
	hh.init(ImageReader::HEAD_SEED);

	fileWords[0] = getLE(ImageReader::HEAD_MAGIC);
	hh.hashWord(ImageReader::HEAD_MAGIC);

	u32 header1 = (_header.width << 16) | _header.height;
	fileWords[1] = getLE(header1);
	hh.hashWord(header1);

	fileWords[2] = getLE(fastHash);
	hh.hashWord(fastHash);

	fileWords[3] = getLE(goodHash);
	hh.hashWord(goodHash);

	u32 headHash = hh.final(ImageReader::HEAD_WORDS);
	fileWords[4] = getLE(headHash);

	fileWords += ImageReader::HEAD_WORDS;

	_header.headHash = headHash;
	_header.fastHash = fastHash;
	_header.goodHash = goodHash;

	// Copy file data

	_words.write(fileWords);

	return WE_OK;
}

