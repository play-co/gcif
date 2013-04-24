#include "EntropyEstimator.hpp"
#include "BitMath.hpp"
#include "Log.hpp"
using namespace cat;

void EntropyEstimator::init() {
	_hist_total = 0;
	CAT_OBJCLR(_hist);
}

u32 EntropyEstimator::entropy(const u8 *symbols, int count) {
	CAT_DEBUG_ENFORCE(count <= SAMPLE_SIZE_MAX);

	// Generate histogram for symbols
	u8 hist[NUM_SYMS] = {0};
	for (int ii = 0; ii < count; ++ii) {
		hist[ii]++;
	}

	// Calculate bits required for symbols
	u8 codelens[NUM_SYMS] = {0};
	u32 bits = 0;
	const u32 total = _hist_total + count;

	// For each symbol,
	for (int ii = 0; ii < count; ++ii) {
		const u8 symbol = symbols[ii];

		// Zeroes are not counted towards entropy since they are the ideal
		if (symbol != 0) {
			// If codelen not determined yet,
			if (!codelens[symbol]) {
				// Get number of instances of this symbol out of total
				u32 inst = _hist[symbol] + hist[symbol];

				// Calculate fixed-point likelihood
				u32 fpLikelihood = ((u64)inst << 24) / total;

				// If it is hugely popular,
				if (fpLikelihood <= 0) {
					// Give it the worst score we can
					codelens[symbol] = 24;
				} else if (fpLikelihood >= 0x800000) {
					// Give it the best score we can above 0
					codelens[symbol] = 1;
				} else if (fpLikelihood >= 0x8000) {
					// Otherwise, find the MSB index
					int msb = BSR32(fpLikelihood);

					// This is quantized log2(likelihood)
					codelens[symbol] = 23 - msb;
				} else {
					// Otherwise, find the MSB index
					int msb = BSR16((u16)fpLikelihood);

					// This is quantized log2(likelihood)
					codelens[symbol] = 24 - msb;
				}
			}

			// Accumulate bits for symbol
			bits += codelens[symbol];
		}
	}

	return bits;
}

void EntropyEstimator::add(const u8 *symbols, int count) {
	_hist_total += count;

	// For each symbol,
	for (int ii = 0; ii < count; ++ii) {
		u8 symbol = symbols[ii];

		// Add it to the global histogram
		_hist[symbol]++;
	}
}

