#ifndef IMAGE_LP_WRITER_HPP
#define IMAGE_LP_WRITER_HPP

#include "Platform.hpp"
#include "ImageWriter.hpp"
#include "ImageMaskWriter.hpp"
#include "ImageLZWriter.hpp"

/*
 * Game Closure Local Palette (GC-LP) Compression
 *
 * This algorithm searches for rectangular regions where the number of colors
 * used is less than a quarter of the number of pixels in the region, or some
 * sort of clever threshold like that that seems to work well; let's be honest
 * this is mainly black magic.
 *
 * Each region takes 6 bytes to represent: x(16b), y(16b), w(8b), h(8b)
 * Plus the cost of transmitting each color.
 */

namespace cat {


class ImageLPWriter {
	static const int ZONE = 4;

	void clear();

	const u8 *_rgba;
	int _width, _height;

	ImageMaskWriter *_mask;
	ImageLZWriter *_lz;

	// Visited bitmask
	u32 *_visited;

	CAT_INLINE void visit(int x, int y) {
		int off = x + y * _width;
		_visited[off >> 5] |= 1 << (off & 31);
	}

	struct Match {
		u16 x, y;
		u8 w, h;
	};

	std::vector<Match> _exact_matches;

	bool checkMatch(u16 x, u16 y, u16 w, u16 h);	
	bool expandMatch(u16 &x, u16 &y, u16 &w, u16 &h);
	void add(u16 x, u16 y, u16 w, u16 h);
	int match();

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
	} Stats;
#endif

public:
	CAT_INLINE ImageLPWriter() {
		_visited = 0;
		_rgba = 0;
	}
	virtual CAT_INLINE ~ImageLPWriter() {
		clear();
	}

	int initFromRGBA(const u8 *rgba, int width, int height, ImageMaskWriter &mask, ImageLZWriter &lz);

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

#endif // IMAGE_PALETTE_WRITER_HPP

