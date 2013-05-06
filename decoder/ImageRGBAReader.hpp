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

#ifndef IMAGE_RGBA_READER_HPP
#define IMAGE_RGBA_READER_HPP

#include "ImageReader.hpp"
#include "ImageMaskReader.hpp"
#include "ImageLZReader.hpp"
#include "GCIFReader.h"
#include "Filters.hpp"
#include "EntropyDecoder.hpp"

/*
 * Game Closure RGBA Decompression
 *
 * The decompressor rebuilds the static Huffman tables generated by the encoder
 * and then iterates over each pixel from upper left to lower right.
 *
 * Where the 2D LZ Exact Match algorithm triggers, it performs LZ decoding.
 * Where the Dominant Color mask is set, it emits a pixel of that color.
 *
 * For the remaining pixels, the BCIF "chaos" metric selects which Huffman
 * tables to use, and filtered pixel values are emitted.  The YUV color data is
 * then reversed to RGB and then the spatial filter is reversed back to the
 * original RGB data.
 *
 * LZ and alpha masking are very cheap decoding operations.  The most expensive
 * per-pixel operation is the static Huffman decoding, which is just a table
 * lookup and some bit twiddling for the majority of decoding.  As a result the
 * decoder is exceptionally fast.  It reaches for the Pareto Frontier.
 */

namespace cat {


//// ImageRGBAReader

class ImageRGBAReader {
public:
	static const int ZRLE_SYMS_Y = 128;
	static const int ZRLE_SYMS_U = 128;
	static const int ZRLE_SYMS_V = 128;
	static const int ZRLE_SYMS_A = 128;

protected:
	static const int NUM_COLORS = 256;

	ImageMaskReader *_mask;
	ImageLZReader *_lz;

	// RGBA output data
	u8 *_rgba;
	u16 _size_x, _size_y;
	u16 _tile_bits_x, _tiles_bits_y;
	u16 _tile_size_x, _tiles_size_y;

	RGBAChaos _chaos;

	RGBAFilterFuncs _sf[MAX_FILTERS];
	int _sf_count;

	MonoReader _sf_decoder, _cf_decoder;

	// Color plane decoders
	EntropyDecoder<NUM_COLORS, ZRLE_SYMS_Y> _y_decoder[CHAOS_LEVELS_MAX];
	EntropyDecoder<NUM_COLORS, ZRLE_SYMS_U> _u_decoder[CHAOS_LEVELS_MAX];
	EntropyDecoder<NUM_COLORS, ZRLE_SYMS_V> _v_decoder[CHAOS_LEVELS_MAX];
	EntropyDecoder<NUM_COLORS, ZRLE_SYMS_A> _a_decoder[CHAOS_LEVELS_MAX];

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
	CAT_INLINE ImageRGBAReader() {
		_rgba = 0;
	}
	virtual CAT_INLINE ~ImageRGBAReader() {
	}

	int read(ImageReader &reader, ImageMaskReader &maskReader, ImageLZReader &lzReader, GCIFImage *image);

#ifdef CAT_COLLECT_STATS
	bool dumpStats();
#else
	CAT_INLINE bool dumpStats() {
		return false;
	}
#endif
};


} // namespace cat

#endif // IMAGE_CM_READER_HPP

