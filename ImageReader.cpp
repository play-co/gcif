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

u32 ImageReader::nextHuffmanSymbol(huffman::decoder_tables *table) {
	u32 code = peek(16);

	// Fast static Huffman decoder, centralized here since this is how most of the data is encoded

	u32 k = static_cast<u32>((code >> 16) + 1);
	u32 sym, len;

	if (k <= table->table_max_code) {
		u32 t = table->lookup[code >> (32 - table->table_bits)];

		sym = static_cast<u16>( t );
		len = static_cast<u16>( t >> 16 );
	}
	else {
		len = table->decode_start_code_size;

		const u32 *max_codes = table->max_codes;

		for (;;) {
			if (k <= max_codes[len - 1])
				break;
			len++;
		}

		int val_ptr = table->val_ptrs[len - 1] + static_cast<int>((code >> (32 - len)));

		if (((u32)val_ptr >= table->num_syms)) {
			return 0;
		}

		sym = table->sorted_symbol_order[val_ptr];
	}

	eat(len);
}

