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

#ifndef IMAGE_PALETTE_READER_HPP
#define IMAGE_PALETTE_READER_HPP

#include "ImageReader.hpp"
#include "Enforcer.hpp"
#include "MonoReader.hpp"
#include "ImageMaskReader.hpp"
#include "SmartArray.hpp"

/*
 * Game Closure Global Palette Decompression
 */

namespace cat {


//// ImagePaletteReader

class ImagePaletteReader {
public:
	static const int PALETTE_MAX = 256;
	static const int ENCODER_ZRLE_SYMS = 16;
	static const int HUFF_LUT_BITS = 7;

protected:
	u32 _palette[PALETTE_MAX];
	int _palette_size;
	u8 _mask_palette;	// Masked palette index

	ImageMaskReader * CAT_RESTRICT _mask;

	u8 * CAT_RESTRICT _rgba;
	u16 _xsize, _ysize;

	SmartArray<u8> _image;

	MonoReader _mono_decoder;

	int readPalette(ImageReader & CAT_RESTRICT reader);
	int readTables(ImageReader & CAT_RESTRICT reader);
	int readPixels(ImageReader & CAT_RESTRICT reader);

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		double paletteUsec;
		double tablesUsec;
		double pixelsUsec;

		int colorCount;
	} Stats;
#endif

public:
	CAT_INLINE bool enabled() {
		return _palette_size > 0;
	}

	int read(ImageReader & CAT_RESTRICT reader, ImageMaskReader & CAT_RESTRICT mask, GCIFImage * CAT_RESTRICT image);

#ifdef CAT_COLLECT_STATS
	bool dumpStats();
#else
	CAT_INLINE bool dumpStats() {
		return false;
	}
#endif
};


} // namespace cat

#endif // IMAGE_PALETTE_READER_HPP

