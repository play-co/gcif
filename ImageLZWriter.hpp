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
 * First step is to scan the whole image in 8x8 blocks and hash each block into
 * a big hash table.  The index of the hash table is the 32-bit hash number and
 * the value at each index is the location to find that block.  The scan is
 * done from the lower right to the upper left so that the hash table prefers
 * matches from the upper left.
 *
 * Then the image is scanned from the upper left to the lower right, one pixel
 * increment at a time.  Each image match is verified and then expanded.
 * Forward matches are as useful as backwards matches at this point.
 *
 * To avoid overlaps, a simple algorithm is used:
 *
 * When a match is found, the one farther right/down locks the 8x8 blocks that
 * are completely covered, and those cannot be used again for further matches.
 *
 * To avoid slowing down too much, ~256x256 is the largest match allowed.
 *
 * The result is a set of pixel source/dest x,y coordinates (32+32 bits) and a
 * width/height (8+8 bits) or 10 bytes of overhead.  These are transmitted with
 * the image data and processed specially in the decoder.
 */

namespace cat {


class ImageLZWriter {
public:
	static const int ZONE = 8;
	static const int TABLE_BITS = 16;
	static const int TABLE_SIZE = 1 << TABLE_BITS;
	static const u32 TABLE_MASK = TABLE_SIZE - 1;
	static const u32 TABLE_NULL = 0xffffffff;

protected:
	const u8 *_rgba;
	int _width, _height;

	// Value is 16-bit x, y coordinates
	u32 *_table;
	int _table_size;

	struct Match {
		u16 sx, sy;
		u16 dx, dy;
		u8 w, h;
	};

	std::vector<Match> _matches;

	void clear();

public:
	CAT_INLINE ImageLZWriter() {
		_rgba = 0;
		_table = 0;
	}
	virtual CAT_INLINE ~ImageLZWriter() {
		clear();
	}

	bool initWithRGBA(const u8 *rgba, int width, int height);

	bool match();
};


} // namespace cat

#endif // IMAGE_LZ_WRITER_HPP

