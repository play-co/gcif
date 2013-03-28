#include "ImageMaskWriter.hpp"
#include "EndianNeutral.hpp"
#include "BitMath.hpp"
#include "HuffmanEncoder.hpp"
#include "HuffmanDecoder.hpp"
using namespace cat;
using namespace std;

#include "lz4.h"
#include "lz4hc.h"


//// ImageMaskWriter

void ImageMaskWriter::applyFilter() {
	// Walk backwards from the end
	const int stride = _stride;
	u32 *lagger = _mask + _size - stride;
	int hctr = _height;
	while (--hctr) {
		u32 cb = 0; // assume no delta from row above

		for (int jj = 0; jj < stride; ++jj) {
			u32 above = lagger[jj - stride];
			u32 now = lagger[jj];

			u32 ydelta = now ^ above;
			u32 y2xdelta = ydelta ^ (((ydelta >> 2) | cb) & ((ydelta >> 1) | (cb << 1)));
			cb = ydelta << 30;

			lagger[jj] = y2xdelta;
		}

		lagger -= stride;
	}

	// First line
	u32 cb = 1 << 31; // assume it is on the edges
	for (int jj = 0; jj < stride; ++jj) {
		u32 now = lagger[jj];
		lagger[jj] = now ^ ((now >> 1) | cb);
		cb = now << 31;
	}
}

#if 0
		{
			CAT_WARN("main") << "Writing delta image file";

			// Convert to image:

			vector<unsigned char> output;
			u8 bits = 0, bitCount = 0;

			for (int ii = 0; ii < height; ++ii) {
				for (int jj = 0; jj < width; ++jj) {
					u32 set = (buffer[ii * bufferStride + jj / 32] >> (31 - (jj & 31))) & 1;
					bits <<= 1;
					bits |= set;
					if (++bitCount >= 8) {
						output.push_back(bits);
						bits = 0;
						bitCount = 0;
					}
				}
			}

			lodepng_encode_file("delta.png", (const unsigned char*)&output[0], width, height, LCT_GREY, 1);
		}
#endif

void ImageMaskWriter::clear() {
	if (_mask) {
		delete []_mask;
	}
}

bool ImageMaskWriter::initFromRGBA(u8 *rgba, int width, int height) {

	if (!rgba || width <= 0 || height <= 0) {
		return false;
	}

	clear();

	_stride = (width + 31) >> 5;
	_size = height * _stride;
	_height = height;

	// Assumes fully-transparent black for now
	// TODO: Also work with images that have a different most-common color
	_value = getLE(0x00000000);

	u32 *writer = _mask;

	// Set from full-transparent alpha:

	const unsigned char *alpha = (const unsigned char*)&rgba[0] + 3;
	for (int y = 0; y < height; ++y) {
		for (int x = 0, len = width >> 5; x < len; ++x) {
			u32 bits = (alpha[0] == 0);
			bits = (bits << 1) | (alpha[4] == 0);
			bits = (bits << 1) | (alpha[8] == 0);
			bits = (bits << 1) | (alpha[12] == 0);
			bits = (bits << 1) | (alpha[16] == 0);
			bits = (bits << 1) | (alpha[20] == 0);
			bits = (bits << 1) | (alpha[24] == 0);
			bits = (bits << 1) | (alpha[28] == 0);
			bits = (bits << 1) | (alpha[32] == 0);
			bits = (bits << 1) | (alpha[36] == 0);
			bits = (bits << 1) | (alpha[40] == 0);
			bits = (bits << 1) | (alpha[44] == 0);
			bits = (bits << 1) | (alpha[48] == 0);
			bits = (bits << 1) | (alpha[52] == 0);
			bits = (bits << 1) | (alpha[56] == 0);
			bits = (bits << 1) | (alpha[60] == 0);
			bits = (bits << 1) | (alpha[64] == 0);
			bits = (bits << 1) | (alpha[68] == 0);
			bits = (bits << 1) | (alpha[72] == 0);
			bits = (bits << 1) | (alpha[76] == 0);
			bits = (bits << 1) | (alpha[80] == 0);
			bits = (bits << 1) | (alpha[84] == 0);
			bits = (bits << 1) | (alpha[88] == 0);
			bits = (bits << 1) | (alpha[92] == 0);
			bits = (bits << 1) | (alpha[96] == 0);
			bits = (bits << 1) | (alpha[100] == 0);
			bits = (bits << 1) | (alpha[104] == 0);
			bits = (bits << 1) | (alpha[108] == 0);
			bits = (bits << 1) | (alpha[112] == 0);
			bits = (bits << 1) | (alpha[116] == 0);
			bits = (bits << 1) | (alpha[120] == 0);
			bits = (bits << 1) | (alpha[124] == 0);

			*writer++ = bits;
			alpha += 128;
		}

		u32 ctr = width & 31;
		if (ctr) {
			u32 bits = 0;
			while (ctr--) {
				bits = (bits << 1) | (alpha[0] == 0);
				alpha += 4;
			}

			*writer++ = bits;
		}
	}

	// Filter it

	applyFilter();

	return true;
}

static CAT_INLINE void byteEncode(vector<unsigned char> &bytes, u32 data) {
	unsigned char b0 = data;

	if (data >>= 7) {
		u8 b1 = data;

		if (data >>= 7) {
			u8 b2 = data;

			if (data >>= 7) {
				u8 b3 = data;

				if (data >>= 7) {
					u8 b4 = data;

					bytes.push_back(b4 | 128);
				}

				bytes.push_back(b3 | 128);
			}

			bytes.push_back(b2 | 128);
		}

		bytes.push_back(b1 | 128);
	}

	bytes.push_back(b0 & 127);
}

void ImageMaskWriter::performRLE(vector<u8> &rle) {
	int prevCount = 0;
	vector<int> deltas;

	u32 *lagger = _mask;
	const int stride = _stride;

	// ydelta for count
	int lastDeltaCount = 0;

	for (int ii = 0, iilen = _height; ii < iilen; ++ii) {
		int prevIndex = 0;

		// for xdelta:
		int lastZeroes = 0;
		int zeroes = 0;

		for (int jj = 0; jj < stride; ++jj) {
			u32 now = lagger[jj];

			if (now) {
				u32 bit, lastbit = 31;
				do {
					bit = BSR32(now);

					zeroes += lastbit - bit;

					deltas.push_back(zeroes);

					zeroes = 0;
					lastbit = bit - 1;
					now ^= 1 << bit;
				} while (now);

				zeroes += bit;
			} else {
				zeroes += 32;
			}
		}

		int deltaCount = static_cast<int>( deltas.size() );
		byteEncode(rle, deltaCount);
		for (int kk = 0; kk < deltaCount; ++kk) {
			int delta = deltas[kk];
			byteEncode(rle, delta);
		}

		deltas.clear();

		lagger += stride;
	}
}

void ImageMaskWriter::performLZ(const std::vector<u8> &rle, std::vector<u8> &lz) {
	lz.resize(LZ4_compressBound(static_cast<int>( rle.size() )));

	int lzSize = LZ4_compressHC((char*)&rle[0], (char*)&lz[0], rle.size());

	lz.resize(lzSize);
}

void ImageMaskWriter::collectFreqs(const std::vector<u8> &lz, u16 freqs[256]) {
	const int NUM_SYMS = 256;
	const int lzSize = static_cast<int>( lz.size() );
	const int MAX_FREQ = 0xffff;

	int hist[NUM_SYMS] = {0};
	int max_freq = 0;

	// Perform histogram, and find maximum symbol count
	for (int ii = 0; ii < lzSize; ++ii) {
		int count = ++hist[lz[ii]];

		if (max_freq < count) {
			max_freq = count;
		}
	}

	// Scale to fit in 16-bit frequency counter
	while (max_freq > MAX_FREQ) {
		// For each symbol,
		for (int ii = 0; ii < NUM_SYMS; ++ii) {
			int count = hist[ii];

			// If it exists,
			if (count) {
				count >>= 1;

				// Do not let it go to zero if it is actually used
				if (!count) {
					count = 1;
				}
			}
		}

		// Update max
		max_freq >>= 1;
	}

	// Store resulting scaled histogram
	for (int ii = 0; ii < NUM_SYMS; ++ii) {
		freqs[ii] = static_cast<u16>( hist[ii] );
	}
}

void ImageMaskWriter::generateHuffmanCodes(u16 freqs[256], u16 codes[256], u8 codelens[256]) {
	const int NUM_SYMS = 256;

	huffman::huffman_work_tables state;

	u32 max_code_size;
	u32 total_freq;

	huffman::generate_huffman_codes(&state, NUM_SYMS, freqs, codelens, max_code_size, total_freq);

	if (max_code_size > huffman::cMaxExpectedCodeSize) {
		huffman::limit_max_code_size(NUM_SYMS, codelens, huffman::cMaxExpectedCodeSize);
	}

	huffman::generate_codes(NUM_SYMS, codelens, codes);
}

void ImageMaskWriter::writeHuffmanTable(u8 codelens[256], ImageWriter &writer) {
	vector<unsigned char> huffTable;

	// Delta-encode the Huffman codelen table
	int lag0 = 3;
	u32 sum = 0;
	for (int ii = 0; ii < 256; ++ii) {
		u8 symbol = ii;
		u8 codelen = codelens[symbol];

		int delta = codelen - lag0;
		lag0 = codelen;

		if (delta <= 0) {
			delta = -delta << 1;
		} else {
			delta = ((delta - 1) << 1) | 1;
		}

		huffTable.push_back(delta);
		sum += delta;
	}

	// Find K shift
	sum >>= 8;
	u32 shift = sum > 0 ? BSR32(sum) : 0;
	u32 shiftMask = (1 << shift) - 1;

	writer.writeBits(shift, 3);

	// For each symbol,
	for (int ii = 0; ii < huffTable.size(); ++ii) {
		int symbol = huffTable[ii];
		int q = symbol >> shift;

		if CAT_UNLIKELY(q > 31) {
			for (int ii = 0; ii < q; ++ii) {
				writer.writeBit(1);
			}
			writer.writeBit(0);
		} else {
			writer.writeBits((0xffffffff >> (31 - q)) << 1, q + 1);
		}

		if (shift > 0) {
			writer.writeBits(symbol & shiftMask, shift);
		}
	}
}

void ImageMaskWriter::writeEncodedLZ(const std::vector<u8> &lz, u16 codes[256], u8 codelens[256], ImageWriter &writer) {
	const int lzSize = static_cast<int>( lz.size() );

	for (int ii = 0; ii < lzSize; ++ii) {
		u8 symbol = lz[ii];

		writer.writeBits(codes[symbol], codelens[symbol]);
	}
}

void ImageMaskWriter::write(ImageWriter &writer) {
	vector<u8> rle;
	performRLE(rle);

	vector<u8> lz;
	performLZ(rle, lz);

	u16 freqs[256];
	collectFreqs(lz, freqs);

	u8 codelens[256];
	u16 codes[256];
	generateHuffmanCodes(freqs, codes, codelens);

	writeHuffmanTable(codelens, writer);

	writeEncodedLZ(lz, codes, codelens, writer);
}

