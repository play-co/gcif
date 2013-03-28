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

	while CAT_LIKELY(!reader.eof()) {
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

bool ImageMaskReader::decode(u16 width, u16 height, u32 *words, int wordCount, u32 dataHash) {
	CAT_INFO("main") << "Huffman+table hash: " << hex << MurmurHash3::hash(words, wordCount * 4);

	_width = width;
	_height = height;
	_stride = ((u32)width + 31) >> 5;
	_writeRow = 0;
	_image = new u32[(u32)height * _stride];

	_sum = 0;
	_rowLeft = 0;
	_rowStarted = false;
	_row = _image;
	_rleTime = 0;

	huffman::HuffmanDecoder decoder;

	decoder.init(words, wordCount);

	u8 *lz = new u8[65536];
	u16 lzIndex = 0, lzLast = 0;
	const int BATCH_RATE = 8192; 

	// LZ4
	{
		while (!decoder.isEOF()) {
			// Read token
			u8 token = decoder.next();

			// TODO: Change LZ4 encoding to avoid EOF checks here
			// Read Literal Length
			int literalLength = token >> 4;
			if (literalLength == 15) {
				int s;
				do {
					s = decoder.next();
					literalLength += s;
				} while (s == 255 && CAT_UNLIKELY(!decoder.isEOF()));
			}

			// Decode literal symbols
			for (int ii = 0; ii < literalLength; ++ii) {
				u8 symbol = decoder.next();
				lz[lzIndex++] = symbol;

				// Decode [wrapped] RLE sequence
				if CAT_UNLIKELY((u16)(lzIndex - lzLast) >= BATCH_RATE) {
					if CAT_UNLIKELY(lzLast > lzIndex) {
						if (decodeRLE(&lz[lzLast], 65536 - lzLast)) {
							return true;
						}

						lzLast = 0;
					}

					if CAT_UNLIKELY(decodeRLE(&lz[lzLast], lzIndex - lzLast)) {
						return true;
					}

					lzLast = lzIndex;
				}
			}

			// Read match offset
			u8 offset0 = decoder.next();
			u8 offset1 = decoder.next();
			u16 offset = ((u16)offset1 << 8) | offset0;

			// Read match length
			int matchLength = token & 15;
			if (matchLength == 15) {
				int s;
				do {
					s = decoder.next();
					matchLength += s;
				} while (s == 255 && CAT_UNLIKELY(!decoder.isEOF()));
			}
			matchLength += 4;

			// Copy match data
			//cout << "rep:" << matchLength << " off:" << offset;
			for (int ii = 0; ii < matchLength; ++ii) {
				u8 symbol = lz[ (u16)(lzIndex - offset) ];
				lz[lzIndex++] = symbol;

				// Decode [wrapped] RLE sequence
				if CAT_UNLIKELY((u16)(lzIndex - lzLast) >= BATCH_RATE) {
					if CAT_UNLIKELY(lzLast > lzIndex) {
						if (decodeRLE(&lz[lzLast], 65536 - lzLast)) {
							return true;
						}

						lzLast = 0;
					}

					if CAT_UNLIKELY(decodeRLE(&lz[lzLast], lzIndex - lzLast)) {
						return true;
					}

					lzLast = lzIndex;
				}
			}
		}

		// Decode [wrapped] RLE sequence
		if CAT_UNLIKELY(lzLast > lzIndex) {
			if (decodeRLE(&lz[lzLast], 65536 - lzLast)) {
				return true;
			}

			lzLast = 0;
		}

		if CAT_UNLIKELY(decodeRLE(&lz[lzLast], lzIndex - lzLast)) {
			return true;
		}
	}

	return true;
}

int ImageMaskReader::read(ImageReader &reader) {
	static const int TABLE_BITS = 9;

	u8 codelens[256];

	if (!readHuffmanCodelens(codelens, reader)) {
		return RE_BAD_DATA;
	}

	huffman::decoder_tables tables;

	huffman::init_decoder_tables(&tables);
	if (!huffman::generate_decoder_tables(256, codelens, &tables, TABLE_BITS)) {
		return RE_BAD_DATA;
	}

	return RE_OK;
}

