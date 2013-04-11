#ifndef IMAGE_LZ_WRITER_HPP
#define IMAGE_LZ_WRITER_HPP

#include "Platform.hpp"
#include "ImageWriter.hpp"
#include "ImageLZReader.hpp"

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
 * To reduce symbol sizes, 259x259 is the largest match allowed.
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
public:
	static const int ZONE = ImageLZReader::ZONE;

protected:
	static const int TABLE_BITS = 18;
	static const int TABLE_SIZE = 1 << TABLE_BITS;
	static const u32 TABLE_MASK = TABLE_SIZE - 1;
	static const u32 TABLE_NULL = 0xffffffff;
	static const int MAX_MATCH_SIZE = 255 + ZONE;
	static const int MIN_SCORE = 16;
	static const int ZERO_COEFF = 4; // Count zeroes as being worth 1/4 of a normal match

	const u8 *_rgba;
	int _width, _height;

	// Value is 16-bit x, y coordinates
	u32 *_table;
	int _table_size;

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

	std::vector<Match> _exact_matches;

	void clear();
	bool checkMatch(u16 x, u16 y, u16 mx, u16 my);	
	bool expandMatch(u16 &sx, u16 &sy, u16 &dx, u16 &dy, u16 &w, u16 &h);
	u32 score(int x, int y, int w, int h);
	void add(int unused, u16 sx, u16 sy, u16 dx, u16 dy, u16 w, u16 h);
	int match();

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		u32 covered, collisions, initial_matches;
		double covered_percent;

		u32 bytes_saved, bytes_overhead, bytes_overhead_uncompressed;
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

	int initFromRGBA(const u8 *rgba, int width, int height);

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
};


} // namespace cat

#endif // IMAGE_LZ_WRITER_HPP

