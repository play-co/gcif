#ifndef CAT_HUFFMAN_ENCODER_H
#define CAT_HUFFMAN_ENCODER_H

#include "Platform.hpp"
#include "ImageWriter.hpp"
#include "HuffmanDecoder.hpp"
#include "Log.hpp"
#include <vector>

/*
 * This was all adapted from the LZHAM project
 * https://code.google.com/p/lzham/
 */

// Copyright (c) 2009-2012 Richard Geldreich, Jr. <richgel99@gmail.com>
//
// LZHAM uses the MIT License:
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

namespace cat {

namespace huffman {


static const u32 cHuffmanMaxSupportedSyms = 512;

struct sym_freq
{
	u32 freq;
	u16 left, right;

	CAT_INLINE bool operator<(const sym_freq &other) const
	{
		return freq > other.freq;
	}
};


struct huffman_work_tables {
	enum { cMaxInternalNodes = cHuffmanMaxSupportedSyms };

	sym_freq syms0[cHuffmanMaxSupportedSyms + 1 + cMaxInternalNodes];
	sym_freq syms1[cHuffmanMaxSupportedSyms + 1 + cMaxInternalNodes];
};


//// Huffman functions from LZHAM

// Modified: one_sym is set as nonzero symbol+1 when there is just one symbol to avoid writing bits for these
bool generate_huffman_codes(huffman_work_tables *state, u32 num_syms, const u16 *pFreq, u8 *pCodesizes,
							u32 &max_code_size, u32 &total_freq_ret, u32 &one_sym);

bool limit_max_code_size(u32 num_syms, u8 *pCodesizes, u32 max_code_size);

bool generate_codes(u32 num_syms, const u8 *pCodesizes, u16 *pCodes);


} // namespace huffman


void normalizeFreqs(u32 max_freq, int num_syms, u32 hist[], u16 freqs[]);
void collectArrayFreqs(int num_syms, int data_size, u8 data[], u16 freqs[]);
void collectFreqs(int num_syms, const std::vector<u8> &lz, u16 freqs[]);

// Try several methods of encoding the table and choose the best one
int writeCompressedHuffmanTable(int num_syms, u8 codelens[], ImageWriter &writer);


//// FreqHistogram

template<int NUM_SYMS> class FreqHistogram {
public:
	u32 hist[NUM_SYMS];
	u32 max_freq;

	CAT_INLINE FreqHistogram() {
		CAT_OBJCLR(hist);
		max_freq = 0;
	}

	CAT_INLINE void add(u32 symbol) {
		u32 freq = ++hist[symbol];

		if (freq > max_freq) {
			max_freq = freq;
		}
	}

	CAT_INLINE void addMore(u32 symbol, u32 count) {
		u32 freq = hist[symbol] += count;

		if (freq > max_freq) {
			max_freq = freq;
		}
	}

	CAT_INLINE int firstHighestPeak() {
		u32 highest = hist[0];
		u32 peak_sym = 0;

		// For each remaining symbol,
		for (u32 sym = 1; sym < NUM_SYMS; ++sym) {
			// If it is higher,
			if (highest < hist[sym]) {
				// Remember it well
				highest = hist[sym];
				peak_sym = sym;
			}
		}

		return peak_sym;
	}

	CAT_INLINE void normalize(u16 freqs[NUM_SYMS]) {
		normalizeFreqs(max_freq, NUM_SYMS, hist, freqs);
	}
};


//// HuffmanEncoder

template<int NUM_SYMS> class HuffmanEncoder {
public:
	u16 _codes[NUM_SYMS];
	u8 _codelens[NUM_SYMS];
	u32 _one_sym;

	CAT_INLINE bool init(FreqHistogram<NUM_SYMS> &hist) {
		u16 freqs[NUM_SYMS];

		hist.normalize(freqs);

		return init(freqs);
	}

	CAT_INLINE bool init(u16 freqs[]) {
		if (!initCodelens(freqs)) {
			CAT_EXCEPTION();
			return false;
		}

		initCodes();
		return true;
	}

	// Split this up because you can simulate writes without generating full codes
	CAT_INLINE bool initCodelens(u16 freqs[]) {
		huffman::huffman_work_tables state;
		u32 max_code_size, total_freq;

		if (!huffman::generate_huffman_codes(&state, NUM_SYMS, freqs, _codelens, max_code_size, total_freq, _one_sym)) {
			CAT_EXCEPTION();
			return false;
		}

		if (!_one_sym) {
			if (max_code_size > HuffmanDecoder::MAX_CODE_SIZE) {
				huffman::limit_max_code_size(NUM_SYMS, _codelens, HuffmanDecoder::MAX_CODE_SIZE);
			}
		}

		return true;
	}
	CAT_INLINE void initCodes() {
		if (!_one_sym) {
			huffman::generate_codes(NUM_SYMS, _codelens, _codes);
		}
	}

	CAT_INLINE int writeTable(ImageWriter &writer) {
		return writeCompressedHuffmanTable(NUM_SYMS, _codelens, writer);
	}

	CAT_INLINE int writeSymbol(u32 sym, ImageWriter &writer) {
		const u32 one_sym = _one_sym;

		if (one_sym) {
			return 0;
		} else {
			u16 code = _codes[sym];
			u8 codelen = _codelens[sym];

			writer.writeBits(code, codelen);
			return codelen;
		}
	}

	CAT_INLINE int simulateWrite(u32 sym) {
		const u32 one_sym = _one_sym;

		if (one_sym) {
			return 0;
		} else {
			u8 codelen = _codelens[sym];
			return codelen;
		}
	}
};


//// HuffmanTableEncoder

class HuffmanTableEncoder {
public:
	static const int NUM_SYMS = HuffmanDecoder::MAX_CODE_SIZE + 1;
	static const int BZ_SYMS = NUM_SYMS + 1;

protected:
	FreqHistogram<BZ_SYMS> _bz_hist;

	HuffmanEncoder<BZ_SYMS> _bz;

	int _zeroRun;
	std::vector<int> _runList;
	int _runListReadIndex;

	int simulateZeroRun(int run);
	int writeZeroRun(int run, ImageWriter &writer);

	void recordZeroRun();

public:
	CAT_INLINE HuffmanTableEncoder() {
		_zeroRun = 0;
	}
	CAT_INLINE virtual ~HuffmanTableEncoder() {
	}

	void add(u16 symbol);
	void finalize();

	int writeTables(ImageWriter &writer);

	int simulateWrite(u16 symbol);
	void reset();
	int writeSymbol(u16 symbol, ImageWriter &writer);
};


} // namespace cat

#endif // CAT_HUFFMAN_ENCODER_H

