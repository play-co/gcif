#include "EntropyEncoder.hpp"
#include "HuffmanDecoder.hpp"
#include "HuffmanEncoder.hpp"
#include "BitMath.hpp"
using namespace cat;

#include <vector>
using namespace std;


//// EntropyEncoder

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


#ifdef FALLBACK_CHAOS_OVERHEAD

#include <iostream>
using namespace std;

static u32 writeHuffmanTable(int num_syms, u8 codelens[], ImageWriter &writer) {
	u32 bitcount = 0;

	for (int ii = 1; ii < num_syms; ++ii) {
		u8 len = codelens[ii];

		cout << (int)len << " ";

		while (len >= 15) {
			writer.writeBits(15, 4);
			len -= 15;
			bitcount += 4;
		}

		writer.writeBits(len, 4);
		bitcount += 4;
	}

	cout << endl << endl;

	return bitcount;
}

#elif defined(GOLOMB_CHAOS_OVERHEAD)

static u32 writeHuffmanTable(int num_syms, u8 codelens[], ImageWriter &writer) {
	vector<u8> huffTable;

	// Delta-encode the Huffman codelen table
	int lag0 = 3;
	u32 sum = 0;

	static const u32 LEN_MOD = HuffmanDecoder::MAX_CODE_SIZE + 1;

	// Skip symbol 0 (never sent)
	for (int ii = 1; ii < num_syms; ++ii) {
		u8 symbol = ii;
		u8 codelen = codelens[symbol];

		u32 delta = (codelen - lag0) % LEN_MOD;
		lag0 = codelen;

		huffTable.push_back(delta);
		sum += delta;
	}

	// Find K shift
	sum >>= 8;
	u32 shift = sum > 0 ? BSR32(sum) : 0;
	u32 shiftMask = (1 << shift) - 1;

	writer.writeBits(shift, 3);

	u32 table_bits = 3;

	// For each symbol,
	for (int ii = 0; ii < huffTable.size(); ++ii) {
		int symbol = huffTable[ii];
		int q = symbol >> shift;

		if CAT_UNLIKELY(q > 31) {
			for (int ii = 0; ii < q; ++ii) {
				writer.writeBit(1);
				++table_bits;
			}
			writer.writeBit(0);
			++table_bits;
		} else {
			writer.writeBits((0x7fffffff >> (31 - q)) << 1, q + 1);
			table_bits += q + 1;
		}

		if (shift > 0) {
			writer.writeBits(symbol & shiftMask, shift);
			table_bits += shift;
		}
	}

	return table_bits;
}

#else

static u32 writeHuffmanTable(int num_syms, u8 codelens[], ImageWriter &writer) {
	u32 bitcount = 0;

	vector<u8> huffTable;

	// Delta-encode the Huffman codelen table
	u32 lag0 = 3;
	u32 sum = 0;

	static const u32 LEN_MOD = HuffmanDecoder::MAX_CODE_SIZE + 1;

	// Skip symbol 0 (never sent)
	for (int ii = 1; ii < num_syms; ++ii) {
		u8 symbol = ii;
		u8 codelen = codelens[symbol];

		u32 delta = (codelen - lag0) % LEN_MOD;
		lag0 = codelen;

		huffTable.push_back(delta);
		sum += delta;
	}

	const int delta_syms = LEN_MOD;
	u16 freqs[delta_syms];

	collectFreqs(delta_syms, huffTable, freqs);

	u16 delta_codes[delta_syms];
	u8 delta_codelens[delta_syms];

	generateHuffmanCodes(delta_syms, freqs, delta_codes, delta_codelens);

	// Write huffman table's huffman table
	for (int ii = 0; ii < delta_syms; ++ii) {
		u8 len = delta_codelens[ii];

		while (len >= 15) {
			writer.writeBits(15, 4);
			len -= 15;
			bitcount += 4;
		}

		writer.writeBits(len, 4);
		bitcount += 4;
	}

	// Write huffman table
	for (int ii = 0; ii < huffTable.size(); ++ii) {
		u8 delta = huffTable[ii];

		u16 code = delta_codes[delta];
		u8 codelen = delta_codelens[delta];

		writer.writeBits(code, codelen);
		bitcount += codelen;
	}

	return bitcount;
}

#endif


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

u32 EntropyEncoder::encodeFinalize(ImageWriter &writer) {
	// Zeroes are written up front so no need for a finalize step
	return 0;
}

