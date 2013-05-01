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

#include "../decoder/Platform.hpp"
#include "../decoder/ImageReader.hpp"
#include "../decoder/EndianNeutral.hpp"
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

	void init();

	CAT_INLINE void push(u32 x) {
		// Grow ropes
		if CAT_UNLIKELY(_used >= _allocated) {
			grow();
		}

		// Make data endian-neutral and write it out
		_work[_used++] = getLE(x);
		++_size;
	}

	CAT_INLINE int getWordCount() {
		return _size;
	}

	void write(u32 *target);
};


//// ImageWriter

class ImageWriter {
public:
	static const u32 HEAD_MAGIC = ImageReader::HEAD_MAGIC;
	static const u32 MAX_WIDTH_BITS = ImageReader::MAX_WIDTH_BITS;
	static const u32 MAX_WIDTH = ImageReader::MAX_WIDTH;
	static const u32 MAX_HEIGHT_BITS = ImageReader::MAX_HEIGHT_BITS;
	static const u32 MAX_HEIGHT = ImageReader::MAX_HEIGHT;

protected:
	ImageReader::Header _header;

	WriteVector _words;
	u64 _work;
	int _bits;

public:
	CAT_INLINE ImageWriter() {
	}
	CAT_INLINE virtual ~ImageWriter() {
	}

	CAT_INLINE ImageReader::Header *getHeader() {
		return &_header;
	}

	static const char *ErrorString(int err);

	int init(int width, int height);

	// Only works with len in [1..32], and code must not have dirty high bits
	void writeBits(u32 code, int len);

	// Only works for 1-bit code, and code must not have dirty high bits
	CAT_INLINE void writeBit(u32 code) {
		return writeBits(code, 1);
	}

	// Write a whole 32-bit word at once
	CAT_INLINE void writeWord(u32 word) {
		return writeBits(word, 32);
	}

	/*
	 * 335-encoding
	 *
	 * Used for encoding runs of zeroes in the Huffman table compressor
	 */
	static CAT_INLINE int simulate335(u32 run) {
		int bits = 0;

		if (run >= 7) {
			run -= 7;
			bits += 3;
			if (run >= 7) {
				run -= 7;
				bits += 3;
				while (run >= 31) {
					run -= 31;
					bits += 5;
				}
				bits += 5;
			} else {
				bits += 3;
			}
		} else {
			bits += 3;
		}

		return bits;
	}

	CAT_INLINE int write335(u32 run) {
		int bits = 0;

		if (run >= 7) {
			writeBits(7, 3);
			run -= 7;
			bits += 3;
			if (run >= 7) {
				writeBits(7, 3);
				run -= 7;
				bits += 3;
				while (run >= 31) {
					writeBits(31, 5);
					run -= 31;
					bits += 5;
				}
				writeBits(run, 5);
				bits += 5;
			} else {
				writeBits(run, 3);
				bits += 3;
			}
		} else {
			writeBits(run, 3);
			bits += 3;
		}

		return bits;
	}

	/*
	 * 255255-encoding
	 *
	 * Used for representing runs of zeroes in the entropy encoder
	 */
	static CAT_INLINE int simulate255255(u32 run) {
		int bits = 0;

		// If multiple FF bytes will be emitted,
		if (run >= 255 + 255) {
			// Step it up to 16-bit words
			run -= 255 + 255;
			bits += 8 + 8;
			while (run >= 65535) {
				bits += 16;
				run -= 65535;
			}
			bits += 16;
		} else {
			// Write out FF bytes
			if (run >= 255) {
				bits += 8;
				run -= 255;
			}

			// Write out last byte
			bits += 8;
		}

		return bits;
	}

	CAT_INLINE int write255255(u32 run) {
		int bits = 0;

		// If multiple FF bytes will be emitted,
		if (run >= 255 + 255) {
			writeBits(255, 8);
			writeBits(255, 8);

			// Step it up to 16-bit words
			run -= 255 + 255;
			bits += 8 + 8;
			while (run >= 65535) {
				writeBits(65535, 16);
				bits += 16;
				run -= 65535;
			}
			writeBits(run, 16);
			bits += 16;
		} else {
			// Write out FF bytes
			if (run >= 255) {
				writeBits(255, 8);
				bits += 8;
				run -= 255;
			}

			// Write out last byte
			writeBits(run, 8);
			bits += 8;
		}

		return bits;
	}

	/*
	 * 17-encoding
	 *
	 * Used for encoding Huffman codelens with values 0..16, where 16 is rare
	 */
	static CAT_INLINE int simulate17(u32 len) {
		CAT_DEBUG_ENFORCE(len < 17);

		int bits = 4;

		if (len >= 15) {
			bits++;
		}

		return bits;
	}

	CAT_INLINE int write17(u32 len) {
		CAT_DEBUG_ENFORCE(len < 17);

		int bits = 4;

		if (len >= 15) {
			writeBits(15, 4);
			writeBit(len - 15);
			bits++;
		} else {
			writeBits(len, 4);
		}

		return bits;
	}

	/*
	 * 9-encoding
	 *
	 * Used for encoding word data that tends to be small but can be bigger
	 */
	CAT_INLINE int write9(u32 word) {
		if (word > 0x00ffffff) {
			writeBits((word >> 24) | 256, 9);
			writeBits(((word >> 16) & 255) | 256, 9);
			writeBits(((word >> 8) & 255) | 256, 9);
			writeBits(word & 255, 8);
			return 35;
		} else if (word > 0x0000ffff) {
			writeBits((word >> 16) | 256, 9);
			writeBits(((word >> 8) & 255) | 256, 9);
			writeBits(word & 255, 9);
			return 27;
		} else if (word > 0x000000ff) {
			writeBits((word >> 8) | 256, 9);
			writeBits(word & 255, 9);
			return 18;
		} else {
			writeBits(word, 9);
			return 9;
		}
	}

	// Finalize the last word and report length of file in bytes
	u32 finalize();

	// Write finalized data to file
	int write(const char *path);
};


} // namespace cat

#endif // IMAGE_WRITER_HPP

