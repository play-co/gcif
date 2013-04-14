#ifndef IMAGE_READER_HPP
#define IMAGE_READER_HPP

#include "Platform.hpp"
#include "HuffmanDecoder.hpp"
#include "HotRodHash.hpp"
#include "MappedFile.hpp"

namespace cat {


struct ImageHeader {
	u16 width, height; // pixels

	u32 headHash; // Hash of head words
	u32 dataHash; // Hash of data words
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

	CAT_INLINE int getTotalDataWords() {
		return _wordCount;
	}

	CAT_INLINE int getWordsLeft() {
		return _wordsLeft;
	}

	// Initialize with file or memory buffer
	int init(const char *path);
	int init(const void *buffer, int bytes);

	CAT_INLINE ImageHeader *getImageHeader() {
		return &_header;
	}

	// Returns at least minBits in the high bits, supporting up to 32 bits
	CAT_INLINE u32 peek(int minBits) {
		if CAT_UNLIKELY(_bitsLeft < minBits) {
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
		_bitsLeft = 0;
		_bits = 0;
		return bits;
	}

	// Efficient static Huffman symbol decoding
	u32 nextHuffmanSymbol(HuffmanDecoder *dec);

	// No bits left to read?
	CAT_INLINE bool eof() {
		return _eof;
	}

	static const u32 HEAD_WORDS = 4;
	static const u32 HEAD_MAGIC = 0x46494347;
	static const u32 HEAD_SEED = 0x120CA71D;
	static const u32 DATA_SEED = 0xCA71D123;

	CAT_INLINE bool finalizeCheckHash() {
		return _header.dataHash == _hash.final(_wordCount);
	}
};

} // namespace cat

#endif // IMAGE_READER_HPP

