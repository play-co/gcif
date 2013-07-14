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

#ifndef SMALL_PALETTE_READER_HPP
#define SMALL_PALETTE_READER_HPP

#include "Platform.hpp"
#include "GCIFReader.h"
#include "ImageReader.hpp"
#include "ImageMaskReader.hpp"
#include "MonoReader.hpp"

#include <vector>
#include <map>

/*
 * Game Closure Small Palette Decompression
 */

namespace cat {


//// SmallPaletteReader

class SmallPaletteReader {
public:
	static const int SMALL_PALETTE_MAX = 16;

protected:
	static const int MAX_SYMS = 256;

	u32 _palette[SMALL_PALETTE_MAX];
	int _palette_size;

	ImageMaskReader * CAT_RESTRICT _mask;

	u8 * CAT_RESTRICT _rgba;
	u16 _xsize, _ysize, _pack_x, _pack_y;

	SmartArray<u8> _image;

	int _pack_palette_size;	// Palette size for repacked bytes
	u8 _pack_palette[MAX_SYMS];
	u8 _mask_palette;	// Masked palette index

	MonoReader _mono_decoder;

	int readSmallPalette(ImageReader & CAT_RESTRICT reader);
	int readPackPalette(ImageReader & CAT_RESTRICT reader);
	int readTables(ImageReader & CAT_RESTRICT reader);
	int readPixels(ImageReader & CAT_RESTRICT reader);
	int unpackPixels();

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		double smallPaletteUsec;
		double packPaletteUsec;
		double tablesUsec;
		double pixelsUsec;
		double unpackUsec;

		double overallUsec;
	} Stats;
#endif

public:
	CAT_INLINE bool enabled() {
		return _palette_size > 0;
	}

	CAT_INLINE bool multipleColors() {
		return _palette_size > 1;
	}

	int readHead(ImageReader & CAT_RESTRICT reader, u8 * CAT_RESTRICT rgba);
	int readTail(ImageReader & CAT_RESTRICT reader, ImageMaskReader & CAT_RESTRICT mask);

	CAT_INLINE u16 getPackX() {
		return _pack_x;
	}
	CAT_INLINE u16 getPackY() {
		return _pack_y;
	}

#ifdef CAT_COLLECT_STATS
	bool dumpStats();
#else
	CAT_INLINE bool dumpStats() {
		return false;
	}
#endif
};


} // namespace cat

#endif // SMALL_PALETTE_READER_HPP

