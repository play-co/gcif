#include "EntropyEncoder.hpp"
#include "HuffmanDecoder.hpp"
#include "HuffmanEncoder.hpp"
#include "BitMath.hpp"
using namespace cat;

#include <vector>
using namespace std;


//// EntropyEncoder

void EntropyEncoder::reset() {
	zeroRun = 0;
	runList.clear();
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
				bz_hist.add(255 + zeroRun);
			} else {
				bz_hist.add(BZ_SYMS - 1);
			}

			runList.push_back(zeroRun);
			zeroRun = 0;
#ifdef USE_AZ
			az_hist.add(symbol);
#else
			bz_hist.add(symbol);
#endif
		} else {
			bz_hist.add(symbol);
		}
	}
}

void EntropyEncoder::endSymbols() {
	if (zeroRun > 0) {
		runList.push_back(zeroRun);

		if (zeroRun < FILTER_RLE_SYMS) {
			bz_hist.add(255 + zeroRun);
		} else {
			bz_hist.add(BZ_SYMS - 1);
		}

		zeroRun = 0;
	}
}

void EntropyEncoder::finalize() {
#ifdef ADAPTIVE_ZRLE
	if (total == 0) {
		return;
	}
#endif

	endSymbols();

#ifdef ADAPTIVE_ZRLE
	if (zeros * 100 / total >= ADAPTIVE_ZRLE_THRESH) {
		usingZ = true;
#endif

		bz_encoder.init(bz_hist);
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
	az_encoder.init(az_hist);
#endif

	runListReadIndex = 0;
}

u32 EntropyEncoder::writeOverhead(ImageWriter &writer) {
	u32 bitcount = bz_encoder.writeTable(writer);

#ifdef USE_AZ
	bitcount += az_encoder.writeTable(writer);
#endif

	return bitcount;
}

u32 EntropyEncoder::encode(u8 symbol, ImageWriter &writer) {
	u32 bits = 0;

#ifdef ADAPTIVE_ZRLE
	if (usingZ) {
#endif
		if (symbol == 0) {
			if (zeroRun == 0) {
				if (runListReadIndex < runList.size()) {
					int runLength = runList[runListReadIndex++];
					bits += writeZeroRun(runLength, writer);
				}
			}
			++zeroRun;
		} else {
			if (zeroRun > 0) {
				zeroRun = 0;
#ifdef USE_AZ
				bits += az_encoder.writeSymbol(symbol, writer);
#else
				bits += bz_encoder.writeSymbol(symbol, writer);
#endif
			} else {
				bits += bz_encoder.writeSymbol(symbol, writer);
			}
		}
#ifdef ADAPTIVE_ZRLE
	} else {
		bits += az_encoder.writeSymbol(symbol, writer);
	}
#endif

	return bits;
}

int EntropyEncoder::writeZeroRun(int run, ImageWriter &writer) {
	if (run < 0) {
		return 0;
	}

	int zsym;
	bool rider = false;

	if (run < FILTER_RLE_SYMS) {
		zsym = 255 + run;
	} else {
		zsym = BZ_SYMS - 1;
		rider = true;
	}

	int bits = bz_encoder.writeSymbol(zsym, writer);

	if (rider) {
		run -= FILTER_RLE_SYMS;
		while (run >= 255) {
			writer.writeBits(255, 8);
			bits += 8;
			run -= 255;
		}

		writer.writeBits(run, 8);
		bits += 8;
	}

	return bits;
}

