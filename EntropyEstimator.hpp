#ifndef ENTROPY_ESTIMATOR_HPP
#define ENTROPY_ESTIMATOR_HPP

#include "Platform.hpp"

namespace cat {


/*
 * Entropy Estimator
 *
 * Since the GCIF image codec uses entropy encoding as its final step, it is a
 * good idea to make decisions that result in the lowest entropy possible to
 * yield smaller files.  Estimating entropy is interesting.
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
};



} // namespace cat

#endif // ENTROPY_ESTIMATOR_HPP

