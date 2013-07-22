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
 *
 * Alternatively, if normal Huffman encoding is more effective, it is used,
 * making this class much more generic.
 */

namespace cat {


//// EntropyEncoder

class EntropyEncoder {
	int _num_syms, _zrle_syms;
	int _bz_syms; // NUM_SYMS + ZRLE_SYMS
	int _az_syms; // NUM_SYMS
	int _bz_tail_sym; // BZ_SYMS - 1

	FreqHistogram _bz_hist;
	FreqHistogram _az_hist;
	HuffmanEncoder _bz;
	HuffmanEncoder _az;

	FreqHistogram _basic_hist;
	std::vector<u16> _basic_syms;
	HuffmanEncoder _basic;
	bool _using_basic;

	int _zeroRun;
	std::vector<int> _runList;
	int _runListReadIndex;
	int _zeroCost;

#ifdef CAT_DEBUG
	int _basic_recall;
#endif

	int simulateZeroRun(int run);
	int writeZeroRun(int run, ImageWriter &writer);

public:
	static const u16 FAKE_ZERO = 0xfffe;

	CAT_INLINE bool InZeroRun() {
		return _zeroRun > 0;
	}

	void init(int num_syms, int zrle_syms);
	void add(u16 symbol);
	int finalize();

	CAT_INLINE void reset() {
		_zeroRun = 0;

		// Set the run list read index for writing
		_runListReadIndex = 0;

#ifdef CAT_DEBUG
		_basic_recall = 0;
#endif
	}

	// Be sure to reset before/after simulation
	int price(u16 symbol); // in bits (for zRLE = average bits >= 1)

	int writeTables(ImageWriter &writer);
	int write(u16 symbol, ImageWriter &writer);
};


} // namespace cat

#endif // ENTROPY_ENCODER_HPP

