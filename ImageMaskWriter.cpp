#include "ImageMaskWriter.hpp"
#include "EndianNeutral.hpp"
#include "BitMath.hpp"
#include "HuffmanEncoder.hpp"
#include "HuffmanDecoder.hpp"
#include "Filters.hpp"
#include "GCIFWriter.hpp"

#ifdef CAT_COLLECT_STATS
#include "Log.hpp"
#include "Clock.hpp"
#endif // CAT_COLLECT_STATS

using namespace cat;
using namespace std;

#include "lz4.h"
#include "lz4hc.h"


#define DUMP_FILTER_OUTPUT

#ifdef DUMP_FILTER_OUTPUT
#include "lodepng.h"
#endif // DUMP_FILTER_OUTPUT


//// ImageMaskWriter

void ImageMaskWriter::applyFilter() {
	// Walk backwards from the end
	const int stride = _stride;
	u32 *lagger = _mask + _size - stride;
	u32 *writer = _filtered + _size - stride;
	int hctr = _height;
	while (--hctr) {
		u32 cb = 0; // assume no delta from row above

		for (int jj = 0; jj < stride; ++jj) {
			u32 above = lagger[jj - stride];
			u32 now = lagger[jj];

			u32 ydelta = now ^ above;
			u32 y2xdelta = ydelta ^ (((ydelta >> 2) | cb) & ((ydelta >> 1) | (cb << 1)));
			cb = ydelta << 30;

			writer[jj] = y2xdelta;
		}

		lagger -= stride;
		writer -= stride;
	}

	// First line
	u32 cb = 1 << 31; // assume it is on the edges
	for (int jj = 0; jj < stride; ++jj) {
		u32 now = lagger[jj];
		writer[jj] = now ^ ((now >> 1) | cb);
		cb = now << 31;
	}

#ifdef DUMP_FILTER_OUTPUT
		{
			//CAT_WARN("main") << "Writing delta image file";

			// Convert to image:

			vector<unsigned char> output;
			u8 bits = 0, bitCount = 0;

			for (int ii = 0; ii < _height; ++ii) {
				for (int jj = 0; jj < _width; ++jj) {
					u32 set = (_filtered[ii * _stride + jj / 32] >> (31 - (jj & 31))) & 1;
					bits <<= 1;
					bits |= set;
					if (++bitCount >= 8) {
						output.push_back(bits);
						bits = 0;
						bitCount = 0;
					}
				}
			}

			lodepng_encode_file("delta.png", (const unsigned char*)&output[0], _width, _height, LCT_GREY, 1);
		}
#endif // DUMP_FILTER_OUTPUT
}

void ImageMaskWriter::clear() {
	if (_mask) {
		delete []_mask;
		_mask = 0;
	}
	if (_filtered) {
		delete []_filtered;
		_filtered = 0;
	}
}

int ImageMaskWriter::initFromRGBA(const u8 *rgba, int width, int height) {

	if (!rgba || width < FILTER_ZONE_SIZE || height < FILTER_ZONE_SIZE) {
		return WE_BAD_DIMS;
	}

	if ((width & FILTER_ZONE_SIZE_MASK) || (height & FILTER_ZONE_SIZE_MASK)) {
		return WE_BAD_DIMS;
	}

	clear();

#ifdef LOWRES_MASK
	const int maskWidth = width >> FILTER_ZONE_SIZE_SHIFT;
	const int maskHeight = height >> FILTER_ZONE_SIZE_SHIFT;
#else
	const int maskWidth = width;
	const int maskHeight = height;
#endif

	// Init mask bitmatrix
	_width = maskWidth;
	_stride = (maskWidth + 31) >> 5;
	_size = maskHeight * _stride;
	_height = maskHeight;
	_mask = new u32[_size];
	_filtered = new u32[_size];

	// Assumes fully-transparent black for now
	// TODO: Also work with images that have a different most-common color
	_value = getLE(0x00000000);

	u32 *writer = _mask;

	// Set from full-transparent alpha:

#ifdef LOWRES_MASK

	// For each block,
	const u8 *alpha = (const u8*)&rgba[0] + 3;
	for (int y = 0; y < _height; ++y) {
		u32 bits = 0;
		int filled = 0;

		const u8 *col = alpha; 
		for (int x = 0; x < _width; ++x) {
			// For each block pixel,
			u32 on = 1;

			const u8 *pixel = col;
			for (int ii = 0; ii < FILTER_ZONE_SIZE; ++ii) {
				for (int jj = 0; jj < FILTER_ZONE_SIZE; ++jj) {
					if (pixel[jj * 4]) {
						on = 0;
						ii = FILTER_ZONE_SIZE;
						break;
					}
				}

				pixel += width * 4;
			}

			bits <<= 1;
			bits |= on;

			if (++filled >= 32) {
				*writer++ = bits;
				filled = 0;
				bits = 0;
			}

			col += FILTER_ZONE_SIZE * 4;
		}

		if (filled > 0) {
			*writer++ = bits;
		}

		alpha += width * 4 * FILTER_ZONE_SIZE;
	}

#else

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

#endif

#ifdef DUMP_FILTER_OUTPUT
		{
			//CAT_WARN("main") << "Writing mask image file";

			// Convert to image:

			vector<unsigned char> output;
			u8 bits = 0, bitCount = 0;

			for (int ii = 0; ii < _height; ++ii) {
				for (int jj = 0; jj < _width; ++jj) {
					u32 set = (_mask[ii * _stride + jj / 32] >> (31 - (jj & 31))) & 1;
					bits <<= 1;
					bits |= set;
					if (++bitCount >= 8) {
						output.push_back(bits);
						bits = 0;
						bitCount = 0;
					}
				}
			}

			lodepng_encode_file("mask.png", (const unsigned char*)&output[0], _width, _height, LCT_GREY, 1);
		}
#endif // DUMP_FILTER_OUTPUT

	return WE_OK;
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
	vector<int> deltas;

	u32 *lagger = _filtered;
	const int stride = _stride;

	for (int ii = 0, iilen = _height; ii < iilen; ++ii) {
		// for xdelta:
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

		const int deltaCount = static_cast<int>( deltas.size() );

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

	const int lzSize = LZ4_compressHC((char*)&rle[0], (char*)&lz[0], rle.size());

	lz.resize(lzSize);

#ifdef CAT_COLLECT_STATS
	Stats.rleBytes = rle.size();
	Stats.lzBytes = lz.size();
#endif // CAT_COLLECT_STATS
}


void ImageMaskWriter::writeEncodedLZ(const std::vector<u8> &lz, u16 codes[256], u8 codelens[256], ImageWriter &writer) {
	const int lzSize = static_cast<int>( lz.size() );

#ifdef CAT_COLLECT_STATS
	u32 data_bits = 0;
#endif // CAT_COLLECT_STATS

	for (int ii = 0; ii < lzSize; ++ii) {
		u8 symbol = lz[ii];

		u16 code = codes[symbol];
		u8 len = codelens[symbol];

		writer.writeBits(code, len);
#ifdef CAT_COLLECT_STATS
		data_bits += len;
#endif // CAT_COLLECT_STATS
	}

#ifdef CAT_COLLECT_STATS
	Stats.data_bits = data_bits;
#endif // CAT_COLLECT_STATS
}

void ImageMaskWriter::write(ImageWriter &writer) {
#ifdef CAT_COLLECT_STATS
	Clock *clock = Clock::ref();
	double t0 = clock->usec();
#endif // CAT_COLLECT_STATS

	applyFilter();

#ifdef CAT_COLLECT_STATS
	double t1 = clock->usec();
#endif // CAT_COLLECT_STATS

	vector<u8> rle;
	performRLE(rle);

#ifdef CAT_COLLECT_STATS
	double t2 = clock->usec();
#endif // CAT_COLLECT_STATS

	vector<u8> lz;
	performLZ(rle, lz);

#ifdef CAT_COLLECT_STATS
	double t3 = clock->usec();
#endif // CAT_COLLECT_STATS

	u16 freqs[256];
	collectFreqs(256, lz, freqs);

#ifdef CAT_COLLECT_STATS
	double t4 = clock->usec();
#endif // CAT_COLLECT_STATS

	u8 codelens[256];
	u16 codes[256];
	generateHuffmanCodes(256, freqs, codes, codelens);

#ifdef CAT_COLLECT_STATS
	double t5 = clock->usec();
#endif // CAT_COLLECT_STATS

	int table_bits = writeHuffmanTable(256, codelens, writer);

#ifdef CAT_COLLECT_STATS
	Stats.table_bits = table_bits;

	double t6 = clock->usec();
#endif // CAT_COLLECT_STATS

	writer.writeWord(rle.size());
	writer.writeWord(lz.size());

	writeEncodedLZ(lz, codes, codelens, writer);

#ifdef CAT_COLLECT_STATS
	double t7 = clock->usec();

	Stats.filterUsec = t1 - t0;
	Stats.rleUsec = t2 - t1;
	Stats.lzUsec = t3 - t2;
	Stats.histogramUsec = t4 - t3;
	Stats.generateTableUsec = t5 - t4;
	Stats.tableEncodeUsec = t6 - t5;
	Stats.dataEncodeUsec = t7 - t6;
	Stats.overallUsec = t7 - t0;

	Stats.originalDataBytes = _width * _height / 8;
	Stats.compressedDataBits = Stats.data_bits + Stats.table_bits;
	Stats.compressedDataBytes = Stats.compressedDataBits / 8;
	Stats.compressionRatio = Stats.originalDataBytes / (double)Stats.compressedDataBytes;

#endif // CAT_COLLECT_STATS
}

#ifdef CAT_COLLECT_STATS

bool ImageMaskWriter::dumpStats() {
	CAT_INANE("stats") << "(Mask Encoding)     Post-RLE Size : " <<  Stats.rleBytes << " bytes";
	CAT_INANE("stats") << "(Mask Encoding)      Post-LZ Size : " <<  Stats.lzBytes << " bytes";
	CAT_INANE("stats") << "(Mask Encoding) Post-Huffman Size : " << (Stats.data_bits + 7) / 8 << " bytes (" << Stats.data_bits << " bits)";
	CAT_INANE("stats") << "(Mask Encoding)        Table Size : " <<  (Stats.table_bits + 7) / 8 << " bytes (" << Stats.table_bits << " bits)";

	CAT_INANE("stats") << "(Mask Encoding)      Filtering : " <<  Stats.filterUsec << " usec (" << Stats.filterUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(Mask Encoding)            RLE : " <<  Stats.rleUsec << " usec (" << Stats.rleUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(Mask Encoding)             LZ : " <<  Stats.lzUsec << " usec (" << Stats.lzUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(Mask Encoding)      Histogram : " <<  Stats.histogramUsec << " usec (" << Stats.histogramUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(Mask Encoding) Generate Table : " <<  Stats.generateTableUsec << " usec (" << Stats.generateTableUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(Mask Encoding)   Encode Table : " <<  Stats.tableEncodeUsec << " usec (" << Stats.tableEncodeUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(Mask Encoding)    Encode Data : " <<  Stats.dataEncodeUsec << " usec (" << Stats.dataEncodeUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(Mask Encoding)        Overall : " <<  Stats.overallUsec << " usec";

	CAT_INANE("stats") << "(Mask Encoding) Throughput : " << Stats.originalDataBytes / Stats.overallUsec << " MBPS (input bytes)";
	CAT_INANE("stats") << "(Mask Encoding) Throughput : " << Stats.compressedDataBytes / Stats.overallUsec << " MBPS (output bytes)";
	CAT_INANE("stats") << "(Mask Encoding) Compression ratio : " << Stats.compressionRatio << ":1 (" << Stats.compressedDataBytes << " bytes) of original data set (" << Stats.originalDataBytes << " bytes)";

	return true;
}

#endif // CAT_COLLECT_STATS

