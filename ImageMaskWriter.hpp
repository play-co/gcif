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
 * Encodes pixels with fully-transparent alpha as a monochrome bitmap.
 *
 * It first performs bitwise filtering to reduce the data down to a few pixels.
 * Then the distance between those pixels is recorded, compressed with LZ4HC,
 * and then Huffman encoded.
 */

namespace cat {


//#define DUMP_FILTER_OUTPUT


//// ImageMaskWriter

class ImageMaskWriter {
	static const int HUFF_THRESH = ImageMaskReader::HUFF_THRESH;

	u32 _value;

	const GCIFKnobs *_knobs;
	u32 *_mask;
	u32 *_filtered;
	int _size, _stride, _width, _height;

	void clear();

	void applyFilter();
	void performRLE(std::vector<u8> &rle);
	void performLZ(const std::vector<u8> &rle, std::vector<u8> &lz);
	void writeEncodedLZ(const std::vector<u8> &lz, HuffmanEncoder<256> &encoder, ImageWriter &writer);

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		u32 table_bits;
		u32 data_bits;
		u32 covered;

		int rleBytes, lzBytes;

		double filterUsec, rleUsec, lzUsec, histogramUsec;
		double generateTableUsec, tableEncodeUsec, dataEncodeUsec;
		double overallUsec;

		int compressedDataBits;

		double compressionRatio;
	} Stats;
#endif // CAT_COLLECT_STATS

public:
	ImageMaskWriter() {
		_mask = 0;
		_filtered = 0;
	}
	virtual ~ImageMaskWriter() {
		clear();
	}

	int initFromRGBA(const u8 *rgba, int width, int height, const GCIFKnobs *knobs);

	void write(ImageWriter &writer);

	CAT_INLINE bool hasRGB(int x, int y) {
		const int maskX = x;
		const int maskY = y;
		const u32 word = _mask[(maskX >> 5) + maskY * _stride];
		return ((word << (maskX & 31)) >> 31) != 0;
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

