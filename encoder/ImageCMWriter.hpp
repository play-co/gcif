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

#ifndef IMAGE_FILTER_WRITER_HPP
#define IMAGE_FILTER_WRITER_HPP

#include "ImageWriter.hpp"
#include "ImageMaskWriter.hpp"
#include "ImageLZWriter.hpp"
#include "../decoder/ImageCMReader.hpp"
#include "EntropyEncoder.hpp"
#include "FilterScorer.hpp"
#include "../decoder/Filters.hpp"
#include "GCIFWriter.h"

#include <vector>

/*
 * Game Closure Context Modeling (GC-CM) Compression
 *
 * This is based heavily on BCIF by Stefano Brocchi
 * from his PhD thesis "Bidimensional pictures: reconstruction, expression and encoding" (Dec 2009)
 * http://www.dsi.unifi.it/DRIIA/RaccoltaTesi/Brocchi.pdf
 *
 * Notable improvements:
 * + Much better compression ratios
 * + Maintainable codebase for future improvements
 * + 2D LZ Exact Match and Dominant Color Mask integration
 * + Uses 4x4 zones instead of 8x8
 * + More/better non-linear spatial and more color filters supported
 * + Linear spatial filters tuned to image where improvement is found
 * + Chaos metric is order-1 stats, so do not fuzz them, and use just 8 levels
 * + Encodes zero runs > ~256 without emitting more symbols for better AZ stats
 * + Better, context-modeled Huffman table compression
 * + Faster entropy estimation allows us to run entropy analysis exhaustively
 * + Revisit top of image after choosing filters for better selection
 */

namespace cat {


//// ImageCMWriter

class ImageCMWriter {
protected:
	static const int CHAOS_LEVELS_MAX = ImageCMReader::CHAOS_LEVELS_MAX;
	static const u16 UNUSED_FILTER = 0xffff;
	static const u16 TODO_FILTER = 0;
	static const int COLOR_PLANES = ImageCMReader::COLOR_PLANES;
	static const int ZRLE_SYMS_Y = ImageCMReader::ZRLE_SYMS_Y;
	static const int ZRLE_SYMS_U = ImageCMReader::ZRLE_SYMS_U;
	static const int ZRLE_SYMS_V = ImageCMReader::ZRLE_SYMS_V;
	static const int ZRLE_SYMS_A = ImageCMReader::ZRLE_SYMS_A;

	// Chosen spatial filter set
	SpatialFilterSet _sf_set;

	// Twiddly knobs from the write API
	const GCIFKnobs *_knobs;

	// Filter matrix, storing filter decisions made up front
	u16 *_filters;		// One element per zone
	int _filters_alloc;
	u8 *_seen_filter;	// Seen filter yet for a block
	int _seen_filter_alloc;
	int _filter_stride;	// Filters per scanline

	// Recent measured chaos
	u8 *_chaos;
	int _chaos_size;
	int _chaos_alloc;

	// Recent post-filter data
	int _chaos_levels;
	const u8 *_chaos_table;

	// RGBA input data
	const u8 *_rgba;
	int _width, _height;

	// Dominant color mask subsystem
	ImageMaskWriter *_mask;

	// 2D-LZ subsystem
	ImageLZWriter *_lz;

	// List of custom linear filter replacements
	std::vector<u32> _filter_replacements;

	// Filter encoders
	HuffmanEncoder<CF_COUNT> _cf_encoder;
	HuffmanEncoder<SF_COUNT> _sf_encoder;

	// Color channel encoders
	EntropyEncoder<256, ZRLE_SYMS_Y> _y_encoder[CHAOS_LEVELS_MAX];
	EntropyEncoder<256, ZRLE_SYMS_U> _u_encoder[CHAOS_LEVELS_MAX];
	EntropyEncoder<256, ZRLE_SYMS_V> _v_encoder[CHAOS_LEVELS_MAX];
	EntropyEncoder<256, ZRLE_SYMS_A> _a_encoder[CHAOS_LEVELS_MAX];

	void clear();

	int init(int width, int height);
	void maskFilters();
	void designFilters();
	void decideFilters();
	void scanlineLZ();
	bool applyFilters();
	void chaosStats();

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
	CAT_INLINE ImageCMWriter() {
		_filters = 0;
		_chaos = 0;
		_seen_filter = 0;
	}
	CAT_INLINE virtual ~ImageCMWriter() {
		clear();
	}

	CAT_INLINE void setFilter(int x, int y, u16 filter) {
		x >>= FILTER_ZONE_SIZE_SHIFT;
		y >>= FILTER_ZONE_SIZE_SHIFT;
		const int w = (_width + FILTER_ZONE_SIZE_MASK) >> FILTER_ZONE_SIZE_SHIFT;
		_filters[x + y * w] = filter;
	}

	CAT_INLINE u16 getFilter(int x, int y) {
		x >>= FILTER_ZONE_SIZE_SHIFT;
		y >>= FILTER_ZONE_SIZE_SHIFT;
		const int w = (_width + FILTER_ZONE_SIZE_MASK) >> FILTER_ZONE_SIZE_SHIFT;
		return _filters[x + y * w];
	}

	int initFromRGBA(const u8 *rgba, int width, int height, ImageMaskWriter &mask, ImageLZWriter &lz, const GCIFKnobs *knobs);
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

#endif // IMAGE_FILTER_WRITER_HPP

