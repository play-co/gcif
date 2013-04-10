#ifndef IMAGE_LZ_WRITER_HPP
#define IMAGE_LZ_WRITER_HPP

#include "Platform.hpp"

#include <vector>

/*
 * Game Closure's custom image LZ
 * or "Please help me I have lost control of my life to this project."
 *
 * It finds repeated blocks of pixels in the original RGB raster so that later
 * instances of those blocks can be encoded.
 *
 * First step is to scan the whole image in 4x4 blocks and hash each block into
 * a big hash table.  The index of the hash table is the 32-bit hash number and
 * the value at each index is the location to find that block.  The scan is
 * done from the lower right to the upper left so that the hash table prefers
 * matches from the upper left.
 *
 * Then the image is scanned from the upper left to the lower right, one pixel
 * increment at a time.  Each image match is verified and then expanded.
 *
 * Overlaps are immediately rejected.
 *
 * When deciding to accept a match or not, it expects a score of at least 16,
 * where colored pixels count for 1 point and zero pixels count as 0.25 points.
 *
 * To reduce symbol sizes, 259x259 is the largest match allowed.
 * Compression speed is definitely not any consideration however, not that it
 * runs slowly.
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
 */

namespace cat {


#define IGNORE_ALL_ZERO_MATCHES /* Disallows fully-transparent pixels from causing matches */


class ImageLZWriter {
public:
	static const int ZONE = 4;
	static const int TABLE_BITS = 18;
	static const int TABLE_SIZE = 1 << TABLE_BITS;
	static const u32 TABLE_MASK = TABLE_SIZE - 1;
	static const u32 TABLE_NULL = 0xffffffff;
	static const int MAX_MATCH_SIZE = 255 + ZONE;
	static const int MIN_SCORE = 16;
	static const int ZERO_COEFF = 4; // Count zeroes as being worth 1/4 of a normal match

protected:
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
	CAT_INLINE u32 visited(int x, int y) {
		int off = x + y * _width;
		return (_visited[off >> 5] >> (off & 31)) & 1;
	}

	struct Match {
		u16 sx, sy;
		u16 dx, dy;
		u8 w, h;
	};

	std::vector<Match> _exact_matches;
	//std::vector<Match> _fuzzy_matches;
	u32 _covered, _collisions, _initial_matches;

	void clear();
	bool checkMatch(u16 x, u16 y, u16 mx, u16 my);	
	bool expandMatch(u16 &sx, u16 &sy, u16 &dx, u16 &dy, u16 &w, u16 &h);
	u32 score(int x, int y, int w, int h);
	void add(int unused, u16 sx, u16 sy, u16 dx, u16 dy, u16 w, u16 h);

public:
	CAT_INLINE ImageLZWriter() {
		_rgba = 0;
		_table = 0;
		_visited = 0;
	}
	virtual CAT_INLINE ~ImageLZWriter() {
		clear();
	}

	bool initWithRGBA(const u8 *rgba, int width, int height);

	bool match();
};


} // namespace cat

#endif // IMAGE_LZ_WRITER_HPP

