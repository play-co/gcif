/*
	Copyright (c) 2013 Christopher A. Taylor.  All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	* Redistributions of source code must retain the above copyright notice,
	  this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	* Neither the name of GCIF nor the names of its contributors may be used
	  to endorse or promote products derived from this software without
	  specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

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

//#define DUMP_MONOCHROME

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

int ImageMaskReader::decodeRLE(u8 *rle, int len) {
	if CAT_UNLIKELY(len <= 0) {
		return RE_MASK_LZ;
	}

#ifdef CAT_COLLECT_STATS
	double t0 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	u32 lastSum = 0;
	bool rowStarted = false;
	int rowLeft = 0;
	u32 *row = _mask;
	int bitOffset = 0;
	int bitOn = true;
	int writeRow = 0;

	const int offsetLimit = _stride * _height;

	u32 sum = 0;
	for (int ii = 0; ii < len; ++ii) {
		u8 symbol = rle[ii];
		if (symbol >= 255) {
			sum += 255;
			continue;
		}
		sum += symbol;

		// If has read row length yet,
		if CAT_LIKELY(rowStarted) {
			int wordOffset = bitOffset >> 5;
			int newBitOffset = bitOffset + sum;
			int newOffset = newBitOffset >> 5;
			int shift = 31 - (newBitOffset & 31);

			// If new write offset is outside of the mask,
			if (newOffset >= offsetLimit) {
				return RE_MASK_LZ;
			}

			// If at the first row,
			if CAT_UNLIKELY(writeRow <= 0) {
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
					lastSum = 0;
				} else {
					// Fill bottom bits with 0s (do nothing)

					row[newOffset] ^= (1 << shift);

					if (sum == 0 && lastSum) {
						bitOn ^= 1;
					}
					lastSum = 1;
				}
			}

			bitOffset += sum + 1;

			// If just finished this row,
			if CAT_UNLIKELY(--rowLeft <= 0) {
				int wordOffset = bitOffset >> 5;

				if CAT_LIKELY(writeRow > 0) {
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

				if CAT_UNLIKELY(++writeRow >= _height) {
					// done!
#ifdef CAT_COLLECT_STATS
					double t1 = m_clock->usec();
					Stats.rleUsec += t1 - t0;
#endif // CAT_COLLECT_STATS
					return RE_OK;
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
				if (writeRow > 0) {
					u32 *copy = row - stride;
					for (int ii = 0; ii < stride; ++ii) {
						row[ii] = copy[ii];
					}
				} else {
					for (int ii = 0; ii < stride; ++ii) {
						row[ii] = 0xffffffff;
					}
				}

				if (++writeRow >= _height) {
					// done!
#ifdef CAT_COLLECT_STATS
					double t1 = m_clock->usec();
					Stats.rleUsec += t1 - t0;
#endif // CAT_COLLECT_STATS
					return RE_OK;
				}

				row += stride;
			} else {
				rowStarted = true;

				// Reset row decode state
				bitOn = false;
				bitOffset = 0;
				lastSum = 0;

				// Setup first word
				if CAT_LIKELY(writeRow > 0) {
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

		// Reset sum
		sum = 0;
	}

#ifdef CAT_COLLECT_STATS
	double t1 = m_clock->usec();
	Stats.rleUsec += t1 - t0;
#endif // CAT_COLLECT_STATS

	return RE_MASK_LZ;
}


int ImageMaskReader::decodeLZ(ImageReader &reader) {
	int rleSize = reader.readWord();
	int lzSize = reader.readWord();

	_rle = new u8[rleSize];
	_lz = new u8[lzSize];

	if (lzSize >= HUFF_THRESH) {
		static const int NUM_SYMS = 256;

		HuffmanDecoder decoder;
		if (!decoder.init(NUM_SYMS, reader, 8)) {
			return RE_MASK_DECI;
		}

		for (int ii = 0; ii < lzSize; ++ii) {
			_lz[ii] = decoder.next(reader);
		}
	} else {
		for (int ii = 0; ii < lzSize; ++ii) {
			_lz[ii] = reader.readBits(8);
		}
	}

	if (reader.eof()) {
		return RE_MASK_LZ;
	}

	int result = LZ4_uncompress_unknownOutputSize(reinterpret_cast<const char *>( _lz ), reinterpret_cast<char *>( _rle ), lzSize, rleSize);

	if (result != rleSize) {
		return RE_MASK_LZ;
	}

	if (reader.eof()) {
		return RE_MASK_LZ;
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

	const int maskWidth = header->width;
	const int maskHeight = header->height;

	_stride = (maskWidth + 31) >> 5;
	_width = maskWidth;
	_height = maskHeight;

	_mask = new u32[_stride * _height];

	return RE_OK;
}

int ImageMaskReader::read(ImageReader &reader) {
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

	Stats.rleUsec = 0;
#endif // CAT_COLLECT_STATS

	if ((err = decodeLZ(reader))) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t2 = m_clock->usec();

	Stats.initUsec = t1 - t0;
	Stats.lzUsec = t2 - t1 - Stats.rleUsec;
	Stats.overallUsec = t2 - t0;
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
	CAT_INANE("stats") << "(Mask Decode) Initialization : " <<  Stats.initUsec << " usec (" << Stats.initUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(Mask Decode)     Huffman+LZ : " <<  Stats.lzUsec << " usec (" << Stats.lzUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(Mask Decode)     RLE+Filter : " <<  Stats.rleUsec << " usec (" << Stats.rleUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(Mask Decode)        Overall : " <<  Stats.overallUsec << " usec";

	return true;
}

#endif // CAT_COLLECT_STATS

