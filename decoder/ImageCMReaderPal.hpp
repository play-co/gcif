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

#ifndef IMAGE_CM_READER_PAL_HPP
#define IMAGE_CM_READER_PAL_HPP

#include "ImageReader.hpp"
#include "ImageMaskReader.hpp"
#include "ImageLZReader.hpp"
#include "ImagePaletteReader.hpp"
#include "GCIFReader.h"
#include "Filters.hpp"
#include "EntropyDecoder.hpp"

/*
 * Game Closure Context Modeling (GC-CM) Decompression -- Palette version
 *
 * The palette version is similar to the normal one, except that it only reads
 * one channel.
 *
 * See ImageCMReader.hpp for more documentation.
 */

namespace cat {


//// ImageCMReaderPal

class ImageCMReaderPal {
public:
	static const int CHAOS_LEVELS_MAX = 8;
	static const int ZRLE_SYMS = 128;
	static const int RECENT_ROWS = 16; // Number of image rows to allocate

protected:
	// RGBA output data
	int _width, _height;
	u8 *_rgba;

	// Recent chaos memory
	u8 *_chaos;
	u32 _chaos_size;
	u32 _chaos_alloc;

	// Chaos lookup table
	int _chaos_levels;
	const u8 *_chaos_table;

	// Palette memory
	u8 *_pdata;
	int _pdata_alloc;

	// Recent scanline filters
	struct FilterSelection {
		PaletteFilterSet::Functions sf;

		CAT_INLINE bool ready() {
			return sf.safe != 0;
		}
	} *_filters;
	int _filters_bytes;
	int _filters_alloc;

	// Mask and LZ subsystems
	ImageMaskReader *_mask;
	ImageLZReader *_lz;
	ImagePaletteReader *_pal;

	// Chosen palette filter set
	PaletteFilterSet _pf_set;

	// Palette filter decoder
	HuffmanDecoder _pf;

	// Palette index decoder
	EntropyDecoder<256, ZRLE_SYMS> _decoder[CHAOS_LEVELS_MAX];

	void clear();

	int init(GCIFImage *image);
	int readFilterTables(ImageReader &reader);
	int readChaosTables(ImageReader &reader);
	int readPixels(ImageReader &reader);

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		double initUsec, readFilterTablesUsec, readChaosTablesUsec;
		double readPixelsUsec, overallUsec;
	} Stats;
#endif

public:
	CAT_INLINE ImageCMReaderPal() {
		_rgba = 0;
		_chaos = 0;
		_filters = 0;
		_pdata = 0;
	}
	virtual CAT_INLINE ~ImageCMReaderPal() {
		clear();
	}

	int read(ImageReader &reader, ImageMaskReader &maskReader, ImageLZReader &lzReader, ImagePaletteReader &palReader, GCIFImage *image);

#ifdef CAT_COLLECT_STATS
	bool dumpStats();
#else
	CAT_INLINE bool dumpStats() {
		return false;
	}
#endif
};


} // namespace cat

#endif // IMAGE_CM_READER_PAL_HPP

