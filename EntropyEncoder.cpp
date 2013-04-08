#include "EntropyEncoder.hpp"
#include "HuffmanEncoder.hpp"
using namespace cat;


//// EntropyEncoder

void EntropyEncoder::endSymbols() {
	if (zeroRun > 0) {
		if (zeroRun < FILTER_RLE_SYMS) {
			histBZ[255 + zeroRun]++;
		} else {
			histBZ[BZ_SYMS - 1]++;
		}

		zeroRun = 0;
	}
}

void EntropyEncoder::normalizeFreqs(u32 max_freq, int num_syms, u32 hist[], u16 freqs[]) {
	static const int MAX_FREQ = 0xffff;

	// Scale to fit in 16-bit frequency counter
	while (max_freq > MAX_FREQ) {
		// For each symbol,
		for (int ii = 0; ii < num_syms; ++ii) {
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
	for (int ii = 0; ii < num_syms; ++ii) {
		freqs[ii] = static_cast<u16>( hist[ii] );
	}
}

void EntropyEncoder::reset() {
	CAT_OBJCLR(histBZ);
	maxBZ = 0;
#ifdef USE_AZ
	CAT_OBJCLR(histAZ);
	maxAZ = 0;
#endif
	zeroRun = 0;
#ifdef ADAPTIVE_ZRLE
	zeros = 0;
	total = 0;
#endif
}

void EntropyEncoder::push(u8 symbol) {
#ifdef ADAPTIVE_ZRLE
	++total;
#endif
	if (symbol == 0) {
		++zeroRun;
#ifdef ADAPTIVE_ZRLE
		++zeros;
#endif
	} else {
		if (zeroRun > 0) {
			if (zeroRun < FILTER_RLE_SYMS) {
				histBZ[255 + zeroRun]++;
			} else {
				histBZ[BZ_SYMS - 1]++;
			}

			zeroRun = 0;
#ifdef USE_AZ
			histAZ[symbol]++;
#else
			histBZ[symbol]++;
#endif
		} else {
			histBZ[symbol]++;
		}
	}
}

void EntropyEncoder::finalize() {
#ifdef ADAPTIVE_ZRLE
	if (total == 0) {
		return;
	}
#endif

	endSymbols();

	u16 freqBZ[BZ_SYMS];
#ifdef USE_AZ
	u16 freqAZ[AZ_SYMS];
#endif

#ifdef ADAPTIVE_ZRLE
	if (zeros * 100 / total >= 15) {
		usingZ = true;
#endif

		normalizeFreqs(maxBZ, BZ_SYMS, histBZ, freqBZ);
		generateHuffmanCodes(BZ_SYMS, freqBZ, codesBZ, codelensBZ);
#ifdef ADAPTIVE_ZRLE
	} else {
		usingZ = false;

		histAZ[0] = zeros;
		u32 maxAZ = 0;
		for (int ii = 1; ii < AZ_SYMS; ++ii) {
			histAZ[ii] += histBZ[ii];
		}
	}
#endif

#ifdef USE_AZ
	normalizeFreqs(maxAZ, AZ_SYMS, histAZ, freqAZ);
	generateHuffmanCodes(AZ_SYMS, freqAZ, codesAZ, codelensAZ);
#endif
}

u32 EntropyEncoder::encode(u8 symbol, ImageWriter &writer) {
	u32 bits = 0;

#ifdef ADAPTIVE_ZRLE
	if (usingZ) {
#endif
		if (symbol == 0) {
			++zeroRun;
		} else {
			if (zeroRun > 0) {
				if (zeroRun < FILTER_RLE_SYMS) {
					bits = codelensBZ[255 + zeroRun];
				} else {
					bits = codelensBZ[BZ_SYMS - 1] + 4; // estimated
				}

				zeroRun = 0;
#ifdef USE_AZ
				bits += codelensAZ[symbol];
#else
				bits += codelensBZ[symbol];
#endif
			} else {
				bits += codelensBZ[symbol];
			}
		}
#ifdef ADAPTIVE_ZRLE
	} else {
		bits += codelensAZ[symbol];
	}
#endif

	return bits;
}

u32 EntropyEncoder::encodeFinalize(ImageWriter &writer) {
	u32 bits = 0;

#ifdef ADAPTIVE_ZRLE
	if (usingZ) {
#endif
		if (zeroRun > 0) {
			if (zeroRun < FILTER_RLE_SYMS) {
				bits = codelensBZ[255 + zeroRun];
			} else {
				bits = codelensBZ[BZ_SYMS - 1] + 4; // estimated
			}
		}
#ifdef ADAPTIVE_ZRLE
	}
#endif

	return bits;
}

