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

#ifndef CHAOS_METRIC_HPP
#define CHAOS_METRIC_HPP

#include "Platform.hpp"
#include "SmartArray.hpp"

/*
 * Chaos Metric
 *
 * This is a flexible and fast implementation of the BCIF chaos metric,
 * adapted for use in GCIF through some simplifications.
 *
 * The chaos metric is applied after filtering.  So the input data are
 * residual pixels that are mostly zero.  The chaos metric is defined
 * as the sum of the number of bits used to represent the Up and Left
 * residuals near the current pixel position.  It is also limited to
 * a maximum number of "chaos levels."
 *
 * As a result it is only necessary to track a number of residuals
 * equal to the width of the image.  Previous residual is the Left one
 * and current residual is the Up one.  Internally the chaos calculator
 * will also do a table lookup to compute the number of bits set and
 * store that in the row data.
 */

namespace cat {


//// Chaos

// Lookup table version of inline function ResidualScore256() below.
extern const u8 RESIDUAL_SCORE_256[256];


/*
 * Monochrome residuals -> Chaos bin
 */
class MonoChaos {
	static const int TABLE_SIZE = 511; // 0..510: Max sum of two numbers 0..255

public:
	// Residual 0..255 value -> Score
	static CAT_INLINE u8 ResidualScore256(u8 residual) {
#ifdef CAT_ISA_X86
		return RESIDUAL_SCORE_256[residual];
#else
		// If score is "positive",
		if (residual <= 128) {
			// Note: 128 = worst score, 0 = best
			return residual;
		} else {
			// Wrap around: ABS(255) = ABS(-1) = 1, etc
			return 256 - residual;
		}
#endif
	}

	// Residual 0..num_syms-1 value -> Score
	static CAT_INLINE u8 ResidualScore(u8 residual, const u16 num_syms) {
		// If score is "positive",
		if (residual <= num_syms / 2) {
			return residual;
		} else {
			return num_syms - residual;
		}
	}

protected:
	// Chaos Lookup Table: residual score -> chaos bin index
	int _chaos_levels;
	u8 _table[TABLE_SIZE];

	SmartArray<u8> _row;	// Residual scores from last row
	u8 *_pixels;			// = &_row[1]

public:
	CAT_INLINE MonoChaos() {
		_chaos_levels = 0;
	}

	void init(int chaos_levels, int xsize);

	CAT_INLINE int getBinCount() {
		return _chaos_levels;
	}

	CAT_INLINE void start() {
		// Initialize top margin to 0
		_row.fill_00();
	}

	CAT_INLINE void zero(u16 x) {
		_pixels[x] = 0;
	}

	CAT_INLINE void zeroRegion(u16 x, u16 len) {
		u8 * CAT_RESTRICT pixels = _pixels + x;

		while (len >= 4) {
			pixels[0] = 0;
			pixels[1] = 0;
			pixels[2] = 0;
			pixels[3] = 0;
			pixels += 4;
			len -= 4;
		}

		while (len > 0) {
			pixels[0] = 0;
			++pixels;
			--len;
		}
	}

	CAT_INLINE u8 get(u16 x) {
		return _table[_pixels[x-1] + (u16)_pixels[x]];
	}

	CAT_INLINE void store256(u16 x, u8 r) {
		_pixels[x] = ResidualScore256(r);
	}

	CAT_INLINE void store(u16 x, u8 r, u16 num_syms) {
		_pixels[x] = ResidualScore(r, num_syms);
	}
};


/*
 * RGB Residuals -> Chaos bins
 */
class RGBChaos {
	static const int TABLE_SIZE = 511; // 0..510: Max sum of two numbers 0..255

public:
	// Residual 0..255 value -> Score
	static CAT_INLINE u8 ResidualScore(u8 residual) {
#ifdef CAT_ISA_X86
		return RESIDUAL_SCORE_256[residual];
#else
		// If score is "positive",
		if (residual <= 128) {
			// Note: 128 = worst score, 0 = best
			return residual;
		} else {
			// Wrap around: ABS(255) = ABS(-1) = 1, etc
			return 256 - residual;
		}
#endif
	}

protected:
	// Chaos Lookup Table: residual score -> chaos bin index
	int _chaos_levels;
	u8 _table[TABLE_SIZE];

	SmartArray<u8> _row;	// Residual scores from last row
	u8 *_pixels;			// = &_row[1]

public:
	CAT_INLINE RGBChaos() {
		_chaos_levels = 0;
	}

	void init(int chaos_levels, int xsize);

	CAT_INLINE int getBinCount() {
		return _chaos_levels;
	}

	CAT_INLINE void start() {
		_row.fill_00();
	}

	CAT_INLINE void zero(u16 x) {
		*(u32*)&_pixels[x << 2] = 0;
	}

	CAT_INLINE void zeroRegion(u16 x, u16 len) {
		u32 * CAT_RESTRICT pixels = reinterpret_cast<u32 * CAT_RESTRICT>( &_pixels[x << 2] );

		while (len >= 4) {
			pixels[0] = 0;
			pixels[1] = 0;
			pixels[2] = 0;
			pixels[3] = 0;
			pixels += 4;
			len -= 4;
		}

		while (len > 0) {
			pixels[0] = 0;
			++pixels;
			--len;
		}
	}

	CAT_INLINE void get(u16 x, u8 & CAT_RESTRICT y, u8 & CAT_RESTRICT u, u8 & CAT_RESTRICT v) {
		u8 * CAT_RESTRICT pixels = &_pixels[x << 2];
		u32 pixel = *(u32*)pixels;
		u32 last = *(u32*)(pixels - 4);

		y = _table[(pixel >> 24) + (last >> 24)];
		u = _table[(u8)(pixel >> 16) + (u16)(u8)(last >> 16)];
		v = _table[(u16)(pixel + last)];
	}

	CAT_INLINE void store(u16 x, const u8 * CAT_RESTRICT yuv) {
		u32 * CAT_RESTRICT pixels = (u32 *)&_pixels[x << 2];
		*pixels = (ResidualScore(yuv[0]) << 24) | (ResidualScore(yuv[1]) << 16) | ResidualScore(yuv[2]);
	}
};


} // namespace cat

#endif // CHAOS_METRIC_HPP

