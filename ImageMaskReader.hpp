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

#ifndef IMAGE_MASK_READER_HPP
#define IMAGE_MASK_READER_HPP

#include "Platform.hpp"
#include "ImageReader.hpp"
#include "Filters.hpp"

/*
 * Game Closure Fully-Transparent Alpha Mask Decompression
 */

namespace cat {


//// ImageMaskReader

class ImageMaskReader {
	u32 *_mask;

	int _width;
	int _height;
	int _stride;

	bool _enabled;

	u8 *_lz;
	u8 *_rle;
	int _rle_remaining;
	const u8 *_rle_next;
	int _scanline_y;

	u32 _color;

	int decodeLZ(ImageReader &reader);
	void clear();

	int init(const ImageHeader *header);

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		double initUsec;
		double lzUsec;
		double overallUsec;
	} Stats;
#endif // CAT_COLLECT_STATS

public:
	ImageMaskReader() {
		_mask = 0;
		_lz = 0;
		_rle = 0;
	}
	virtual ~ImageMaskReader() {
		clear();
	}

	int read(ImageReader &reader);

	int nextScanline();

	CAT_INLINE u32 getColor() {
		return _color;
	}

	CAT_INLINE bool masked(int x) {
		const u32 word = _mask[x >> 5];
		return ((word >> (31 - (x & 31))) & 1) != 0;
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

#endif // IMAGE_MASK_READER_HPP

