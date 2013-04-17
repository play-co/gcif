#include "EntropyDecoder.hpp"
using namespace cat;


//// EntropyDecoder

void EntropyDecoder::clear() {
}

bool EntropyDecoder::init(ImageReader &reader) {
	if (!_bz.init(BZ_SYMS, reader, 9)) {
		return false;
	}

#ifdef USE_AZ
	if (!_az.init(AZ_SYMS, reader, 9)) {
		return false;
	}

	_afterZero = 0;
#endif

	_zeroRun = 0;

	return true;
}

u16 EntropyDecoder::next(ImageReader &reader) {
	if (_zeroRun) {
		--_zeroRun;
		return 0;
	} else {
		u16 sym;

#ifdef USE_AZ
		if (_afterZero) {
			sym = (u16)_az.next(reader);
			_afterZero = false;
		} else {
#endif
			sym = (u16)_bz.next(reader);
#ifdef USE_AZ
		}
#endif

		// If zero,
		if (sym >= 256 + RECENT_SYMS) {
			// If it is represented by rider bytes,
			if (sym >= BZ_SYMS - 1) {
				// Read riders
				u32 run = FILTER_RLE_SYMS, s;

				s = reader.readBits(8);
				run += s;

				if (s == 255) {
					s = reader.readBits(8);
					run += s;

					if (s == 255) {
						run += reader.readBits(16);
					}
				}

				_zeroRun = run - 1;
			} else {
				_zeroRun = sym - (256 + RECENT_SYMS);
			}
#ifdef USE_AZ
			_afterZero = true;
#endif
			return 0;
		} else {
			return sym;
		}
	}
}

