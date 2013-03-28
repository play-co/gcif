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
}

int ImageReader::init(const void *buffer, int bytes) {
}

u32 ImageReader::nextHuffmanSymbol(huffman::decoder_tables *table) {
	u32 code = peek(16);

	// Fast static Huffman decoder

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

