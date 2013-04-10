#include "ImageLZWriter.hpp"
#include "EndianNeutral.hpp"
using namespace cat;

#include <iostream>
using namespace std;


//// ImageLZWriter

void ImageLZWriter::clear() {
	if (_table) {
		delete []_table;
		_table = 0;
	}
	if (_visited) {
		delete []_visited;
		_visited = 0;
	}

	_rgba = 0;
}

static CAT_INLINE u32 hashPixel(u32 key) {
	key <<= 8; // Remove alpha

	// Thomas Wang's integer hash function
	// http://www.cris.com/~Ttwang/tech/inthash.htm
	// http://burtleburtle.net/bob/hash/integer.html
	key = (key ^ 61) ^ (key >> 16);
	key = key + (key << 3);
	key = key ^ (key >> 4);
	key = key * 0x27d4eb2d;
	key = key ^ (key >> 15);

	return key;
}

bool ImageLZWriter::initWithRGBA(const u8 *rgba, int width, int height) {
	clear();

	if (width % ZONE || height % ZONE) {
		return false;
	}

	_rgba = rgba;
	_width = width;
	_height = height;

	_table = new u32[TABLE_SIZE];
	memset(_table, 0xff, TABLE_SIZE * sizeof(u32));

	const int visited_size = (width * height + 31) / 32;
	_visited = new u32[visited_size];
	memset(_visited, 0, visited_size * sizeof(u32));

	for (int y = height - ZONE; y >= 0; y -= ZONE) {
		for (int x = width - ZONE; x >= 0; x -= ZONE) {
			u32 hash = 0;

			for (int ii = 0; ii < ZONE; ++ii) {
				for (int jj = 0; jj < ZONE; ++jj) {
					u32 *p = (u32*)&rgba[((x + ii) + (y + jj) * width)*4];
					hash += hashPixel(getLE(*p));
				}
			}

			_table[hash & TABLE_MASK] = (x << 16) | y;
		}
	}

	return true;
}

static bool checkMatch(const u8 *rgba, int width, u16 x, u16 y, u16 mx, u16 my) {
	static const int ZONE = ImageLZWriter::ZONE;

	for (int ii = 0; ii < ZONE; ++ii) {
		for (int jj = 0; jj < ZONE; ++jj) {
			u32 *p = (u32*)&rgba[((x + ii) + (y + jj) * width)*4];
			u32 *mp = (u32*)&rgba[((mx + ii) + (my + jj) * width)*4];

			// If RGB components match,
			if (((getLE(*p) ^ getLE(*mp)) << 8) != 0) {
				return false;
			}
		}
	}

	return true;
}

static bool expandMatch(const u8 *rgba, int width, int height, u16 &sx, u16 &sy, u16 &dx, u16 &dy, u16 &w, u16 &h) {
	static const int MAX_MATCH_SIZE = ImageLZWriter::MAX_MATCH_SIZE;

	// Try expanding to the right
	while (w + 1 <= MAX_MATCH_SIZE && sx + w < width && dx + w < width) {
		for (int jj = 0; jj < h; ++jj) {
			u32 *sp = (u32*)&rgba[((sx + w) + (sy + jj) * width)*4];
			u32 *dp = (u32*)&rgba[((dx + w) + (dy + jj) * width)*4];

			// If RGB components match,
			if (((getLE(*sp) ^ getLE(*dp)) << 8) != 0) {
				goto try_down;
			}
		}

		// Widen width
		++w;
	}

	// Try expanding down
try_down:
	while (h + 1 <= MAX_MATCH_SIZE && sy + h < height && dy + h < height) {
		for (int jj = 0; jj < w; ++jj) {
			u32 *sp = (u32*)&rgba[((sx + jj) + (sy + h) * width)*4];
			u32 *dp = (u32*)&rgba[((dx + jj) + (dy + h) * width)*4];

			// If RGB components match,
			if (((getLE(*sp) ^ getLE(*dp)) << 8) != 0) {
				goto try_left;
			}
		}

		// Heighten height
		++h;
	}

	// Try expanding left
try_left:
	while (w + 1 <= MAX_MATCH_SIZE && sx >= 1 && dx >= 1) {
		for (int jj = 0; jj < h; ++jj) {
			u32 *sp = (u32*)&rgba[((sx - 1) + (sy + jj) * width)*4];
			u32 *dp = (u32*)&rgba[((dx - 1) + (dy + jj) * width)*4];

			// If RGB components match,
			if (((getLE(*sp) ^ getLE(*dp)) << 8) != 0) {
				goto try_up;
			}
		}

		// Widen width
		sx--;
		dx--;
		++w;	
	}

	// Try expanding up
try_up:
	while (h + 1 <= MAX_MATCH_SIZE && sy >= 1 && dy >= 1) {
		for (int jj = 0; jj < w; ++jj) {
			u32 *sp = (u32*)&rgba[((sx + jj) + (sy - 1) * width)*4];
			u32 *dp = (u32*)&rgba[((dx + jj) + (dy - 1) * width)*4];

			// If RGB components match,
			if (((getLE(*sp) ^ getLE(*dp)) << 8) != 0) {
				return true;
			}
		}

		// Widen width
		sy--;
		dy--;
		++h;	
	}

	return true;
}

u32 ImageLZWriter::score(int x, int y, int w, int h) {
	u32 sum = 0;

	for (int ii = 0; ii < w; ++ii) {
		for (int jj = 0; jj < h; ++jj) {
			u32 *p = (u32*)&_rgba[((x + ii) + (y + jj) * _width)*4];
			u32 pv = getLE(*p) << 8;
			if (pv == 0) {
				sum += 1 ^ visited(x + ii, y + jj);
			} else {
				sum += (1 ^ visited(x + ii, y + jj)) * ZERO_COEFF;
			}
		}
	}

	return sum / ZERO_COEFF;
}

void ImageLZWriter::add(int unused, u16 sx, u16 sy, u16 dx, u16 dy, u16 w, u16 h) {
	// Mark pixels as visited
	for (int ii = 0; ii < w; ++ii) {
		for (int jj = 0; jj < h; ++jj) {
			visit(dx + ii, dy + jj);
		}
	}

	Match m = {
		sx, sy, dx, dy, w - ZONE, h - ZONE
	};

	_matches.push_back(m);

	cout << sx << "," << sy << " -> " << dx << "," << dy << " [" << w << "," << h << "] unused=" << unused << endl;

	_covered += unused;
}

bool ImageLZWriter::match() {
	if (!_rgba) {
		return false;
	}

	const u8 *rgba = _rgba;
	const int width = _width;

	_collisions = 0;
	_initial_matches = 0;
	_covered = 0;

	// For each raster,
	for (u16 y = 0, yend = _height - ZONE; y <= yend; ++y) {
		u32 hash = 0;

		// Initialize a full hash block in the upper left of this row
		for (int ii = 0; ii < ZONE; ++ii) {
			for (int jj = 0; jj < ZONE; ++jj) {
				u32 *p = (u32*)&rgba[(ii + jj * width)*4];
				hash += hashPixel(getLE(*p));
			}
		}

		u16 x = 0, xend = width - ZONE;
		do {
			u32 match = _table[hash & TABLE_MASK];
			if (match != TABLE_NULL) {
				u16 mx = (u16)(match >> 16);
				u16 my = (u16)match;

				if (mx != x && my != y) {

					if (checkMatch(rgba, width, x, y, mx, my)) {
						++_initial_matches;

						// Determine source and destination in decoder order
						u16 sx = x, sy = y, dx = mx, dy = my, w = ZONE, h = ZONE;
						if (my > y) {
						} else if (my == y) {
							if (mx > x) {
							} else {
								sx = mx;
								sy = my;
								dx = x;
								dy = y;
							}
						} else {
							sx = mx;
							sy = my;
							dx = x;
							dy = y;
						}

						// See how far the match can be expanded
						expandMatch(rgba, width, _height, sx, sy, dx, dy, w, h);

						int unused = score(dx, dy, w, h);
						if (unused >= MIN_SCORE) {
							add(unused, sx, sy, dx, dy, w, h);
						}
					} else {
						++_collisions;
					}
				}
			}

			for (int jj = 0; jj < ZONE; ++jj) {
				u32 *lp = (u32*)&rgba[(x + (y + jj) * width)*4];
				hash -= hashPixel(getLE(*lp));
				u32 *rp = (u32*)&rgba[((x + ZONE) + (y + jj) * width)*4];
				hash += hashPixel(getLE(*rp));
			}
		} while (++x <= xend);
	}

	cout << _initial_matches << " initial matches" << endl;
	cout << _collisions << " initial collisions" << endl;
	cout << _covered << " covered / " << width * _height << endl;
	cout << _matches.size() * 10 << " bytes overhead / " << _covered * 3 << " bytes saved" << endl;

	return true;
}

