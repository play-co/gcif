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

#include "Platform.hpp"
#include "ImageWriter.hpp"
#include "ImageMaskReader.hpp"
#include "Filters.hpp"
#include "HuffmanEncoder.hpp"
#include "GCIFWriter.hpp"

#include <vector>

/*
 * Game Closure Fully-Transparent Alpha Mask Compression
 *
 * Encodes pixels with fully-transparent alpha and/or a dominant image color
 * (often black) as a monochrome bitmap.  This is designed to improve on the
 * compression ratios offered by context modeling or LZ for data that can be
 * compactly represented as a bitmask.  Most graphics files for games have
 * some sort of solid color or transparent background.
 *
 * When fully-transparent alpha masks are transmitted, any color information
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


//#define DUMP_FILTER_OUTPUT


//// ImageMaskWriter

class ImageMaskWriter {
	const GCIFKnobs *_knobs;
	u32 *_alpha;
	u32 *_alpha_filtered;
	u32 *_color;
	u32 *_color_filtered;
	u32 _colorValue;
	int _size, _stride, _width, _height;
	bool _using_encoder;

	void clear();

	void applyFilter();
	void performRLE(std::vector<u8> &rle);
	void performLZ(const std::vector<u8> &rle, std::vector<u8> &lz);
	void writeEncodedLZ(const std::vector<u8> &lz, HuffmanEncoder<256> &encoder, ImageWriter &writer);

	u32 dominantColor();

	void maskAlpha();
	void maskColor(u32 color);

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		u32 alpha_table_bits, alpha_data_bits, alpha_covered;
		int alpha_rleBytes, alpha_lzBytes;
		double alpha_filterUsec, alpha_rleUsec, alpha_lzUsec, alpha_histogramUsec;
		double alpha_generateTableUsec, alpha_tableEncodeUsec, alpha_dataEncodeUsec;
		double alpha_overallUsec;

		u32 color_table_bits, color_data_bits, color_covered;
		int color_rleBytes, color_lzBytes;
		double color_filterUsec, color_rleUsec, color_lzUsec, color_histogramUsec;
		double color_generateTableUsec, color_tableEncodeUsec, color_dataEncodeUsec;
		double color_overallUsec;

		double overallUsec;

		int alpha_compressedDataBits;
		double alpha_compressionRatio;

		int color_compressedDataBits;
		double color_compressionRatio;
	} Stats;
#endif // CAT_COLLECT_STATS

public:
	ImageMaskWriter() {
		_alpha = 0;
		_alpha_filtered = 0;
		_color = 0;
		_color_filtered = 0;
	}
	virtual ~ImageMaskWriter() {
		clear();
	}

	int initFromRGBA(const u8 *rgba, int width, int height, const GCIFKnobs *knobs);

	void write(ImageWriter &writer);

	CAT_INLINE bool masked(int x, int y) {
		const int index = (x >> 5) + y * _stride;
		const u32 word0 = _alpha[index];
		const u32 word1 = _color[index];
		const u32 mask = 1 << (31 - (maskX & 31));
		return ((word0 | word1) & mask) != 0;
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


} // namespace cat

#endif // IMAGE_MASK_WRITER_HPP

