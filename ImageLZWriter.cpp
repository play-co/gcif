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

	return true;
}

bool ImageLZWriter::checkMatch(u16 x, u16 y, u16 mx, u16 my) {
	const u8 *rgba = _rgba;
	const int width = _width;

	for (int ii = 0; ii < ZONE; ++ii) {
		for (int jj = 0; jj < ZONE; ++jj) {
			if (visited(x + ii, y + jj)) {
				return false;
			}

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

bool ImageLZWriter::expandMatch(u16 &sx, u16 &sy, u16 &dx, u16 &dy, u16 &w, u16 &h) {
	const u8 *rgba = _rgba;
	const int width = _width;
	const int height = _height;

	// Try expanding to the right
	while (w + 1 <= MAX_MATCH_SIZE && sx + w < width && dx + w < width) {
		for (int jj = 0; jj < h; ++jj) {
			if (visited(dx + w, dy + jj)) {
				goto try_down;
			}

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
			if (visited(dx + jj, dy + h)) {
				goto try_left;
			}

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
			if (visited(dx - 1, dy + jj)) {
				goto try_up;
			}

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
			if (visited(dx + jj, dy - 1)) {
				return true;
			}

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
			if (visited(x + ii, y + jj)) {
				return 0;
			}
			u32 *p = (u32*)&_rgba[((x + ii) + (y + jj) * _width)*4];
			u32 pv = getLE(*p) << 8;
			if (pv == 0) {
				sum += 1;
			} else {
				sum += ZERO_COEFF;
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

	_exact_matches.push_back(m);

	//cout << sx << "," << sy << " -> " << dx << "," << dy << " [" << w << "," << h << "] unused=" << unused << endl;

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
				u16 sx = (u16)(match >> 16);
				u16 sy = (u16)match;

				if (checkMatch(sx, sy, x, y)) {
					++_initial_matches;

					// Determine source and destination in decoder order
					u16 dx = x, dy = y, w = ZONE, h = ZONE;

					// See how far the match can be expanded
					expandMatch(sx, sy, dx, dy, w, h);

					int unused = score(dx, dy, w, h);
					if (unused >= MIN_SCORE) {
						add(unused, sx, sy, dx, dy, w, h);
					}
				} else {
					++_collisions;
				}
			}

			_table[hash & TABLE_MASK] = ((u32)x << 16) | y;

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
	cout << _exact_matches.size() * 10 << " bytes overhead / " << _covered * 3 << " bytes saved" << endl;

	return true;
}

