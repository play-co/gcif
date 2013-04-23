#include "ImageReader.hpp"
#include "EndianNeutral.hpp"
#include "GCIFReader.hpp"
using namespace cat;


//// ImageReader

void ImageReader::clear() {
	_words = 0;
}

u32 ImageReader::refill() {
	u64 bits = _bits;
	int bitsLeft = _bitsLeft;

	CAT_DEBUG_ENFORCE(bitsLeft < 32);

	if CAT_LIKELY(_wordsLeft > 0) {
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

	FileValidationHash hh;
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

	u32 fastHash = getLE(words[2]);
	hh.hashWord(fastHash);

	u32 goodHash = getLE(words[3]);
	hh.hashWord(goodHash);

	u32 headHash = getLE(words[4]);
	if CAT_UNLIKELY(headHash != hh.final(HEAD_WORDS)) {
		return RE_BAD_HEAD;
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

	return RE_OK;
}

