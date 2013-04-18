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
	static const int BZ_ZERO_OFF = NUM_SYMS - 1;
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
			bits = _bz.writeSymbol(BZ_ZERO_OFF + run, writer);
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
					writer.writeBits(run, 16);
					bits += 16;
				}
			} else {
				// Write out FF bytes
				while (run >= 255) {
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
					_bz_hist.add(BZ_ZERO_OFF + zeroRun);
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
				_bz_hist.add(BZ_ZERO_OFF + zeroRun);
			} else {
				_bz_hist.add(BZ_TAIL_SYM);
			}

			_zeroRun = 0;
		}

		// Initialize Huffman encoders with histograms
		_bz.init(_bz_hist);
		_az.init(_az_hist);

		// Set the run list read index for writing
		_runListReadIndex = 0;
	}

	int writeTables(ImageWriter &writer) {
		int bitcount = _bz.writeTable(writer);

		bitcount += _az.writeTable(writer);

		return bitcount;
	}

	int write(u16 symbol, ImageWriter &writer) {
		CAT_DEBUG_ENFORCE(symbol < NUM_SYMS);

		int bits = 0;

		// If zero,
		if (symbol == 0) {
			// If starting a zero run,
			if (_zeroRun == 0) {
				CAT_DEBUG_ENFORCE(_runListReadIndex < _runList.size());

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
};


} // namespace cat

#endif // ENTROPY_ENCODER_HPP

