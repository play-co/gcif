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

#ifndef IMAGE_RGBA_WRITER_HPP
#define IMAGE_RGBA_WRITER_HPP

#include "ImageWriter.hpp"
#include "ImageMaskWriter.hpp"
#include "ImageLZWriter.hpp"
#include "ImagePaletteWriter.hpp"
#include "../decoder/ImageRGBAReader.hpp"
#include "EntropyEncoder.hpp"
#include "FilterScorer.hpp"
#include "../decoder/Filters.hpp"
#include "GCIFWriter.h"

#include <vector>

/*
 * Game Closure RGBA Compression
 *
 * This is based heavily on BCIF by Stefano Brocchi
 * from his PhD thesis "Bidimensional pictures: reconstruction, expression and encoding" (Dec 2009)
 * http://www.dsi.unifi.it/DRIIA/RaccoltaTesi/Brocchi.pdf
 *
 * Notable improvements:
 * + Much better compression ratios
 * + Maintainable codebase for future improvements
 * + 2D LZ Exact Matc, Dominant Color Mask, and Global Palette integration
 * + Uses 4x4 tiles instead of 8x8
 * + More/better non-linear spatial and more color filters supported
 * + Linear spatial filters tuned to image where improvement is found
 * + Chaos metric is order-1 stats, so do not fuzz them, and use just 8 levels
 * + Encodes zero runs > ~256 without emitting more symbols for better AZ stats
 * + Better, context-modeled Huffman table compression
 * + Faster entropy estimation allows us to run entropy analysis exhaustively
 * + Revisit top of image after choosing filters for better selection
 */

namespace cat {


//// ImageRGBAWriter

class ImageRGBAWriter {
protected:
	static const int CHAOS_LEVELS_MAX = ImageRGBAReader::CHAOS_LEVELS_MAX;
	static const int COLOR_PLANES = ImageRGBAReader::COLOR_PLANES;
	static const int ZRLE_SYMS_Y = ImageRGBAReader::ZRLE_SYMS_Y;
	static const int ZRLE_SYMS_U = ImageRGBAReader::ZRLE_SYMS_U;
	static const int ZRLE_SYMS_V = ImageRGBAReader::ZRLE_SYMS_V;
	static const int ZRLE_SYMS_A = ImageRGBAReader::ZRLE_SYMS_A;
	static const int MAX_FILTERS = 32;

	static const u8 MASK_TILE = 255;
	static const u8 TODO_TILE = 0;

	// Twiddly knobs from the write API
	const GCIFKnobs *_knobs;

	// Subsystems
	ImageMaskWriter *_mask;
	ImageLZWriter *_lz;

	// RGBA image
	const u8 *_rgba;
	u16 _size_x, _size_y;

	// Filter tiles
	u16 _tile_bits_x, _tile_bits_y;
	u16 _tile_size_x, _tile_size_y;
	u16 _tiles_x, _tiles_y;
	u8 *_sf_tiles;
	u8 *_cf_tiles;
	int _tiles_alloc;

	// Chosen spatial filter set
	RGBAFilterFuncs _sf[MAX_FILTERS];
	u16 _sf_indices[MAX_FILTERS];
	int _sf_count;

	// Write state
	u8 *_seen_filter;
	int _seen_filter_alloc;
	RGBAChaos _chaos;

	// Color channel encoders
	EntropyEncoder<256, ZRLE_SYMS_Y> _y_encoder[CHAOS_LEVELS_MAX];
	EntropyEncoder<256, ZRLE_SYMS_U> _u_encoder[CHAOS_LEVELS_MAX];
	EntropyEncoder<256, ZRLE_SYMS_V> _v_encoder[CHAOS_LEVELS_MAX];
	EntropyEncoder<256, ZRLE_SYMS_A> _a_encoder[CHAOS_LEVELS_MAX];

	MonoWriter _sf_encoder;
	MonoWriter _cf_encoder;

	void clear();

	bool IsMasked(u16 x, u16 y);

	void maskTiles();
	void designFilters();
	void designTiles();
	void compressFilters();
	void initializeEncoders();

	bool writeFilters(ImageWriter &writer);
	bool writeChaos(ImageWriter &writer);

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		// For these SF = 0, CF = 1
		int filter_table_bits[2];
		int filter_compressed_bits[2];

		int chaos_overhead_bits;

		// RGB data
		int rgb_bits[COLOR_PLANES];

		int chaos_bits;
		int total_bits;

		u32 chaos_count;
		double chaos_compression_ratio;

		double overall_compression_ratio;
	} Stats;
#endif // CAT_COLLECT_STATS

public:
	CAT_INLINE ImageRGBAWriter() {
		_sf_tiles = 0;
		_cf_tiles = 0;
		_seen_filter = 0;
	}
	CAT_INLINE virtual ~ImageRGBAWriter() {
		clear();
	}

	int init(const u8 *rgba, int size_x, int size_y, ImageMaskWriter &mask, ImageLZWriter &lz, const GCIFKnobs *knobs);

	void write(ImageWriter &writer);

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

#endif // IMAGE_RGBA_WRITER_HPP

