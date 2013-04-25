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

#ifndef IMAGE_WRITER_HPP
#define IMAGE_WRITER_HPP

#include "Platform.hpp"
#include "HotRodHash.hpp"
#include "ImageReader.hpp"
#include "EndianNeutral.hpp"
#include "Log.hpp"

namespace cat {


//// WriteVector

/*
 * Vector optimized for file write access pattern:
 *
 * + Only writes are to append
 * + Final operation is to read it all back out
 *
 * Allocates data in ropes that double in size
 * Each rope ends in a pointer to the next rope
 *
 * Cannot just memory map a file and append to it because mmap files cannot
 * grow at all.  So my solution is this optimal vector representation and
 * then write it all out.  Data is stored internally in little-endian byte
 * order so that it can just be memcpy out to the file.
 *
 * Iunno...  Speeeed!
 */

class WriteVector {
	static const int HEAD_SIZE = 4096; // Words in head chunk, grows from here
	static const int WORD_BYTES = static_cast<int>( sizeof(u32) );
	static const int PTR_BYTES = static_cast<int>( sizeof(u32 *) );
	static const int PTR_WORDS = PTR_BYTES / WORD_BYTES;

	u32 *_head; 	// First rope strand
	u32 *_work; 	// Rope under construction
	int _used;		// Words used in workspace
	int _allocated;	// Words allocated in workspace

	int _size;		// Total number of words

	HotRodHash _fastHash;
	FileValidationHash _goodHash;

	void clear();
	void grow();

public:
	CAT_INLINE WriteVector() {
		_head = _work = 0;
		_used = _allocated = _size = 0;
	}

	CAT_INLINE virtual ~WriteVector() {
		clear();
	}

	void init(u32 hashSeed);

	CAT_INLINE void push(u32 x) {
		// Grow ropes
		if CAT_UNLIKELY(_used >= _allocated) {
			grow();
		}

		// Hash data for later validation
		_fastHash.hashWord(x);
		_goodHash.hashWord(x);

		// Make data endian-neutral and write it out
		_work[_used++] = getLE(x);
		++_size;
	}

	CAT_INLINE void finalizeHash(u32 &fastHash, u32 &goodHash) {
		fastHash = _fastHash.final(_size);
		goodHash = _goodHash.final(_size);
	}

	CAT_INLINE int getWordCount() {
		return _size;
	}

	void write(u32 *target);
};


//// ImageWriter

class ImageWriter {
	ImageHeader _header;

	WriteVector _words;
	u32 _work;	// Word workspace
	int _bits;	// Modulo 32

	void writeBitPush(u32 code);
	void writeBitsPush(u32 code, int len, int available);

public:
	CAT_INLINE ImageWriter() {
	}
	CAT_INLINE virtual ~ImageWriter() {
	}

	CAT_INLINE ImageHeader *getImageHeader() {
		return &_header;
	}

	static const char *ErrorString(int err);

	int init(int width, int height);

	// Only works for 1-bit code, and code must not have dirty high bits
	CAT_INLINE void writeBit(u32 code) {
		CAT_DEBUG_ENFORCE(code < 2);

		const int bits = _bits;
		const int available = 31 - bits;

		if CAT_LIKELY(available > 0) {
			_work |= code << available;

			_bits = bits + 1;
		} else {
			writeBitPush(code);
		}
	}

	// Only works with len in [1..32], and code must not have dirty high bits
	CAT_INLINE void writeBits(u32 code, int len) {
		CAT_DEBUG_ENFORCE(len >= 1 && len <= 32);
		CAT_DEBUG_ENFORCE((code >> len) == 0);

		const int bits = _bits;
		const int available = 32 - bits;

		if CAT_LIKELY(available > len) {
			CAT_DEBUG_ENFORCE((available - len) < 32);

			_work |= code << (available - len);

			_bits = bits + len;
		} else {
			writeBitsPush(code, len, available);
		}
	}

	// Write a whole 32-bit word at once
	CAT_INLINE void writeWord(u32 word) {
		const int shift = _bits;

		CAT_DEBUG_ENFORCE(shift != 32);

		const u32 pushWord = _work | (word >> shift);

		_words.push(pushWord);

		if (shift > 0) {
			_work = word << (32 - shift);
		} else {
			_work = 0;
		}

		_bits = shift;
	}

	int finalizeAndWrite(const char *path);
};


} // namespace cat

#endif // IMAGE_WRITER_HPP

