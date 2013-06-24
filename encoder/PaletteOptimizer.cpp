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


void PaletteOptimizer::histogramImage(MaskDelegate &mask) {
	CAT_OBJCLR(_hist);

	// Histogram
	const u8 *image = _image;
	for (int y = 0, yend = _size_y; y < yend; ++y) {
		for (int x = 0, xend = _size_x; x < xend; ++x, ++image) {
			if (!mask(x, y)) {
				_hist[*image]++;
			}
		}
	}

	int palette_size = 0;

	// Find most common
	int best_ii = 0;
	u32 best_count = _hist[0];

	if (best_count > 0) {
		++palette_size;
	}

	for (int ii = 1; ii < PALETTE_MAX; ++ii) {
		u32 count = _hist[ii];

		if (count > 0) {
			++palette_size;

			if (best_count < count) {
				best_count = count;
				best_ii = ii;
			}
		}
	}

	_most_common = (u8)best_ii;

	CAT_DEBUG_ENFORCE(_palette_size >= palette_size);

#ifdef CAT_DUMP_FILTERS
	if (_palette_size != palette_size) {
		CAT_INFO("pal") << "Palette optimizer noticed the data uses " << palette_size << " palette indices of " << _palette_size;
	}
#endif
}

void PaletteOptimizer::sortPalette(MaskDelegate &mask) {
	CAT_OBJCLR(_forward);

	// Index 0 is the most common color
	//_forward[_most_common] = 0;

	// Temporary matrix to store scoring data
	_result.resize(_size_x * _size_y);
	_result.fill_ff();

	// Fill in score matrix for most common color
	u8 *result = _result.get();
	const u8 *image = _image;
	for (int y = 0, yend = _size_y; y < yend; ++y) {
		for (int x = 0, xend = _size_x; x < xend; ++x, ++result, ++image) {
			// If original image used this one,
			if (*image == _most_common) {
				*result = 0;
			}
		}
	}

	// For each remaining index,
	for (int index = 1; index < _palette_size; ++index) {
		// Score each of the remaining palette indices
		result = _result.get();
		image = _image;
		u32 scores[PALETTE_MAX] = {0};
		static const int THRESH = 8;
		const int cutoff = index - THRESH;
		for (int y = 0, size_y = _size_y; y < size_y; ++y) {
			for (int x = 0, size_x = _size_x; x < size_x; ++x, ++result, ++image) {
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
						if (!mask(x - 1, y)) {
							scores[image[-1]] += p*2; // A
						}

						if (y < size_y-1) {
							if (!mask(x - 1, y + 1)) {
								scores[image[size_x - 1]] += p; // d
							}
						}

						if (y > 0) {
							if (!mask(x - 1, y - 1)) {
								scores[image[-size_x - 1]] += p; // C
							}
						}
					}
					if (x < size_x-1) {
						if (!mask(x + 1, y)) {
							scores[image[1]] += p*2; // a
						}

						if (y < size_y-1) {
							if (!mask(x + 1, y + 1)) {
								scores[image[size_x + 1]] += p; // c
							}
						}
					}
					if (y > 0) {
						if (!mask(x, y - 1)) {
							scores[image[-size_x]] += p; // B
						}

						if (x < size_x-1) {
							if (!mask(x + 1, y - 1)) {
								scores[image[-size_x + 1]] += p; // D
							}
						}
					}
					if (y < size_y-1) {
						if (!mask(x, y + 1)) {
							scores[image[size_x]] += p; // b
						}
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

	// Leaving masked result pixels as zero (same as most common -- seems wise)

#ifdef CAT_DEBUG
	// Sanity check
	result = _result.get();
	for (int y = 0, yend = _size_y; y < yend; ++y) {
		for (int x = 0, xend = _size_x; x < xend; ++x, ++result) {
			CAT_DEBUG_ENFORCE(*result < _palette_size);
		}
	}
#endif
}

void PaletteOptimizer::process(const u8 *image, int size_x, int size_y, int palette_size, MaskDelegate mask) {
	_image = image;
	_size_x = size_x;
	_size_y = size_y;
	_palette_size = palette_size;

	histogramImage(mask);
	sortPalette(mask);
}

