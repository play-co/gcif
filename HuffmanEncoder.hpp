#ifndef CAT_HUFFMAN_ENCODER_H
#define CAT_HUFFMAN_ENCODER_H

#include "Platform.hpp"
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

bool generate_huffman_codes(huffman_work_tables *state, u32 num_syms, const u16 *pFreq, u8 *pCodesizes, u32 &max_code_size, u32 &total_freq_ret);

bool limit_max_code_size(u32 num_syms, u8 *pCodesizes, u32 max_code_size);

bool generate_codes(u32 num_syms, const u8 *pCodesizes, u16 *pCodes);


} // namespace huffman


void normalizeFreqs(u32 max_freq, int num_syms, u32 hist[], u16 freqs[]);
void collectArrayFreqs(int num_syms, int data_size, u8 data[], u16 freqs[]);
void collectFreqs(int num_syms, const std::vector<u8> &lz, u16 freqs[]);
void generateHuffmanCodes(int num_syms, u16 freqs[], u16 codes[], u8 codelens[]);


// Convenience class
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

	CAT_INLINE void generateHuffman(u16 codes[], u8 codelens[]) {
		u16 freqs[NUM_SYMS];
		normalizeFreqs(max_freq, NUM_SYMS, hist, freqs);
		generateHuffmanCodes(NUM_SYMS, freqs, codes, codelens);
	}
};


} // namespace cat

#endif // CAT_HUFFMAN_ENCODER_H

