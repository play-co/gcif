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

	_rgba = 0;
}

static CAT_INLINE u32 hashPixel(u32 key) {
	key <<= 8; // Remove alpha
	key >>= 8;

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

static CAT_INLINE bool checkMatch(const u8 *rgba, int width, u16 x, u16 y, u16 mx, u16 my) {
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

bool ImageLZWriter::match() {
	if (!_rgba) {
		return false;
	}

	const u8 *rgba = _rgba;
	const int width = _width;

	u32 collisions = 0;

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
						cout << "Found hash match between (" << x << ", " << y << ") and (" << mx << ", " << my << ")" << endl;
					} else {
						++collisions;
					}
				}
			}
		} while (++x <= xend);

		for (int jj = 0; jj < ZONE; ++jj) {
			u32 *lp = (u32*)&rgba[(x + (y + jj) * width)*4];
			hash -= hashPixel(getLE(*lp));
			u32 *rp = (u32*)&rgba[((x + ZONE) + (y + jj) * width)*4];
			hash += hashPixel(getLE(*rp));
		}
	}

	cout << collisions << " collisions" << endl;

	return true;
}

