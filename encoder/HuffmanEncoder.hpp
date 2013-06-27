/*
	Copyright (c) 2013 Game Closure.  All rights reserved.

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

#ifndef CAT_HUFFMAN_ENCODER_H
#define CAT_HUFFMAN_ENCODER_H

#include "../decoder/Platform.hpp"
#include "../decoder/HuffmanDecoder.hpp"
#include "ImageWriter.hpp"
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


struct sym_freq
{
	u32 freq;
	u16 left, right;

	CAT_INLINE bool operator<(const sym_freq &other) const
	{
		return freq > other.freq;
	}
};


class HuffmanWorkTables {
	SmartArray<sym_freq> syms0, syms1;

public:
	void init(int num_syms) {
		int size = num_syms * 2 + 1;

		syms0.resize(size);
		syms1.resize(size);
	}

	CAT_INLINE sym_freq &get0(int x) {
		return syms0[x];
	}

	CAT_INLINE sym_freq &get1(int x) {
		return syms1[x];
	}

	CAT_INLINE sym_freq *ptr0() {
		return &syms0[0];
	}

	CAT_INLINE sym_freq *ptr1() {
		return &syms1[0];
	}
};


//// Huffman functions from LZHAM

// Modified: one_sym is set as nonzero symbol+1 when there is just one symbol to avoid writing bits for these
bool generate_huffman_codes(HuffmanWorkTables &state, u32 num_syms, const u16 *pFreq, u8 *pCodesizes,
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

class FreqHistogram {
public:
	SmartArray<u32> hist;
	u32 max_freq;

	CAT_INLINE void init(int num_syms) {
		hist.resize(num_syms);
		hist.fill_00();
		max_freq = 0;
	}

	CAT_INLINE int size() {
		return hist.size();
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
		for (u32 sym = 1, num_syms = hist.size(); sym < num_syms; ++sym) {
			// If it is higher,
			if (highest < hist[sym]) {
				// Remember it well
				highest = hist[sym];
				peak_sym = sym;
			}
		}

		return peak_sym;
	}

	CAT_INLINE void normalize(u16 *freqs, int num_syms) {
		CAT_DEBUG_ENFORCE(num_syms == hist.size());

		normalizeFreqs(max_freq, num_syms, hist.get(), freqs);
	}
};


//// HuffmanEncoder

class HuffmanEncoder {
public:
	SmartArray<u16> _codes;
	SmartArray<u8> _codelens;
	u32 _one_sym;

	CAT_INLINE bool init(FreqHistogram &hist) {
		SmartArray<u16> freqs;
		freqs.resize(hist.size());

		hist.normalize(freqs.get(), freqs.size());

		return init(freqs.get(), freqs.size());
	}

	CAT_INLINE bool init(u16 freqs[], int num_syms) {
		_codes.resize(num_syms);
		_codelens.resize(num_syms);

		if (!initCodelens(freqs, num_syms)) {
			CAT_EXCEPTION();
			return false;
		}

		initCodes();
		return true;
	}

	// Split this up because you can simulate writes without generating full codes
	CAT_INLINE bool initCodelens(u16 freqs[], int num_syms) {
		_codes.resize(num_syms);
		_codelens.resize(num_syms);

		huffman::HuffmanWorkTables state;
		u32 max_code_size, total_freq;

		if (!huffman::generate_huffman_codes(state, num_syms, freqs, _codelens.get(), max_code_size, total_freq, _one_sym)) {
			CAT_EXCEPTION();
			return false;
		}

		if (!_one_sym) {
			if (max_code_size > HuffmanDecoder::MAX_CODE_SIZE) {
				huffman::limit_max_code_size(num_syms, _codelens.get(), HuffmanDecoder::MAX_CODE_SIZE);
			}
		}

		return true;
	}
	CAT_INLINE void initCodes() {
		if (!_one_sym) {
			huffman::generate_codes(_codelens.size(), _codelens.get(), _codes.get());
		} else {
			_codelens.fill_00();
			_codelens[_one_sym - 1] = 1;
		}
	}

	CAT_INLINE int writeTable(ImageWriter &writer) {
		return writeCompressedHuffmanTable(_codelens.size(), _codelens.get(), writer);
	}

	CAT_INLINE int writeSymbol(u32 sym, ImageWriter &writer) {
		const u32 one_sym = _one_sym;

		if (one_sym) {
			return 0;
		} else {
			u16 code = _codes[sym];
			u8 codelen = _codelens[sym];
			CAT_DEBUG_ENFORCE(codelen > 0);

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
			CAT_DEBUG_ENFORCE(codelen > 0);
			return codelen;
		}
	}
};


//// HuffmanTableEncoder

class HuffmanTableEncoder {
public:
	static const int NUM_SYMS = HuffmanDecoder::MAX_CODE_SIZE + 1;
	static const int BZ_SYMS = NUM_SYMS;

protected:
	FreqHistogram _bz_hist;

	HuffmanEncoder _bz;

	int _zeroRun;
	std::vector<int> _runList;
	int _runListReadIndex;

	int simulateZeroRun(int run);
	int writeZeroRun(int run, ImageWriter &writer);

	void recordZeroRun();

public:
	void init();
	void add(u16 symbol);
	void finalize();

	int writeTables(ImageWriter &writer);

	int simulateWrite(u16 symbol);
	void reset();
	int writeSymbol(u16 symbol, ImageWriter &writer);
};


} // namespace cat

#endif // CAT_HUFFMAN_ENCODER_H

