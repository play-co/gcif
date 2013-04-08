#ifndef ENTROPY_ESTIMATOR_HPP
#define ENTROPY_ESTIMATOR_HPP

#include "Platform.hpp"
#include <cmath>

namespace cat {


template<class LType> class EntropyEstimator {
	int _num_syms;

	u32 *_global;
	u32 _globalTotal;

	LType *_best;
	u32 _bestTotal;

	LType *_local;
	u32 _localTotal;

	void cleanup() {
		if (_global) {
			delete []_global;
			_global = 0;
		}
		if (_best) {
			delete []_best;
			_best = 0;
		}
		if (_local) {
			delete []_local;
			_local = 0;
		}
	}

public:
	CAT_INLINE EntropyEstimator() {
		_global = 0;
		_best = 0;
		_local = 0;
	}
	CAT_INLINE virtual ~EntropyEstimator() {
		cleanup();
	}

	void clear(int num_syms) {
		cleanup();

		_num_syms = num_syms;
		_global = new u32[num_syms];
		_best = new LType[num_syms];
		_local = new LType[num_syms];

		_globalTotal = 0;
		CAT_CLR(_global, _num_syms * sizeof(u32));
	}

	void setup() {
		_localTotal = 0;
		CAT_CLR(_local, _num_syms * sizeof(_local[0]));
	}

	void push(s16 symbol) {
		_local[symbol]++;
		++_localTotal;
	}

	double entropy() {
		double total = _globalTotal + _localTotal;
		double e = 0;
		static const double log2 = log(2.);

		for (int ii = 0, num_syms = _num_syms; ii < num_syms; ++ii) {
			u32 count = _global[ii] + _local[ii];
			if (count > 0) {
				double freq = count / total;
				e -= freq * log(freq) / log2;
			}
		}

		return e;
	}

#if 0
	void drawHistogram(u8 *rgba, int width) {
		double total = _globalTotal + _localTotal;
		double e = 0;
		static const double log2 = log(2.);

		for (int ii = 0, num_syms = _num_syms; ii < num_syms; ++ii) {
			u32 count = _global[ii] + _local[ii];
			double freq;
			if (count > 0) {
				freq = count / total;
				e -= freq * log(freq) / log2;
			} else {
				freq = 0;
			}

			int r, g, b;
			r = 255;
			g = 0;
			b = 0;

			if (ii > 127) g = 255;
			if (ii > 255) b = 255;

			int bar = 200 * freq;
			for (int jj = 0; jj < bar; ++jj) {
				rgba[ii * width * 4 + jj * 4] = r;
				rgba[ii * width * 4 + jj * 4 + 1] = g;
				rgba[ii * width * 4 + jj * 4 + 2] = b;
			}
			for (int jj = bar; jj < 200; ++jj) {
				rgba[ii * width * 4 + jj * 4] = 0;
				rgba[ii * width * 4 + jj * 4 + 1] = 0;
				rgba[ii * width * 4 + jj * 4 + 2] = 0;
			}
		}
	}
#endif

	void save() {
		memcpy(_best, _local, _num_syms * sizeof(_best[0]));
		_bestTotal = _localTotal;
	}

	void commit() {
		for (int ii = 0, num_syms = _num_syms; ii < num_syms; ++ii) {
			_global[ii] += _best[ii];
		}
		_globalTotal += _bestTotal;
	}
};



} // namespace cat

#endif // ENTROPY_ESTIMATOR_HPP

