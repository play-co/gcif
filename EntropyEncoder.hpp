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

#ifndef ENTROPY_ENCODER_HPP
#define ENTROPY_ENCODER_HPP

#include "ImageWriter.hpp"
#include "HuffmanEncoder.hpp"
#include <vector>

/*
 * Game Closure Entropy-Based Compression
 *
 * This reuseable class produces a Huffman encoding of a data stream that
 * is expected to contain runs of zeroes.
 *
 * Statistics before and after zeroes are recorded separately, so that the
 * after-zero statistics can use fewer symbols for shorter codes.
 *
 * Zero runs are encoded with a set of ZRLE_SYMS symbols that directly encode
 * the lengths of these runs.  When the runs are longer than the ZRLE_SYMS
 * count, up to two FF bytes are written out and subtracted from the run count.
 * The remaining bytes are 16-bit words that repeat on FFFF.
 *
 * The two Huffman tables are written using the compressed representation in
 * HuffmanEncoder.
 */

namespace cat {


//// EntropyEncoder

template<int NUM_SYMS, int ZRLE_SYMS> class EntropyEncoder {
public:
	static const int BZ_SYMS = NUM_SYMS + ZRLE_SYMS;
	static const int AZ_SYMS = NUM_SYMS;
	static const int BZ_TAIL_SYM = BZ_SYMS - 1;

protected:
	FreqHistogram<BZ_SYMS> _bz_hist;
	FreqHistogram<AZ_SYMS> _az_hist;

	HuffmanEncoder<BZ_SYMS> _bz;
	HuffmanEncoder<AZ_SYMS> _az;

	int _zeroRun;
	std::vector<int> _runList;
	int _runListReadIndex;

	int writeZeroRun(int run, ImageWriter &writer) {
		if (run <= 0) {
			return 0;
		}

		int bits;

		if (run < ZRLE_SYMS) {
			bits = _bz.writeSymbol(NUM_SYMS + run - 1, writer);
		} else {
			bits = _bz.writeSymbol(BZ_TAIL_SYM, writer);

			run -= ZRLE_SYMS;

			// If multiple FF bytes will be emitted,
			if (run >= 255 + 255) {
				writer.writeBits(255, 8);
				writer.writeBits(255, 8);

				// Step it up to 16-bit words
				run -= 255 + 255;
				bits += 8 + 8;
				while (run >= 65535) {
					writer.writeBits(65535, 16);
					bits += 16;
					run -= 65535;
				}
				writer.writeBits(run, 16);
				bits += 16;
			} else {
				// Write out FF bytes
				if (run >= 255) {
					writer.writeBits(255, 8);
					bits += 8;
					run -= 255;
				}

				// Write out last byte
				writer.writeBits(run, 8);
				bits += 8;
			}
		}

		return bits;
	}

	int simulateZeroRun(int run) {
		if (run <= 0) {
			return 0;
		}

		int bits;

		if (run < ZRLE_SYMS) {
			bits = _bz.simulateWrite(NUM_SYMS + run - 1);
		} else {
			bits = _bz.simulateWrite(BZ_TAIL_SYM);

			run -= ZRLE_SYMS;

			// If multiple FF bytes will be emitted,
			if (run >= 255 + 255) {
				// Step it up to 16-bit words
				run -= 255 + 255;
				bits += 8 + 8;
				while (run >= 65535) {
					bits += 16;
					run -= 65535;
				}
				bits += 16;
			} else {
				// Write out FF bytes
				if (run >= 255) {
					bits += 8;
					run -= 255;
				}

				// Write out last byte
				bits += 8;
			}
		}

		return bits;
	}

public:
	CAT_INLINE EntropyEncoder() {
		_zeroRun = 0;
	}

	CAT_INLINE virtual ~EntropyEncoder() {
	}

	void add(u16 symbol) {
		CAT_DEBUG_ENFORCE(symbol < NUM_SYMS);

		if (symbol == 0) {
			++_zeroRun;
		} else {
			const int zeroRun = _zeroRun;

			if (zeroRun > 0) {
				if (zeroRun < ZRLE_SYMS) {
					_bz_hist.add(NUM_SYMS + zeroRun - 1);
				} else {
					_bz_hist.add(BZ_TAIL_SYM);
				}

				_runList.push_back(zeroRun);
				_zeroRun = 0;

				_az_hist.add(symbol);
			} else {
				_bz_hist.add(symbol);
			}
		}
	}

	void finalize() {
		const int zeroRun = _zeroRun;

		// If a zero run is in progress at the end,
		if (zeroRun > 0) {
			// Record it
			_runList.push_back(zeroRun);

			// Record symbols that will be emitted
			if (zeroRun < ZRLE_SYMS) {
				_bz_hist.add(NUM_SYMS + zeroRun - 1);
			} else {
				_bz_hist.add(BZ_TAIL_SYM);
			}
		}

		// Initialize Huffman encoders with histograms
		_bz.init(_bz_hist);
		_az.init(_az_hist);
	}

	int writeTables(ImageWriter &writer) {
		int bitcount = _bz.writeTable(writer);
		bitcount += _az.writeTable(writer);

		return bitcount;
	}

	void reset() {
		_zeroRun = 0;

		// Set the run list read index for writing
		_runListReadIndex = 0;
	}

	int write(u16 symbol, ImageWriter &writer) {
		CAT_DEBUG_ENFORCE(symbol < NUM_SYMS);

		int bits = 0;

		// If zero,
		if (symbol == 0) {
			// If starting a zero run,
			if (_zeroRun == 0) {
				CAT_DEBUG_ENFORCE(_runListReadIndex < (int)_runList.size());

				// Write stored zero run
				int runLength = _runList[_runListReadIndex++];

				bits += writeZeroRun(runLength, writer);
			}

			++_zeroRun;
		} else {
			// If just out of a zero run,
			if (_zeroRun > 0) {
				_zeroRun = 0;
				bits += _az.writeSymbol(symbol, writer);
			} else {
				bits += _bz.writeSymbol(symbol, writer);
			}
		}

		return bits;
	}

	int simulate(u16 symbol) {
		CAT_DEBUG_ENFORCE(symbol < NUM_SYMS);

		int bits = 0;

		// If zero,
		if (symbol == 0) {
			// If starting a zero run,
			if (_zeroRun == 0) {
				CAT_DEBUG_ENFORCE(_runListReadIndex < (int)_runList.size());

				// Write stored zero run
				int runLength = _runList[_runListReadIndex++];

				bits += simulateZeroRun(runLength, writer);
			}

			++_zeroRun;
		} else {
			// If just out of a zero run,
			if (_zeroRun > 0) {
				_zeroRun = 0;
				bits += _az.simulateWrite(symbol);
			} else {
				bits += _bz.simulateWrite(symbol);
			}
		}

		return bits;
	}
};


} // namespace cat

#endif // ENTROPY_ENCODER_HPP

