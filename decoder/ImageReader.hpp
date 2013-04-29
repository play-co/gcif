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

#ifndef IMAGE_READER_HPP
#define IMAGE_READER_HPP

#include "Platform.hpp"
#include "HotRodHash.hpp"
#include "MappedFile.hpp"
#include "Enforcer.hpp"

namespace cat {


struct ImageHeader {
	u16 width, height; // pixels

	u32 headHash;	// Hash of head words
	u32 fastHash;	// Fast hash of data words (used during normal decoding)
	u32 goodHash;	// Good hash of data words (used during verification mode)
};


//// ImageReader

class ImageReader {
	MappedFile _file;
	MappedView _fileView;

	ImageHeader _header;

	HotRodHash _hash;

	bool _eof;

	const u32 *_words;
	int _wordCount;
	int _wordsLeft;

	u64 _bits;
	int _bitsLeft;

	void clear();

	u32 refill();

public:
	ImageReader() {
		_words = 0;
	}
	virtual ~ImageReader() {
	}

	CAT_INLINE int getTotalDataWords() {
		return _wordCount;
	}

	CAT_INLINE int getWordsLeft() {
		return _wordsLeft;
	}

	// Initialize with file or memory buffer
	int init(const char *path);
	int init(const void *buffer, long bytes);

	CAT_INLINE ImageHeader *getImageHeader() {
		return &_header;
	}

	// Returns at least minBits in the high bits, supporting up to 32 bits
	CAT_INLINE u32 peek(int minBits) {
		if (_bitsLeft < minBits) {
			return refill();
		} else {
			return (u32)(_bits >> 32);
		}
	}

	// After peeking, consume up to 32 bits
	CAT_INLINE void eat(int len) {
		CAT_DEBUG_ENFORCE(len <= 32);

		_bits <<= len;
		_bitsLeft -= len;
	}

	// Read up to 31 bits
	CAT_INLINE u32 readBits(int len) {
		CAT_DEBUG_ENFORCE(len >= 1 && len <= 31);

		const u32 bits = peek(len);
		eat(len);
		return bits >> (32 - len);
	}

	// Read one bit
	CAT_INLINE u32 readBit() {
		return readBits(1);
	}

	// Read 32 bits
	CAT_INLINE u32 readWord() {
		const u32 bits = peek(32);
		eat(32);
		return bits;
	}

	/*
	 * 335-encoding
	 *
	 * Used for encoding runs of zeroes in the Huffman table compressor
	 */
	CAT_INLINE u32 read335() {
		u32 run = readBits(3), s;

		// If another 3 bits are expected,
		if (run == 7) {
			s = readBits(3);
			run += s;

			// If the remaining data is in 7 bit chunks,
			if (s == 7) {
				do {
					s = readBits(5);
					run += s;
				} while (s == 31); // HuffmanDecoder emits 0 on EOF
			}
		}

		return run;
	}

	/*
	 * 255255-encoding
	 *
	 * Used for encoding runs of zeroes in the entropy encoder
	 */
	CAT_INLINE u32 read255255() {
		u32 run = readBits(8), s;

		// If another byte is expected,
		if (run == 255) {
			s = readBits(8);
			run += s;

			// If the remaining data is in 16-bit words,
			if (s == 255) {
				do {
					s = readBits(16);
					run += s;
				} while (s == 65535); // HuffmanDecoder emits 0 on EOF
			}
		}

		return run;
	}

	/*
	 * 17-encoding
	 *
	 * Used for encoding Huffman codelens with values 0..16, where 16 is rare
	 */
	CAT_INLINE u32 read17() {
		u32 word = readBits(4);

		if (word >= 15) {
			word += readBit();
		}

		return word;
	}

	/*
	 * 9-encoding
	 *
	 * Used for encoding word data that tends to be small but can be bigger
	 */
	CAT_INLINE u32 read9() {
		u32 code = readBits(9);
		if (code > 255) {
			u32 word = code & 255;

			code = readBits(9);
			if (code > 255) {
				word = (word << 8) | (code & 255);

				code = readBits(9);
				if (code > 255) {
					word = (word << 8) | (code & 255);

					return (word << 8) | readBits(8);
				}

				return (word << 8) | code;
			}

			return (word << 8) | code;
		}

		return code;
	}

	// No bits left to read?
	CAT_INLINE bool eof() {
		return _eof;
	}

	static const u32 HEAD_WORDS = 5;
	static const u32 HEAD_MAGIC = 0x46494347;
	static const u32 HEAD_SEED = 0x120CA71D;
	static const u32 DATA_SEED = 0xCA71D123;

	CAT_INLINE bool finalizeCheckHash() {
		return _header.fastHash == _hash.final(_wordCount);
	}
};

} // namespace cat

#endif // IMAGE_READER_HPP

