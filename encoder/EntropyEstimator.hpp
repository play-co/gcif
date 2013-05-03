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

#ifndef ENTROPY_ESTIMATOR_HPP
#define ENTROPY_ESTIMATOR_HPP

#include "../decoder/Platform.hpp"

namespace cat {


/*
 * Entropy Estimator
 *
 * Since the GCIF image codec uses entropy encoding as its final step, it is a
 * good idea to make decisions that result in the lowest entropy possible to
 * yield smaller files.
 *
 * Since file size is proportional to entropy, a good estimation of entropy is
 * the number of bits required to represent a given set of data.  This can be
 * approximated efficiently without floating-point math.
 *
 * Since encoded symbols take from 1..16 bits to represent, the number of bits
 * required for a symbol is equal to the base-2 logarithm (or BSR16 highest set
 * bit) of the likelihood of that symbol scaled to fit in 16 bits.
 *
 * Likelihood is defined as the number of times a symbol occurs divided by
 * the total number of occurrences of all symbols.
 */

class EntropyEstimator {
	static const int NUM_SYMS = 256;

	// Running histogram
	u32 _hist[NUM_SYMS];
	u32 _hist_total;

public:
	CAT_INLINE EntropyEstimator() {
	}
	CAT_INLINE virtual ~EntropyEstimator() {
	}

	void init();

	// Calculate entropy of given symbols (lower = better)
	u32 entropy(const u8 *symbols, int count);

	// Add symbols to running histogram
	void add(const u8 *symbols, int count);

	// Subtract symbols from running histogram
	void subtract(const u8 *symbols, int count);
};



} // namespace cat

#endif // ENTROPY_ESTIMATOR_HPP

