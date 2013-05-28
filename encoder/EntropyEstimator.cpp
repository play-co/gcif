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

#include "EntropyEstimator.hpp"
#include "../decoder/BitMath.hpp"
#include "Log.hpp"
#include "../decoder/Enforcer.hpp"
using namespace cat;


//// EntropyEstimator

static u32 calculateEntropy(u32 inst, u32 total) {
	// Calculate fixed-point likelihood
	u32 fpLikelihood = ((u64)inst << 24) / total;

	if (fpLikelihood <= 0) {
		// Very unlikely: Give it the worst score we can
		return 24;
	} else if (fpLikelihood >= 0x800000) {
		// Very likely: Give it the best score we can above 0
		return 1;
	} else if (fpLikelihood >= 0x8000) {
		// Find MSB index using assembly code instruction intrinsic
		int msb = BSR32(fpLikelihood);

		// This is quantized log2(likelihood)
		return 23 - msb;
	} else {
		// Adapted from the Stanford Bit Twiddling Hacks collection
		u32 shift, r, x = fpLikelihood;
		r = (x > 0xFF) << 3; x >>= r;
		shift = (x > 0xF) << 2; x >>= shift; r |= shift;
		shift = (x > 0x3) << 1; x >>= shift; r |= shift;
		r |= (x >> 1);

		// This is quantized log2(likelihood)
		return 24 - r;
	}
}

void EntropyEstimator::init() {
	_hist_total = 0;
	CAT_OBJCLR(_hist);
}

void EntropyEstimator::add(const u8 * CAT_RESTRICT symbols, int count) {
	// Update histogram total count
	_hist_total += count;

	// For each symbol,
	for (int ii = 0; ii < count; ++ii) {
		u8 symbol = symbols[ii];

		if (symbol > 0) {
			// Add it to the global histogram
			_hist[symbol]++;
		}
	}
}

void EntropyEstimator::subtract(const u8 * CAT_RESTRICT symbols, int count) {
	// Update histogram total count
	_hist_total -= count;

	// For each symbol,
	for (int ii = 0; ii < count; ++ii) {
		u8 symbol = symbols[ii];

		if (symbol > 0) {
			// Subtract it from the global histogram
			_hist[symbol]--;
		}
	}
}

u32 EntropyEstimator::entropy(const u8 * CAT_RESTRICT symbols, int count) {
	if (count == 0) {
		return 0;
	}

	u32 hist[NUM_SYMS] = {0};

	// Generate histogram for symbols
	for (int ii = 0; ii < count; ++ii) {
		const u8 symbol = symbols[ii];

		if (symbol > 0) {
			hist[symbol]++;
		}
	}

	// Calculate bits required for symbols
	u8 codelens[NUM_SYMS] = {0};
	u32 bits = 0;
	const u32 total = _hist_total + count;

	// If the total is nonzero,
	if (total > 0) {
		// For each symbol,
		for (int ii = 0; ii < count; ++ii) {
			const u8 symbol = symbols[ii];

			// Zeroes are not counted towards entropy since they are the ideal
			if (symbol > 0) {
				// If codelen not determined yet,
				if (!codelens[symbol]) {
					// Get number of instances of this symbol out of total
					u32 inst = _hist[symbol] + hist[symbol];

					// Calculate codelen
					codelens[symbol] = calculateEntropy(inst, total);
				}

				// Accumulate bits for symbol
				bits += codelens[symbol];
			}
		}
	}

	return bits;
}

