/*
	Copyright (c) 2013 Game Closure.  All rights reserved.

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
#include "GCIFReader.h"

#ifdef CAT_COLLECT_STATS
#include "../encoder/Log.hpp"
#include "../encoder/Clock.hpp"

#include <iomanip>
using namespace std;

static cat::Clock *m_clock = 0;
#endif // CAT_COLLECT_STATS

using namespace cat;

#include "lz4.h"


//// ImageMaskReader

int ImageMaskReader::decodeLZ(ImageReader & CAT_RESTRICT reader) {
	int rleSize = reader.read9();
	int lzSize = reader.read9();

	_rle.resize(rleSize);
	_lz.resize(lzSize);

	// If compressed,
	if (reader.readBit()) {
		static const int NUM_SYMS = 256;

		HuffmanDecoder decoder;
		if (!decoder.init(NUM_SYMS, reader, 8)) {
			CAT_DEBUG_EXCEPTION();
			return GCIF_RE_MASK_DECI;
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
		CAT_DEBUG_EXCEPTION();
		return GCIF_RE_MASK_LZ;
	}

	int result = LZ4_uncompress_unknownOutputSize(reinterpret_cast<const char *>( _lz.get() ), reinterpret_cast<char *>( _rle.get() ), lzSize, rleSize);

	if (result != rleSize) {
		CAT_DEBUG_EXCEPTION();
		return GCIF_RE_MASK_LZ;
	}

	if (reader.eof()) {
		CAT_DEBUG_EXCEPTION();
		return GCIF_RE_MASK_LZ;
	}

	// Set up decoder
	_rle_next = _rle.get();
	_rle_remaining = rleSize;
	_scanline_y = 0;

	return GCIF_RE_OK;
}

int ImageMaskReader::init(int xsize, int ysize) {
	const int maskWidth = xsize;
	const int maskHeight = ysize;

	_color = 0;
	_stride = (maskWidth + 31) >> 5;
	_xsize = maskWidth;
	_ysize = maskHeight;

	_mask.resize(_stride);

	return GCIF_RE_OK;
}

int ImageMaskReader::read(ImageReader & CAT_RESTRICT reader, int planes, int xsize, int ysize) {
#ifdef CAT_COLLECT_STATS
	m_clock = Clock::ref();

	double t0 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	int err;

	if ((err = init(xsize, ysize))) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t1 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	_enabled = reader.readBit() != 0;
	if (_enabled) {
		if (planes == 4) {
			_color = getLE(reader.readWord());
		} else {
			_color = 0;
		}

		if ((err = decodeLZ(reader))) {
			return err;
		}
	} else {
		// Clear mask when disabled to avoid two checks
		_mask.fill_00();
	}

#ifdef CAT_COLLECT_STATS
	double t2 = m_clock->usec();

	Stats.initUsec = t1 - t0;
	Stats.lzUsec = t2 - t1;
	Stats.overallUsec = t2 - t0;
#endif // CAT_COLLECT_STATS

	return GCIF_RE_OK;
}

const u32 *ImageMaskReader::nextScanline() {
	if (!_enabled) {
		return _mask.get();
	}

	// Read RLE symbol count
	int sym_count = 0;
	const u8 * CAT_RESTRICT rle = _rle_next;
	int rle_remaining = _rle_remaining;

	while (rle_remaining-- > 0) {	
		u8 sym = *rle++;
		sym_count += sym;

		if (sym < 255) {
			break;
		}
	}

	const int stride = _stride;
	u32 * CAT_RESTRICT row = _mask.get();

	// If no symbols on this row,
	if (sym_count == 0) {
		// If first row,
		if (_scanline_y == 0) {
			// Initialize to all 1
			for (int ii = 0; ii < stride; ++ii) {
				row[ii] = 0xffffffff;
			}
		}
		// Otherwise: Row is a copy of previous
	} else {
		// If first row,
		int bitOn = 0;
		if (_scanline_y == 0) {
			// Initialize to all 0
			for (int ii = 0; ii < stride; ++ii) {
				row[ii] = 0;
			}
			bitOn = 1;
		}

		// Read this scanline
		int bitOffset = 0;
		int lastSum = 0;
		int sum = 0;

		while (rle_remaining-- > 0) {
			u8 sym = *rle++;
			sum += sym;

			// If sum is complete,
			if (sym < 255) {
				int wordOffset = bitOffset >> 5;
				int newBitOffset = bitOffset + sum;
				int newOffset = newBitOffset >> 5;
				int shift = 31 - (newBitOffset & 31);

				// If new write offset is outside of the mask,
				if (newOffset >= _stride) {
					CAT_DEBUG_EXCEPTION();
					break;
				}

				// If at the first row,
				if (_scanline_y == 0) {
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

						// Write a 1 at the new location
						row[newOffset] |= 1 << shift;
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
				if (--sym_count <= 0) {
					int wordOffset = bitOffset >> 5;

					if CAT_LIKELY(_scanline_y > 0) {
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

					// Done
					break;
				}

				// Reset sum
				sum = 0;
			}
		}
	}

	// Update RLE pointer
	_rle_remaining = rle_remaining;
	_rle_next = rle;
	++_scanline_y;
	return _mask.get();
}


#ifdef CAT_COLLECT_STATS

bool ImageMaskReader::dumpStats() {
	if (!_enabled) {
		CAT_INANE("stats") << "(Mask Decode) Disabled.";
	} else {
		CAT_INANE("stats") << "(Mask Decode)   Chosen Color : (" << (_color & 255) << "," << ((_color >> 8) & 255) << "," << ((_color >> 16) & 255) << "," << ((_color >> 24) & 255) << ") ...";
		CAT_INANE("stats") << "(Mask Decode) Initialization : " <<  Stats.initUsec << " usec (" << Stats.initUsec * 100.f / Stats.overallUsec << " %total)";
		CAT_INANE("stats") << "(Mask Decode)     Huffman+LZ : " <<  Stats.lzUsec << " usec (" << Stats.lzUsec * 100.f / Stats.overallUsec << " %total)";
		CAT_INANE("stats") << "(Mask Decode)        Overall : " <<  Stats.overallUsec << " usec";
	}

	return true;
}

#endif // CAT_COLLECT_STATS

