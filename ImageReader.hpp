#ifndef IMAGE_READER_HPP
#define IMAGE_READER_HPP

#include "Platform.hpp"
#include "HuffmanDecoder.hpp"
#include "MurmurHash3.hpp"

namespace cat {


/*
 * Image file info
 *
 * This is the parsed, not raw, data
 */
struct ImageInfo {
	u16 width, height; // pixels

	u32 headHash; // MurmurHash3 of head words
	u32 dataHash; // MurmurHash3 of data words
};


enum ReaderErrors {
	RE_OK,		// No problemo

	RE_FILE,	// File access error

	RE_COUNT
};


//// ImageReader

class ImageReader {
	MappedFile _file;

	ImageHeader _header;

	MurmurHash3 _hash;

	bool _eof;

	u32 *_words;
	int _wordsLeft;

	u32 _bits;
	int _bitsLeft;

	u32 _nextWord;
	int _nextLeft;

	void clear();

	u32 refill();

public:
	ImageReader() {
		_words = 0;
	}
	virtual ~ImageReader() {
	}

	// Initialize with file or memory buffer
	int init(const char *path);
	int init(const void *buffer, int bytes);

	CAT_INLINE ImageHeader *getHeader() {
		return &_header;
	}

	// Returns at least minBits in the high bits, supporting up to 32 bits
	CAT_INLINE u32 peek(int minBits) {
		if (_bitsLeft < minBits) {
			return refill();
		} else {
			return _bits;
		}
	}

	// After peeking, consume up to 31 bits
	CAT_INLINE void eat(int len) {
		_bits <<= len;
		_bitsLeft -= len;
	}

	// Read up to 31 bits
	CAT_INLINE u32 readBits(int len) {
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
		_bits = 0;
		_bitsLeft = 0;
		return bits;
	}

	// Efficient static Huffman symbol decoding
	u32 nextHuffmanSymbol(huffman::decoder_tables *tables);

	// No bits left to read?
	CAT_INLINE bool isEOF() {
		return _eof;
	}

	static const u32 HEAD_WORDS = 4;
	static const u32 HEAD_MAGIC = 0x46494347;
	static const u32 HEAD_SEED = 0x120CA71D;
	static const u32 DATA_SEED = 0xCA71D123;
};

} // namespace cat

#endif // IMAGE_READER_HPP

