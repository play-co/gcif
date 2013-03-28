#include "ImageMaskReader.hpp"
#include "EndianNeutral.hpp"
#include "BitMath.hpp"
#include "HuffmanDecoder.hpp"
using namespace cat;
using namespace std;

#include "lz4.h"


//// ImageMaskReader

void ImageMaskReader::clear() {
	if (_mask) {
		delete []_mask;
		_mask = 0;
	}

	_size = 0;
	_stride = 0;
}

bool ImageMaskReader::readHuffmanCodelens(u8 codelens[256], ImageReader &reader) {
	if CAT_UNLIKELY(reader.eof()) {
		return false;
	}

	// Decode Golomb-encoded Huffman table

	u32 pivot = reader.readBits(3);

	int tableWriteIndex = 0;
	int lag0 = 3, q = 0;

	for (;;) {
		u32 bit = reader.readBit();
		q += bit;

		if (!bit) {
			u32 result = pivot ? reader.readBits(pivot) : 0;

			result += q << pivot;
			q = 0;

			int orig = result;
			if (orig & 1) {
				orig = (orig >> 1) + 1;
			} else {
				orig = -(orig >> 1);
			}

			orig += lag0;
			lag0 = orig;

			if ((u32)orig > huffman::cMaxExpectedCodeSize) {
				return false;
			}

			codelens[tableWriteIndex++] = orig;

			// If we're done,
			if (tableWriteIndex >= 256) {
				break;
			}
		}
	}

	return true;
}

int ImageMaskReader::read(ImageReader &reader) {

	u8 codelens[256];

	readHuffmanCodelens(codelens, reader);

	huffman::init_decoder_tables(&_tables);
	huffman::generate_decoder_tables(256, codelens, &_tables, 8);

	return RE_OK;
}

