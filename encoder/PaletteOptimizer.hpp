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

#ifndef PALETTE_OPTIMIZER_HPP
#define PALETTE_OPTIMIZER_HPP

#include "../decoder/Platform.hpp"
#include "../decoder/SmartArray.hpp"
#include "../decoder/Delegates.hpp"

/*
 * Image Compression: Palette Sorting
 *
 * Choosing the palette index for each of the <= 256 colors is essential for
 * producing good compression results using a PNG-like filter-based approach.
 *
 * Palette index assignment does not affect LZ or mask results, nor any
 * direct improvement in entropy encoding.
 *
 * However, when neighboring pixels have similar values, the filters are more
 * effective at predicting them, which increases the number of post-filter zero
 * pixels and reduces overall entropy.
 *
 * A simple approximation to good choices is to just sort by luminance, so the
 * brighest pixels get the highest palette index.  However you can do better,
 * and luminance cannot be measured for filter matrices.
 *
 * Since this is designed to improve filter effectiveness, the criterion for a
 * good palette selection is based on how close each pixel index is to its up,
 * up-left, left, and up-right neighbor pixel indices.  If you also include the
 * reverse relation, all 8 pixels around the center pixel should be scored.
 *
 * The algorithm is:
 * (1) Assign each palette index by popularity, most popular gets index 0.
 * (2) From palette index 1:
 * *** (1) Score each color by how often palette index 0 appears in filter zone.
 * *** (2) Add in how often the color appears in index 0's filter zone.
 * *** (3) Choose the one that scores highest to be index 1.
 * (3) For palette index 2+, score by filter zone closeness for index 0 and 1.
 * (4) After index 8, it cares about closeness to the last 8 indices only.
 *
 * The closeness to the last index is more important than earlier indices, so
 * those are scored higher.  Also, left/right neighbors are scored twice as
 * high as other neighbors, matching the natural horizontal correlation of
 * most images.
 */

namespace cat {


//// PaletteOptimizer

class PaletteOptimizer {
public:
	static const int PALETTE_MAX = 256;

	// bool IsMasked(u16 x, u16 y)
	typedef Delegate2<bool, u16, u16> MaskDelegate;

protected:
	// Input
	int _palette_size;
	const u8 *_image;			// Indexed image
	int _xsize, _ysize;		// Image size in pixels

	// Working state
	u32 _hist[PALETTE_MAX];		// Image histogram
	u8 _most_common;

	// Output
	SmartArray<u8> _result;
	u8 _forward[PALETTE_MAX];	// Map old index -> new index

	void histogramImage(MaskDelegate &mask);
	void sortPalette(MaskDelegate &mask);

public:
	// Assumes image data is entirely in 0..palette_size-1 range
	void process(const u8 *_image, int xsize, int ysize, int palette_size, MaskDelegate mask);

	CAT_INLINE const u8 *getOptimizedImage() {
		return _result.get();
	}

	CAT_INLINE u8 forward(u8 index) {
		return _forward[index];
	}
};


} // namespace cat

#endif // PALETTE_OPTIMIZER_HPP

