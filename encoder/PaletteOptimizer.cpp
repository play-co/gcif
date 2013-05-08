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

#include "PaletteOptimizer.hpp"
#include "Log.hpp"
using namespace cat;


void PaletteOptimizer::histogramImage() {
	CAT_OBJCLR(_hist);

	// Histogram
	const u8 *image = _image;
	for (int y = 0, yend = _size_y; y < yend; ++y) {
		for (int x = 0, xend = _size_x; x < xend; ++x, ++image) {
			_hist[*image]++;
		}
	}

	// Find most common
	int best_ii = 0;
	u32 best_count = 0;
	for (int ii = 0; ii < PALETTE_MAX; ++ii) {
		u32 count = _hist[ii];

		if (count > 0) {
			if (best_count < count) {
				best_count = count;
				best_ii = ii;
			}
		}
	}
	_most_common = (u8)best_ii;
}

void PaletteOptimizer::sortPalette() {
	CAT_OBJCLR(_forward);

	// Index 0 is the most common color
	//_forward[_most_common] = 0;

	// Temporary matrix to store scoring data
	_result.resize(_size_x * _size_y);
	_result.fill_ff();

	// For each remaining index,
	for (int index = 1; index < _palette_size; ++index) {
		// Score each of the remaining palette indices
		u8 *result = _result.get();
		const u8 *image = _image;
		u32 scores[PALETTE_MAX] = {0};
		static const int THRESH = 8;
		const int cutoff = index - THRESH;
		for (int y = 0, yend = _size_y - 1; y <= yend; ++y) {
			for (int x = 0, xend = _size_x - 1; x <= xend; ++x, ++result, ++image) {
				u8 p = *result;
				p = (u8)((int)p - cutoff);

				if (p < THRESH) {
					++p;
					/*
					 * C B D
					 * A x a
					 * d b c
					 */
					if (x > 0) {
						scores[image[-1]] += p; // A

						if (y < yend) {
							scores[image[xend]] += p; // d
						}

						if (y > 0) {
							scores[image[-xend - 2]] += p; // C
						}
					}
					if (x < xend) {
						scores[image[1]] += p; // a

						if (x < xend) {
							scores[image[xend + 2]] += p; // c
						}
					}
					if (y > 0) {
						scores[image[-xend - 1]] += p; // B

						if (x < xend) {
							scores[image[-xend]] += p; // D
						}
					}
					if (y < yend) {
						scores[image[xend + 1]] += p; // b
					}
				}
			}
		}

		// Find best score
		int best_ii = 0;
		u32 best_score = 0;
		for (int ii = 0; ii < _palette_size; ++ii) {
			// If index is not already claimed,
			if (ii != _most_common && _forward[ii] == 0) {
				u32 score = scores[ii];

				if (best_score <= score) {
					best_score = score;
					best_ii = ii;
				}
			}
		}

		// Insert it
		_forward[best_ii] = (u8)index;

		// Fill in score matrix
		result = _result.get();
		image = _image;
		for (int y = 0, yend = _size_y; y < yend; ++y) {
			for (int x = 0, xend = _size_x; x < xend; ++x, ++result, ++image) {
				// If original image used this one,
				if (*image == best_ii) {
					*result = index;
				}
			}
		}
	}

#ifdef CAT_DEBUG
	// Sanity check
	const u8 *result = _result.get();
	for (int y = 0, yend = _size_y; y < yend; ++y) {
		for (int x = 0, xend = _size_x; x < xend; ++x, ++result) {
			CAT_DEBUG_ENFORCE(*result < _palette_size);
		}
	}
#endif
}

	/*
	 * TODO
	 *
	 * For Palette input, the LZ data should be same as original, and the
	 * masked data should be set to same as original unless the palette index
	 * does not exist, in which case the most common value should be used.
	 * In this case the data does not need a mask function since all input will
	 * be inside the palette space.
	 *
	 * For MonoWriter output, it is 0..n-1 or 255 for masked out.  This is bad
	 * for recursive compression.  Instead of emitting 255 we should emit same
	 * as left value, defaulting to most common; will be zero after this code.
	 *
	 * For SF/CF matrices, it is also set to 255 for masked out.  We should do
	 * the same and use same-as-left.
	 *
	 * After changing these things, the Palette Optimizer will not need a mask
	 * and all the input will be in the palette space.
	 */

void PaletteOptimizer::process(const u8 *image, int size_x, int size_y, int palette_size) {
	_image = image;
	_size_x = size_x;
	_size_y = size_y;
	_palette_size = palette_size;

	histogramImage();
	sortPalette();
}

