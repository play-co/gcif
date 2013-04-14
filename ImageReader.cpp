#include "ImageReader.hpp"
#include "EndianNeutral.hpp"
#include "GCIFReader.hpp"
using namespace cat;


//// ImageReader

void ImageReader::clear() {
	_words = 0;
}

u32 ImageReader::refill() {
	u32 bits = _bits;
	int bitsLeft = _bitsLeft;

	u32 nextWord = _nextWord;
	int nextLeft = _nextLeft;

	bits |= nextWord >> bitsLeft;

	int readBits = 32 - bitsLeft;

	if CAT_LIKELY(nextLeft >= readBits) {
		nextWord <<= readBits;
		nextLeft -= readBits;
		_bitsLeft = 32;
	} else {
		if CAT_LIKELY(_wordsLeft > 0) {
			--_wordsLeft;

			nextWord = getLE(*_words++);
			_hash.hashWord(nextWord);

			bitsLeft += nextLeft;
			bits |= nextWord >> bitsLeft;

			if (bitsLeft == 0) {
				nextWord = 0;
			} else {
				nextWord <<= (32 - bitsLeft);
			}
			nextLeft = bitsLeft;
			_bitsLeft = 32;
		} else {
			nextWord = 0;
			nextLeft = 0;

			if (bitsLeft <= 0) {
				_eof = true;
			}
		}
	}

	_nextWord = nextWord;
	_nextLeft = nextLeft;

	_bits = bits;

	return bits;
}

int ImageReader::init(const char *path) {

	// Map file for reading

	if CAT_UNLIKELY(!_file.OpenRead(path)) {
		return RE_FILE;
	}

	if CAT_UNLIKELY(!_fileView.Open(&_file)) {
		return RE_FILE;
	}

	u8 *fileData = _fileView.MapView();
	if CAT_UNLIKELY(!fileData) {
		return RE_FILE;
	}

	// Run from memory

	return init(fileData, _fileView.GetLength());
}

int ImageReader::init(const void *buffer, int fileSize) {
	clear();

	const u32 *words = reinterpret_cast<const u32 *>( buffer );
	const u32 fileWords = fileSize / sizeof(u32);

	// Validate header

	HotRodHash hh;
	hh.init(HEAD_SEED);

	if CAT_UNLIKELY(fileWords < HEAD_WORDS) {
		return RE_BAD_HEAD;
	}

	u32 word0 = getLE(words[0]);
	hh.hashWord(word0);

	if CAT_UNLIKELY(HEAD_MAGIC != word0) {
		return RE_BAD_HEAD;
	}

	u32 word1 = getLE(words[1]);
	hh.hashWord(word1);

	u32 dataHash = getLE(words[2]);
	hh.hashWord(dataHash);

	u32 headHash = getLE(words[3]);
	if CAT_UNLIKELY(headHash != hh.final(HEAD_WORDS)) {
		return RE_BAD_HEAD;
	}

	// Read header

	_header.width = word1 >> 16;
	_header.height = (u16)word1;
	_header.headHash = headHash;
	_header.dataHash = dataHash;

	// Get ready to read words

	_hash.init(DATA_SEED);

	_words = words + HEAD_WORDS;
	_wordsLeft = fileWords - HEAD_WORDS;
	_wordCount = _wordsLeft;

	_eof = false;

	_bits = 0;
	_bitsLeft = 0;

	_nextWord = 0;
	_nextLeft = 0;

	return RE_OK;
}

u32 ImageReader::nextHuffmanSymbol(HuffmanDecoder *dec) {
	u32 code = peek(16);

	u32 len;
	u32 sym = dec->get(code, len);

	eat(len);

	return sym;
}

