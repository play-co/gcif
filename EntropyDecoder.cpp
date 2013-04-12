#include "EntropyDecoder.hpp"
using namespace cat;


//// EntropyDecoder

void EntropyDecoder::clear() {
}

bool EntropyDecoder::readHuffmanTable(int num_syms, HuffmanDecoder &table, ImageReader &reader) {
	static const u32 LEN_MOD = HuffmanDecoder::MAX_CODE_SIZE + 1;

	u8 codelens[BZ_SYMS]; // Choose max(LEN_MOD, BZ_SYMS, AZ_SYMS)

	for (int ii = 0; ii < LEN_MOD; ++ii) {
		u8 len = reader.readBits(4);
		if (len >= 15) {
			len += reader.readBit();
		}

		codelens[ii] = len;
	}

	HuffmanDecoder table_decoder;

	// If table decoder could be initialized,
	if (!table_decoder.init(LEN_MOD, codelens, 8)) {
		return false;
	}

	// Decode context modeling on huffman codelens
	u8 lag0 = 3;
	codelens[0] = 0;
	for (int ii = 0, iiend = num_syms - 1; ii < iiend; ++ii) {
		u8 delta = reader.nextHuffmanSymbol(&table_decoder);
		u8 codelen = (delta + lag0) % LEN_MOD;
		lag0 = codelen;

		codelens[ii + 1] = codelen;
	}

	// If table could not be initialized,
	if (!table.init(num_syms, codelens, 9)) {
		return false;
	}

	return true;
}

bool EntropyDecoder::init(ImageReader &reader) {
	if (!readHuffmanTable(BZ_SYMS, _bz, reader)) {
		return false;
	}

#ifdef USE_AZ
	if (!readHuffmanTable(AZ_SYMS, _az, reader)) {
		return false;
	}

	_afterZero = 0;
#endif

	_zeroRun = 0;

	return true;
}

u8 EntropyDecoder::next(ImageReader &reader) {
	if (_zeroRun) {
		--_zeroRun;
		return 0;
	} else {
		u16 sym;

#ifdef USE_AZ
		if (_afterZero) {
			sym = (u16)reader.nextHuffmanSymbol(&_az);
			_afterZero = false;
		} else {
#endif
			sym = (u16)reader.nextHuffmanSymbol(&_bz);
#ifdef USE_AZ
		}
#endif

		// If zero,
		if (sym > 255) {
			// If it is represented by rider bytes,
			if (sym >= BZ_SYMS - 1) {
				// Read riders
				u32 run = FILTER_RLE_SYMS, s;
				do {
					s = reader.readBits(8);
					run += s;
				} while (s == 255); // eof => 0 so this is safe

				_zeroRun = run - 1;
			} else {
				_zeroRun = sym - 256;
			}
#ifdef USE_AZ
			_afterZero = true;
#endif
			return 0;
		} else {
			return sym;
		}
	}
}

