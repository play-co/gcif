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

#ifndef IMAGE_LZ_WRITER_HPP
#define IMAGE_LZ_WRITER_HPP

#include "Platform.hpp"
#include "ImageWriter.hpp"
#include "ImageLZReader.hpp"
#include "GCIFWriter.hpp"

#include <vector>

/*
 * Game Closure 2D LZ (GC-2D-LZ) Compression
 *
 * It finds repeated blocks of pixels in the original RGB raster so that later
 * instances of those blocks can be encoded.
 *
 * A rolling hash is used to index a large hash table of locations.
 * Matching is accelerated by hash lookup, so it runs pretty fast.
 *
 * The image is scanned from the upper left to the lower right, one pixel
 * increment at a time.  Each image match is verified and then expanded.
 *
 * Overlaps with previous matches are immediately rejected.
 *
 * When deciding to accept a match or not, it expects a score of at least 16,
 * where colored pixels count for 1 point and zero pixels count as 0.25 points.
 *
 * To reduce symbol sizes, 258x258 is the largest match allowed.
 *
 * The result is a set of pixel source/dest x,y coordinates (32+32 bits) and a
 * width/height (8+8 bits) for 10 bytes of overhead per match.
 *
 * As an interesting side-note this algorithm provides 2D RLE for free by
 * producing output like this:
 * 22,0 -> 23,0 [4,22] unused=85
 *
 * Note that the source and destination rectangles overlap.  In the transmitted
 * data, only the first literal pixel column is emitted, and then the remaining
 * 21 columns are copied across.  Slick!
 *
 * It seems that about 7% of test image data can be represented with 2D LZ, and
 * the compression ratio is about 5x better than context modeling.
 * So the average 1024x1024 spritesheet gets 50KB smaller with this approach.
 */

namespace cat {


#define IGNORE_ALL_ZERO_MATCHES /* Disallows fully-transparent pixels from causing matches */


class ImageLZWriter {
	static const int ZONEW = ImageLZReader::ZONEW;
	static const int ZONEH = ImageLZReader::ZONEH;
	static const u32 TABLE_NULL = 0xffffffff;
	static const int MAXW = 255 + ZONEW;
	static const int MAXH = 255 + ZONEH;
	static const int ENCODER_ZRLE_SYMS = ImageLZReader::ENCODER_ZRLE_SYMS;

	int _table_size; // 1 << table_bits
	int _table_mask; // table_size - 1

	const GCIFKnobs *_knobs;
	const u8 *_rgba;
	int _width, _height;

	// Value is 16-bit x, y coordinates
	u32 *_table;

	// Visited bitmask
	u32 *_visited;

	CAT_INLINE void visit(int x, int y) {
		int off = x + y * _width;
		_visited[off >> 5] |= 1 << (off & 31);
	}

	struct Match {
		u16 sx, sy;
		u16 dx, dy;
		u8 w, h; // 0 = ZONE, 1 = ZONE+1, ...
	};

	static bool matchSortCompare(const Match &i, const Match &j);

	std::vector<Match> _exact_matches;

	void clear();
	bool checkMatch(u16 x, u16 y, u16 mx, u16 my);	
	bool expandMatch(u16 &sx, u16 &sy, u16 &dx, u16 &dy, u16 &w, u16 &h);
	u32 score(int x, int y, int w, int h);
	void add(int unused, u16 sx, u16 sy, u16 dx, u16 dy, u16 w, u16 h);
	int match();
	void sortMatches();

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		u32 covered, collisions, initial_matches;
		double covered_percent;

		u32 bytes_saved, bytes_overhead, match_count;
		double compression_ratio;

		u32 huff_bits;
	} Stats;
#endif

public:
	CAT_INLINE ImageLZWriter() {
		_rgba = 0;
		_table = 0;
		_visited = 0;
	}
	virtual CAT_INLINE ~ImageLZWriter() {
		clear();
	}

	int initFromRGBA(const u8 *rgba, int width, int height, const GCIFKnobs *knobs);

	CAT_INLINE u32 visited(int x, int y) {
		int off = x + y * _width;
		return (_visited[off >> 5] >> (off & 31)) & 1;
	}

	void write(ImageWriter &writer);
#ifdef CAT_COLLECT_STATS
	bool dumpStats();
#else
	CAT_INLINE bool dumpStats() {
		return false;
	}
#endif

	// Returns false if no LZ destination region under x,y
	// w,h : number of pixels left in region including the pixel at x,y
	bool findExtent(int x, int y, int &w, int &h);
};


} // namespace cat

#endif // IMAGE_LZ_WRITER_HPP

