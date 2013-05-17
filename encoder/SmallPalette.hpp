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

#ifndef SMALL_PALETTE_HPP
#define SMALL_PALETTE_HPP

#include "../decoder/Platform.hpp"
#include "../decoder/SmartArray.hpp"
#include "GCIFWriter.h"
#include "ImageWriter.hpp"

#include <vector>
#include <map>

/*
 * Game Closure Small Palette Compression
 *
 * Packing for Small Palette sizes (in colors):
 * 1 = Special case where we do not emit anything
 * 2 = 1 bits/pixel => Packs 4 pixels from current scanline and 4 pixels from next.
 * 3-4 = 2 bits/pixel => Packs 2 pixels from current scanline and 2 from next.
 * 5-16 = 3-4 bits/pixel => Packs 2 pixels from current scanline.
 *
 * For Small Palette Mode, the compressor generates a new palette from the
 * packed pixels and then compresses that as normal.
 *
 * The number of image colors may be affected by masking, so an up-front
 * small palette check is performed.  In small palette mode, the LZ and mask
 * preprocessors go into a special byte-wise mode since the data is not RGBA.
 */

namespace cat {


//// SmallPalette

class SmallPalette {
	static const int SMALL_PALETTE_MAX = 16;

	const GCIFKnobs *_knobs;

	int _size_x, _size_y;	// In pixels
	const u8 *_rgba;		// Original image

	int _pack_x, _pack_y;	// In packed pixels
	SmartArray<u8> _image;	// Repacked image

	int _palette_size;		// Number of palette entries (> 0 : enabled)

	std::vector<u32> _palette;		// Map index => color
	std::map<u32, u16> _map;		// Map color => index

	bool generatePalette();
	void generateImage();

	void writeTable(ImageWriter &writer);

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		int palette_size;
		int overhead_bits;
	} Stats;
#endif

public:
	int init(const u8 *rgba, int size_x, int size_y, const GCIFKnobs *knobs);

	CAT_INLINE bool enabled() {
		return _palette_size > 0;
	}

	CAT_INLINE bool isSingleColor() {
		return _palette_size == 1;
	}

	CAT_INLINE u8 *get() {
		return _image.get();
	}

	CAT_INLINE int getPackX() {
		return _pack_x;
	}

	CAT_INLINE int getPackY() {
		return _pack_y;
	}

	void write(ImageWriter &writer);
#ifdef CAT_COLLECT_STATS
	bool dumpStats();
#else
	CAT_INLINE bool dumpStats() {
		return false;
	}
#endif
};


} // namespace cat

#endif // SMALL_PALETTE_HPP

