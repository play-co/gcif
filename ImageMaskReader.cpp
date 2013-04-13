#include "ImageMaskReader.hpp"
#include "EndianNeutral.hpp"
#include "BitMath.hpp"
#include "HuffmanDecoder.hpp"
#include "Filters.hpp"
#include "GCIFReader.hpp"

#ifdef CAT_COLLECT_STATS
#include "Log.hpp"
#include "Clock.hpp"

static cat::Clock *m_clock = 0;
#endif // CAT_COLLECT_STATS

using namespace cat;

#include "lz4.h"

#define DUMP_MONOCHROME

#ifdef DUMP_MONOCHROME
#include "lodepng.h"
#include <vector>
#endif


//// ImageMaskReader

void ImageMaskReader::clear() {
	if (_mask) {
		delete []_mask;
		_mask = 0;
	}
	if (_rle) {
		delete []_rle;
		_rle = 0;
	}
	if (_lz) {
		delete []_lz;
		_lz = 0;
	}
}

bool ImageMaskReader::readHuffmanCodelens(u8 codelens[256], ImageReader &reader) {
	if CAT_UNLIKELY(reader.eof()) {
		return false;
	}

	// Decode Golomb-encoded Huffman table

	int pivot = reader.readBits(3);

	int tableWriteIndex = 0;
	int lag0 = 3, q = 0;

	while CAT_LIKELY(!reader.eof()) {
		u32 bit = reader.readBit();
		q += bit;

		if (!bit) {
			u32 result = pivot > 0 ? reader.readBits(pivot) : 0;

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

			if CAT_UNLIKELY(static_cast<u32>( orig ) > HuffmanDecoder::MAX_CODE_SIZE) {
				return false;
			}

			codelens[tableWriteIndex++] = orig;

			// If we're done,
			if CAT_UNLIKELY(tableWriteIndex >= 256) {
				break;
			}
		}
	}

	return true;
}

bool ImageMaskReader::decodeRLE(u8 *rle, int len) {
	if CAT_UNLIKELY(len <= 0) {
		return false;
	}

#ifdef CAT_COLLECT_STATS
	double t0 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	u32 sum = _sum;
	bool rowStarted = _rowStarted;
	int rowLeft = _rowLeft;
	u32 *row = _row;

	int bitOffset = _bitOffset;
	bool bitOn = _bitOn;

	for (int ii = 0; ii < len; ++ii) {
		u8 symbol = rle[ii];

		sum <<= 7;
		if CAT_UNLIKELY(symbol & 128) {
			sum |= symbol & 127;
		} else {
			sum |= symbol;

			// If has read row length yet,
			if CAT_LIKELY(rowStarted) {
				int wordOffset = bitOffset >> 5;
				int newBitOffset = bitOffset + sum;
				int newOffset = newBitOffset >> 5;
				int shift = 31 - (newBitOffset & 31);

				// If at the first row,
				if CAT_UNLIKELY(_writeRow <= 0) {
					/*
					 * First row is handled specially:
					 *
					 * For each input X:
					 * 1. Write out X bits of current state
					 * 2. Flip the state
					 * 3. Then write out one bit of the new state
					 * When out of input, pad to the end with current state
					 *
					 * Demo:
					 *
					 * (1) 1 0 1 0 0 0 1 1 1
					 *     0 1 1 1 0 0 1 0 0
					 * {1, 0, 0, 2}
					 * --> 1 0 1 0 0 0 1 1 1
					 */

					// If previous state was 0,
					if (bitOn ^= 1) {
						// Fill bottom bits with 0s (do nothing)

						// For each intervening word and new one,
						for (int ii = wordOffset + 1; ii < newOffset; ++ii) {
							row[ii] = 0;
						}

						// Write a 1 at the new location
						row[newOffset] = 1 << shift;
					} else {
						u32 bitsUsedMask = 0xffffffff >> (bitOffset & 31);

						if (newOffset <= wordOffset) {
							row[newOffset] |= bitsUsedMask & (0xfffffffe << shift);
						} else {
							// Fill bottom bits with 1s
							row[wordOffset] |= bitsUsedMask;

							// For each intervening word,
							for (int ii = wordOffset + 1; ii < newOffset; ++ii) {
								row[ii] = 0xffffffff;
							}

							// Set 1s for new word, ending with a 0
							row[newOffset] = 0xfffffffe << shift;
						}
					}
				} else {
					/*
					 * 0011110100
					 * 0011001100
					 * {2,0,2,0}
					 * 0011110100
					 *
					 * 0111110100
					 * 1,0,0,0,0,1
					 * 0110110
					 *
					 * Same as first row except only flip on when we get X = 0
					 * And we will XOR with previous row
					 */

					// If previous state was toggled on,
					if (bitOn) {
						u32 bitsUsedMask = 0xffffffff >> (bitOffset & 31);

						if (newOffset <= wordOffset) {
							row[newOffset] ^= bitsUsedMask & (0xfffffffe << shift);
						} else {
							// Fill bottom bits with 1s
							row[wordOffset] ^= bitsUsedMask;

							// For each intervening word,
							for (int ii = wordOffset + 1; ii < newOffset; ++ii) {
								row[ii] ^= 0xffffffff;
							}

							// Set 1s for new word, ending with a 0
							row[newOffset] ^= (0xfffffffe << shift);
						}

						bitOn ^= 1;
						_lastSum = 0;
					} else {
						// Fill bottom bits with 0s (do nothing)

						row[newOffset] ^= (1 << shift);

						if (sum == 0 && _lastSum) {
							bitOn ^= 1;
						}
						_lastSum = 1;
					}
				}

				bitOffset += sum + 1;

				// If just finished this row,
				if CAT_UNLIKELY(--rowLeft <= 0) {
					int wordOffset = bitOffset >> 5;

					if CAT_LIKELY(_writeRow > 0) {
						// If last bit written was 1,
						if (bitOn) {
							// Fill bottom bits with 1s

							row[wordOffset] ^= 0xffffffff >> (bitOffset & 31);

							// For each remaining word,
							for (int ii = wordOffset + 1, iilen = _stride; ii < iilen; ++ii) {
								row[ii] ^= 0xffffffff;
							}
						}
					} else {
						// If last bit written was 1,
						if (bitOn) {
							// Fill bottom bits with 1s
							row[wordOffset] |= 0xffffffff >> (bitOffset & 31);

							// For each remaining word,
							for (int ii = wordOffset + 1, iilen = _stride; ii < iilen; ++ii) {
								row[ii] = 0xffffffff;
							}
						} else {
							// Fill bottom bits with 0s (do nothing)

							// For each remaining word,
							for (int ii = wordOffset + 1, iilen = _stride; ii < iilen; ++ii) {
								row[ii] = 0;
							}
						}
					}

					if CAT_UNLIKELY(++_writeRow >= _height) {
						// done!
#ifdef CAT_COLLECT_STATS
						double t1 = m_clock->usec();
						Stats.rleUsec += t1 - t0;
#endif // CAT_COLLECT_STATS
						return true;
					}

					rowStarted = false;
					row += _stride;
				}
			} else {
				rowLeft = sum;

				// If row was empty,
				if (rowLeft == 0) {
					const int stride = _stride;

					// Decode as an exact copy of the row above it
					if (_writeRow > 0) {
						u32 *copy = row - stride;
						for (int ii = 0; ii < stride; ++ii) {
							row[ii] = copy[ii];
						}
					} else {
						for (int ii = 0; ii < stride; ++ii) {
							row[ii] = 0xffffffff;
						}
					}

					if (++_writeRow >= _height) {
						// done!
#ifdef CAT_COLLECT_STATS
						double t1 = m_clock->usec();
						Stats.rleUsec += t1 - t0;
#endif // CAT_COLLECT_STATS
						return true;
					}

					row += stride;
				} else {
					rowStarted = true;

					// Reset row decode state
					bitOn = false;
					bitOffset = 0;
					_lastSum = 0;

					// Setup first word
					if CAT_LIKELY(_writeRow > 0) {
						const int stride = _stride;

						u32 *copy = row - stride;
						for (int ii = 0; ii < stride; ++ii) {
							row[ii] = copy[ii];
						}
					} else {
						row[0] = 0;
					}
				}
			}

			sum = 0;
		}
	}

	_bitOffset = bitOffset;
	_bitOn = bitOn;
	_row = row;
	_sum = sum;
	_rowStarted = rowStarted;
	_rowLeft = rowLeft;

#ifdef CAT_COLLECT_STATS
	double t1 = m_clock->usec();
	Stats.rleUsec += t1 - t0;
#endif // CAT_COLLECT_STATS

	return false;
}


bool ImageMaskReader::decodeLZ(HuffmanDecoder &decoder, ImageReader &reader) {
	u32 rleSize = reader.readWord();
	u32 lzSize = reader.readWord();

	CAT_WARN("TEST") << rleSize;
	CAT_WARN("TEST") << lzSize;

	_rle = new u8[rleSize];
	_lz = new u8[lzSize];

	for (int ii = 0; ii < lzSize; ++ii) {
		_lz[ii] = reader.nextHuffmanSymbol(&decoder);
	}

	if (reader.eof()) {
		return false;
	}

	int result = LZ4_uncompress_unknownOutputSize(reinterpret_cast<const char *>( _lz ), reinterpret_cast<char *>( _rle ), lzSize, rleSize);

	if (result != rleSize) {
		return false;
	}

	return decodeRLE(_rle, rleSize);
}

int ImageMaskReader::init(const ImageHeader *header) {
	clear();

	if (header->width < FILTER_ZONE_SIZE || header->height < FILTER_ZONE_SIZE) {
		return RE_BAD_DIMS;
	}

	if ((header->width & FILTER_ZONE_SIZE_MASK) || (header->height & FILTER_ZONE_SIZE_MASK)) {
		return RE_BAD_DIMS;
	}

#ifdef LOWRES_MASK
	const int maskWidth = header->width >> FILTER_ZONE_SIZE_SHIFT;
	const int maskHeight = header->height >> FILTER_ZONE_SIZE_SHIFT;
#else
	const int maskWidth = header->width;
	const int maskHeight = header->height;
#endif

	_stride = (maskWidth + 31) >> 5;
	_width = maskWidth;
	_height = maskHeight;

	// TODO: Reuse these buffers
	_mask = new u32[_stride * _height];
	_row = _mask;

	_sum = 0;
	_lastSum = 0;
	_rowLeft = 0;
	_rowStarted = false;

	_bitOffset = 0;
	_bitOn = true;
	_writeRow = 0;

	return RE_OK;
}

int ImageMaskReader::read(ImageReader &reader) {
	static const int NUM_SYMS = 256;

#ifdef CAT_COLLECT_STATS
	m_clock = Clock::ref();

	double t0 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	int err;

	if ((err = init(reader.getImageHeader()))) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t1 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	u8 codelens[NUM_SYMS];
	if (!readHuffmanCodelens(codelens, reader)) {
		return RE_MASK_CODES;
	}

#ifdef CAT_COLLECT_STATS
	double t2 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	HuffmanDecoder decoder;
	if (!decoder.init(NUM_SYMS, codelens, 8)) {
		return RE_MASK_DECI;
	}

#ifdef CAT_COLLECT_STATS
	double t3 = m_clock->usec();

	Stats.rleUsec = 0;
#endif // CAT_COLLECT_STATS

	if (!decodeLZ(decoder, reader)) {
		return RE_MASK_LZ;
	}

#ifdef CAT_COLLECT_STATS
	double t4 = m_clock->usec();

	Stats.initUsec = t1 - t0;
	Stats.readCodelensUsec = t2 - t1;
	Stats.initHuffmanUsec = t3 - t2;
	Stats.lzUsec = t4 - t3 - Stats.rleUsec;
	Stats.overallUsec = t4 - t0;

	Stats.originalDataBytes = _width * _height / 8;
#endif // CAT_COLLECT_STATS

#ifdef DUMP_MONOCHROME
	std::vector<unsigned char> output;
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

	lodepng_encode_file("decoded_mono.png", (const unsigned char*)&output[0], _width, _height, LCT_GREY, 1);
#endif

	return RE_OK;
}

#ifdef CAT_COLLECT_STATS

bool ImageMaskReader::dumpStats() {
	CAT_INFO("stats") << "(Mask Decode) Initialization : " <<  Stats.initUsec << " usec (" << Stats.initUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INFO("stats") << "(Mask Decode)  Read Codelens : " <<  Stats.readCodelensUsec << " usec (" << Stats.readCodelensUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INFO("stats") << "(Mask Decode)  Setup Huffman : " <<  Stats.initHuffmanUsec << " usec (" << Stats.initHuffmanUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INFO("stats") << "(Mask Decode)     Huffman+LZ : " <<  Stats.lzUsec << " usec (" << Stats.lzUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INFO("stats") << "(Mask Decode)     RLE+Filter : " <<  Stats.rleUsec << " usec (" << Stats.rleUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INFO("stats") << "(Mask Decode)        Overall : " <<  Stats.overallUsec << " usec";

	CAT_INFO("stats") << "(Mask Decode)  Original Size : " <<  Stats.originalDataBytes << " bytes";
	CAT_INFO("stats") << "(Mask Decode)     Throughput : " << Stats.originalDataBytes / Stats.overallUsec << " MBPS (output bytes/time)";

	return true;
}

#endif // CAT_COLLECT_STATS

