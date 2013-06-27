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

#ifndef IMAGE_MASK_WRITER_HPP
#define IMAGE_MASK_WRITER_HPP

#include "../decoder/Platform.hpp"
#include "ImageWriter.hpp"
#include "../decoder/ImageMaskReader.hpp"
#include "../decoder/Filters.hpp"
#include "HuffmanEncoder.hpp"
#include "GCIFWriter.h"
#include "../decoder/SmartArray.hpp"

#include <vector>

/*
 * Game Closure Dominant Color Mask Compression
 *
 * Encodes pixels with a dominant image color (often black or transparent) as
 * a monochrome bitmap.  This is designed to improve on the compression ratio
 * offered by context modeling or LZ for data that can be compactly represented
 * as a bitmask.  Most graphics files for games have some sort of solid color
 * or transparent background.
 *
 * When fully-transparent alpha masks are transmitted, any RGB information
 * stored in the fully-transparent pixels is lost.
 *
 * It first performs bitwise filtering to reduce the data down to a few pixels.
 * Then the distance between those pixels is recorded, compressed with LZ4HC,
 * and entropy-encoded for compression with Huffman codes.
 *
 * This method is chosen if a minimum compression ratio is achieved for the
 * masked pixels.
 */

namespace cat {


/*
 * Support class that does the heavy lifting for each mask layer
 *
 * Accepts a color to match and then dutifully compresses it as a bitmask.
 * If the result achieves a specified compression ratio, then it is written.
 * Otherwise a single bit is sent indicating it was not enabled to the decoder.
 */

class Masker {
	static const int MIN_SIZE = 16;

	const GCIFKnobs *_knobs;

	bool _enabled;			// Is this mask layer enabled?

	SmartArray<u32> _mask, _filtered;
	const u8 *_rgba;		// Original pixel data
	int _size, _stride, _size_x, _size_y, _planes;
	u32 _covered;			// Number of pixels covered

	bool _using_encoder;	// Above threshold for encodering?
	int _min_ratio;			// Minimum compression ratio to achieve

	u32 _color;				// Color value for match
	u32 _color_mask;		// Portion of color value that must match

	std::vector<u8> _lz, _rle;
	HuffmanEncoder _encoder; // 256 symbols

	void applyFilter();
	void performRLE();
	void performLZ();
	void writeEncodedLZ(ImageWriter &writer);
	u32 simulate();

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		u32 table_bits, data_bits, covered;
		int rleBytes, lzBytes;
		double filterUsec, rleUsec, lzUsec, histogramUsec;
		double generateTableUsec, tableEncodeUsec, dataSimulateUsec, dataEncodeUsec;
		double overallUsec;

		int compressedDataBits;
		double compressionRatio;
	} Stats;
#endif // CAT_COLLECT_STATS

public:
	CAT_INLINE bool enabled() {
		return _enabled;
	}

	CAT_INLINE u32 getColor() {
		return _color;
	}

	// Create mask
	int init(const u8 *rgba, int planes, u32 color, u32 color_mask, int size_x, int size_y, const GCIFKnobs *knobs, int min_ratio);

	// Evaluate compression ratio
	bool evaluate();

	// Write result
	void write(ImageWriter &writer);

	CAT_INLINE bool masked(int x, int y) {
		if (!_enabled) {
			return false;
		}
		const int index = (x >> 5) + y * _stride;
		const u32 word = _mask[index];
		const u32 mask = 1 << (31 - (x & 31));
		return (word & mask) != 0;
	}

#ifdef CAT_COLLECT_STATS
	bool dumpStats();
#else
	CAT_INLINE bool dumpStats() {
		// Not implemented
		return false;
	}
#endif
};


//// ImageMaskWriter

class ImageMaskWriter {
	const GCIFKnobs *_knobs;

	const u8 *_rgba;
	int _size_x, _size_y;
	int _planes;

	Masker _color;

	u32 dominantRGBA();	// 4-plane mode
	u8 dominantMono();	// 1-plane mode

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		int compressedDataBits;
		double overallUsec;
	} Stats;
#endif // CAT_COLLECT_STATS

public:
	int init(const u8 *rgba, int planes, int size_x, int size_y, const GCIFKnobs *knobs);

	void write(ImageWriter &writer);

	CAT_INLINE bool enabled() {
		return _color.enabled();
	}

	CAT_INLINE u32 getColor() {
		return _color.getColor();
	}

	CAT_INLINE bool masked(int x, int y) {
		return _color.masked(x, y);
	}

	CAT_INLINE bool dumpStats() {
		return _color.dumpStats();
	}
};


} // namespace cat

#endif // IMAGE_MASK_WRITER_HPP

