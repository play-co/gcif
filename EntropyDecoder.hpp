#ifndef ENTROPY_DECODER_HPP
#define ENTROPY_DECODER_HPP

#include "HuffmanDecoder.hpp"
#include "ImageReader.hpp"

/*
 * Game Closure Entropy-Based Decompression
 *
 * Decodes the bitstream generated by EntropyEncoder.hpp
 *
 * See that file for more information.
 */

namespace cat {


//// EntropyDecoder

template<int NUM_SYMS, int ZRLE_SYMS> class EntropyDecoder {
public:
	static const int BZ_SYMS = NUM_SYMS + ZRLE_SYMS;
	static const int AZ_SYMS = NUM_SYMS;
	static const int BZ_TAIL_SYM = BZ_SYMS - 1;
	static const int HUFF_LUT_BITS = 9;

protected:
	int _zeroRun;
	HuffmanDecoder _bz;
	HuffmanDecoder _az;
	bool _afterZero;

public:
	CAT_INLINE EntropyDecoder() {
	}
	virtual CAT_INLINE ~EntropyDecoder() {
	}

	bool init(ImageReader &reader) {
		if (!_bz.init(BZ_SYMS, reader, HUFF_LUT_BITS)) {
			return false;
		}

		if (!_az.init(AZ_SYMS, reader, HUFF_LUT_BITS)) {
			return false;
		}

		_afterZero = false;
		_zeroRun = 0;

		return true;
	}

	u16 next(ImageReader &reader) {
		// If in a zero run,
		if (_zeroRun > 0) {
			--_zeroRun;
			return 0;
		}

		// If after zero,
		if (_afterZero) {
			_afterZero = false;
			return _az.next(reader);
		}

		// Read before-zero symbol
		u16 sym = (u16)_bz.next(reader);

		// If not a zero run,
		if (sym < NUM_SYMS) {
			return sym;
		}

		// If zRLE is represented by the symbol itself,
		if (sym < BZ_TAIL_SYM) {
			// Decode zero run from symbol
			_zeroRun = sym - NUM_SYMS;
		} else {
			// Read riders
			u32 run = ZRLE_SYMS - 1, s;

			s = reader.readBits(8);
			run += s;

			// If another byte is expected,
			if (s == 255) {
				s = reader.readBits(8);
				run += s;

				// If the remaining data is in words,
				if (s == 255) {
					do {
						s = reader.readBits(16);
						run += s;
					} while (s == 65535); // HuffmanDecoder emits 0 on EOF
				}
			}

			_zeroRun = run;
		}

		_afterZero = true;
		return 0;
	}
};

} // namespace cat

#endif // ENTROPY_DECODER_HPP

