#include "ImageReader.hpp"
#include "EndianNeutral.hpp"
using namespace cat;


//// ImageReader

void ImageReader::clear() {
	_eof = false;

	_words = 0;
	_wordsLeft = 0;

	_bits = 0;
	_bitsLeft = 0;

	_nextWord = 0;
	_nextLeft = 0;
}

u32 ImageReader::refill() {
	u32 bits = _bits;
	int bitsLeft = _bitsLeft;

	u32 nextWord = _nextWord;
	int nextLeft = _nextLeft;

	bits |= nextWord >> bitsLeft;

	int readBits = 32 - bitsLeft;

	if (nextLeft >= readBits) {
		nextWord <<= readBits;
		nextLeft -= readBits;
	} else {
		if (_wordsLeft > 0) {
			--_wordsLeft;

			nextWord = getLE(*_words++);
			_hash.hashWord(nextWord);

			bitsLeft += nextLeft;
			bits |= nextWord >> bitsLeft;

			nextWord <<= (32 - bitsLeft);
			nextLeft = bitsLeft;
		} else {
			nextWord = 0;
			nextLeft = 32;

			// TODO: Fix this
			if (bitsLeft <= 0) {
				_eof = true;
			}
		}
	}

	_nextWord = nextWord;
	_nextLeft = nextLeft;

	_bitsLeft = 32;
	_bits = bits;

	return bits;
}

const char *ImageReader::ErrorString(int err) {
	switch (err) {
		case RE_OK:			// No problemo
			return "No problemo";
		case RE_FILE:		// File access error
			return "File access error";
		case RE_BAD_HEAD:	// File header is bad
			return "File header is bad";
		case RE_BAD_DATA:	// File data is bad
			return "File data is bad";
		case RE_MASK_INIT:	// Mask init failed
			return "Mask init failed";
		case RE_MASK_CODES:	// Mask codelen read failed
			return "Mask codelen read failed";
		case RE_MASK_DECI:	// Mask decode init failed
			return "Mask decode init failed";
		case RE_MASK_LZ:	// Mask LZ decode failed
			return "Mask LZ decode failed";
		default:
			break;
	}

	return "Unknown error code";
}

int ImageReader::init(const char *path) {

	// Map file for reading

	if (!_file.OpenRead(path)) {
		return RE_FILE;
	}

	if (!_fileView.Open(&_file)) {
		return RE_FILE;
	}

	u8 *fileData = _fileView.MapView();
	if (!fileData) {
		return RE_FILE;
	}

	// Run from memory

	return init(fileData, _fileView.GetLength());
}

int ImageReader::init(const void *buffer, int fileSize) {
	clear();

	const u32 *words = reinterpret_cast<const u32 *>( buffer );
	const u32 fileWords = fileSize / 4;

	// Validate header

	MurmurHash3 hh;
	hh.init(HEAD_SEED);

	if (fileWords < HEAD_WORDS) {
		return RE_BAD_HEAD;
	}

	u32 word0 = getLE(words[0]);
	hh.hashWord(word0);

	if (HEAD_MAGIC != word0) {
		return RE_BAD_HEAD;
	}

	u32 word1 = getLE(words[1]);
	hh.hashWord(word1);

	u32 dataHash = getLE(words[2]);
	hh.hashWord(dataHash);

	u32 headHash = getLE(words[3]);
	if (headHash != hh.final(HEAD_WORDS)) {
		return RE_BAD_HEAD;
	}

	// Read header

	_info.width = word1 >> 16;
	_info.height = word1 & 0xffff;
	_info.headHash = headHash;
	_info.dataHash = dataHash;

	// Get ready to read words

	_words = words + HEAD_WORDS;
	_wordsLeft = fileWords - HEAD_WORDS;

	return RE_OK;
}

u32 ImageReader::nextHuffmanSymbol(HuffmanDecoder *dec) {
	u32 code = peek(16);

	u32 len;
	u32 sym = dec->get(code, len);

	eat(len);

	return sym;
}

