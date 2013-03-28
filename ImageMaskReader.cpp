#include "ImageMaskReader.hpp"
#include "EndianNeutral.hpp"
#include "BitMath.hpp"
#include "HuffmanDecoder.hpp"

#ifdef CAT_COLLECT_STATS
#include "Log.hpp"
#include "Clock.hpp"

static cat::Clock *m_clock = 0;
#endif // CAT_COLLECT_STATS

using namespace cat;

#include "lz4.h"


//// ImageMaskReader

void ImageMaskReader::clear() {
	if (_mask) {
		delete []_mask;
		_mask = 0;
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
#ifdef CAT_COLLECT_STATS
	Stats.pivot = pivot;
#endif // CAT_COLLECT_STATS

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

			if (static_cast<u32>( orig ) > HuffmanDecoder::MAX_CODE_SIZE) {
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

bool ImageMaskReader::decodeRLE(u8 *rle, int len) {
	if (len <= 0) {
		return false;
	}

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
			if (rowStarted) {
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

					//if (_writeRow > 120 && _writeRow < 200) cout << sum << ":" << bitOn << " ";

					// If previous state was toggled on,
					if (bitOn) {
						u32 bitsUsedMask = 0xffffffff >> (bitOffset & 31);

						if (newOffset <= wordOffset) {
							//cout << "S(" << row[newOffset] << "," << bitsUsedMask << "," << shift << ") ";
							row[newOffset] ^= bitsUsedMask & (0xfffffffe << shift);
							//cout << "S(" << row[newOffset] << "," << bitsUsedMask << "," << shift << ") ";
						} else {
							//cout << "M ";
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

						//cout << "Z ";
						row[newOffset] ^= (1 << shift);

						if (sum == 0 && _lastSum) {
							bitOn ^= 1;
						}
						_lastSum = 1;
					}
				}

				bitOffset += sum + 1;

				// If just finished this row,
				if (--rowLeft <= 0) {
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

					//if (_writeRow > 120 && _writeRow < 200) cout << endl;

					if (++_writeRow >= _height) {
						// done!
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

					//if (_writeRow > 120 && _writeRow < 200) cout << "(empty)" << endl;
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

	return false;
}


bool ImageMaskReader::decodeLZ(HuffmanDecoder &decoder, ImageReader &reader) {
	const int BATCH_RATE = 8192;

	u8 *lz = _lz;
	u16 lzIndex = 0, lzLast = 0;

	while CAT_UNLIKELY(!reader.eof()) {
		// Read token
		u8 token = reader.nextHuffmanSymbol(&decoder);

		// TODO: Change LZ4 encoding to avoid EOF checks here

		// Read Literal Length
		int literalLength = token >> 4;
		if (literalLength == 15) {
			int s;
			do {
				s = reader.nextHuffmanSymbol(&decoder);
				literalLength += s;
			} while (s == 255 && CAT_UNLIKELY(!reader.eof()));
		}

		// Decode literal symbols
		for (int ii = 0; ii < literalLength; ++ii) {
			u8 symbol = reader.nextHuffmanSymbol(&decoder);
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
		u8 offset0 = reader.nextHuffmanSymbol(&decoder);
		u8 offset1 = reader.nextHuffmanSymbol(&decoder);
		u16 offset = ((u16)offset1 << 8) | offset0;

		// Read match length
		int matchLength = token & 15;
		if (matchLength == 15) {
			int s;
			do {
				s = reader.nextHuffmanSymbol(&decoder);
				matchLength += s;
			} while (s == 255 && CAT_UNLIKELY(!reader.eof()));
		}
		matchLength += 4;

		// Copy match data
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

	return true;
}

bool ImageMaskReader::init(const ImageInfo *info) {
	clear();

	_stride = (info->width + 31) >> 5;
	_width = info->width;
	_height = info->height;

	_mask = new u32[_stride * _height];
	_row = _mask;

	_sum = 0;
	_lastSum = 0;
	_rowLeft = 0;
	_rowStarted = false;

	_lz = new u8[65536];

	_bitOffset = 0;
	_bitOn = true;
	_writeRow = 0;

	return true;
}

int ImageMaskReader::read(ImageReader &reader) {
	static const int NUM_SYMS = 256;
	static const int TABLE_BITS = 9;

#ifdef CAT_COLLECT_STATS
	m_clock = Clock::ref();

	double t0 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	if (!init(reader.getImageInfo())) {
		return RE_MASK_INIT;
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
	if (!decoder.init(NUM_SYMS, codelens, TABLE_BITS)) {
		return RE_MASK_DECI;
	}

#ifdef CAT_COLLECT_STATS
	double t3 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	if (!decodeLZ(decoder, reader)) {
		return RE_MASK_LZ;
	}

#ifdef CAT_COLLECT_STATS
	double t4 = m_clock->usec();

	Stats.initUsec = t1 - t0;
	Stats.readCodelensUsec = t2 - t1;
	Stats.initHuffmanUsec = t3 - t2;
	Stats.overallUsec = t4 - t0;

	Stats.originalDataBytes = _width * _height / 8;
	Stats.compressedDataBytes = reader.getTotalDataWords() * 4;
#endif // CAT_COLLECT_STATS

	return RE_OK;
}

#ifdef CAT_COLLECT_STATS

bool ImageMaskReader::dumpStats() {
	CAT_INFO("stats") << "(Mask Decoding) Table Pivot : " <<  Stats.pivot;

	CAT_INFO("stats") << "(Mask Decoding) Initialization : " <<  Stats.initUsec << " usec (" << Stats.initUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INFO("stats") << "(Mask Decoding)  Read Codelens : " <<  Stats.readCodelensUsec << " usec (" << Stats.readCodelensUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INFO("stats") << "(Mask Decoding)  Setup Huffman : " <<  Stats.initHuffmanUsec << " usec (" << Stats.initHuffmanUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INFO("stats") << "(Mask Decoding)        Overall : " <<  Stats.overallUsec << " usec";

	CAT_INFO("stats") << "(Mask Decoding) Throughput : " << Stats.compressedDataBytes / Stats.overallUsec << " MBPS (input bytes)";
	CAT_INFO("stats") << "(Mask Decoding) Throughput : " << Stats.originalDataBytes / Stats.overallUsec << " MBPS (output bytes)";

	return true;
}

#endif // CAT_COLLECT_STATS


