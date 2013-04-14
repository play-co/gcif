#include "EntropyEncoder.hpp"
#include "HuffmanDecoder.hpp"
#include "HuffmanEncoder.hpp"
#include "BitMath.hpp"
using namespace cat;

#include <vector>
using namespace std;


//// EntropyEncoder

void EntropyEncoder::reset() {
	CAT_OBJCLR(histBZ);
	maxBZ = 0;
#ifdef USE_AZ
	CAT_OBJCLR(histAZ);
	maxAZ = 0;
#endif
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
				histBZ[255 + zeroRun]++;
			} else {
				histBZ[BZ_SYMS - 1]++;
			}

			runList.push_back(zeroRun);
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

void EntropyEncoder::endSymbols() {
	if (zeroRun > 0) {
		runList.push_back(zeroRun);

		if (zeroRun < FILTER_RLE_SYMS) {
			histBZ[255 + zeroRun]++;
		} else {
			histBZ[BZ_SYMS - 1]++;
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

	u16 freqBZ[BZ_SYMS];
#ifdef USE_AZ
	u16 freqAZ[AZ_SYMS];
#endif

#ifdef ADAPTIVE_ZRLE
	if (zeros * 100 / total >= ADAPTIVE_ZRLE_THRESH) {
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

	runListReadIndex = 0;
}

u32 EntropyEncoder::writeOverhead(ImageWriter &writer) {
	u32 bitcount = writeHuffmanTable(BZ_SYMS, codelensBZ, writer);

#ifdef USE_AZ
	bitcount += writeHuffmanTable(AZ_SYMS, codelensAZ, writer);
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
				writer.writeBits(codesAZ[symbol], codelensAZ[symbol]);
				bits += codelensAZ[symbol];
#else
				writer.writeBits(codesBZ[symbol], codelensBZ[symbol]);
				bits += codelensBZ[symbol];
#endif
			} else {
				writer.writeBits(codesBZ[symbol], codelensBZ[symbol]);
				bits += codelensBZ[symbol];
			}
		}
#ifdef ADAPTIVE_ZRLE
	} else {
		writer.writeBits(codesAZ[symbol], codelensAZ[symbol]);
		bits += codelensAZ[symbol];
	}
#endif

	return bits;
}

int EntropyEncoder::writeZeroRun(int run, ImageWriter &writer) {
	if (run < 0) {
		return 0;
	}

	int bits, zsym;
	bool rider = false;

	if (run < FILTER_RLE_SYMS) {
		zsym = 255 + run;
	} else {
		zsym = BZ_SYMS - 1;
		rider = true;
	}

	writer.writeBits(codesBZ[zsym], codelensBZ[zsym]);
	bits = codelensBZ[zsym];

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

