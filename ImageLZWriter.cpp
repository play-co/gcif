#include "ImageLZWriter.hpp"
#include "EndianNeutral.hpp"
#include "HuffmanEncoder.hpp"
#include "ImageLZReader.hpp"
#include "Log.hpp"
#include "GCIFWriter.hpp"
using namespace cat;


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
	swapLE(key);
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

int ImageLZWriter::initFromRGBA(const u8 *rgba, int width, int height) {
	clear();

	if (width < ZONE || height < ZONE) {
		return WE_BAD_DIMS;
	}

	_rgba = rgba;
	_width = width;
	_height = height;

	_table = new u32[TABLE_SIZE];
	memset(_table, 0xff, TABLE_SIZE * sizeof(u32));

	const int visited_size = (width * height + 31) / 32;
	_visited = new u32[visited_size];
	memset(_visited, 0, visited_size * sizeof(u32));

	return match();
}

bool ImageLZWriter::checkMatch(u16 x, u16 y, u16 mx, u16 my) {
	const u8 *rgba = _rgba;
	const int width = _width;
#ifdef IGNORE_ALL_ZERO_MATCHES
	u32 nonzero = 0;
#endif

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

#ifdef IGNORE_ALL_ZERO_MATCHES
			nonzero |= *p;
#endif
		}
	}

#ifdef IGNORE_ALL_ZERO_MATCHES
	return (getLE(nonzero) >> 24) != 0;
#else
	return true;
#endif
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
			u32 pv = getLE(*p);

#ifdef IGNORE_ALL_ZERO_MATCHES
			// If not fully transparent,
			if (pv >> 24)
#endif
			{
				// If color data,
				if (pv << 8) {
					sum += ZERO_COEFF;
				} else {
					sum += 1;
				}
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

	//CAT_WARN("TEST") << sx << "," << sy << " -> " << dx << "," << dy << " " << (int)w << "," << (int)h;

	Stats.covered += w * h;
}

int ImageLZWriter::match() {
	if (!_rgba) {
		return WE_BUG;
	}

	const u8 *rgba = _rgba;
	const int width = _width;

	Stats.collisions = 0;
	Stats.initial_matches = 0;
	Stats.covered = 0;

	// For each raster,
	for (u16 y = 0, yend = _height - ZONE; y <= yend; ++y) {
		// Initialize a full hash block in the upper left of this row
		u32 hash = 0;
		for (int ii = 0; ii < ZONE; ++ii) {
			for (int jj = 0; jj < ZONE; ++jj) {
				u32 *p = (u32*)&rgba[(ii + jj * width)*4];
				hash += hashPixel(*p);
			}
		}

		// For each column,
		u16 x = 0, xend = width - ZONE;
		for (;;) {
			// If a previous zone had this hash,
			u32 match = _table[hash & TABLE_MASK];
			if (match != TABLE_NULL) {
				// Lookup the location for the potential match
				u16 sx = (u16)(match >> 16);
				u16 sy = (u16)match;

				// If the match was genuine,
				if (checkMatch(sx, sy, x, y)) {
					++Stats.initial_matches;

					// See how far the match can be expanded
					u16 dx = x, dy = y, w = ZONE, h = ZONE;
					expandMatch(sx, sy, dx, dy, w, h);

					// If the match scores well,
					int unused = score(dx, dy, w, h);
					if (unused >= MIN_SCORE) {
						// Accept it
						add(unused, sx, sy, dx, dy, w, h);

						if (_exact_matches.size() >= ImageLZReader::MAX_ZONE_COUNT) {
							CAT_WARN("lz") << "WARNING: Ran out of LZ zones so compression is not optimal.  Maybe we should allow more?  Please report this warning.";
							break;
						}
					}
				} else {
					++Stats.collisions;
				}
			}

			// Insert this zone into the hash table
			_table[hash & TABLE_MASK] = ((u32)x << 16) | y;

			// If exceeded end,
			if (++x >= xend) {
				break;
			}

			// Roll the hash to the next zone one pixel over
			for (int jj = 0; jj < ZONE; ++jj) {
				u32 *lp = (u32*)&rgba[(x + (y + jj) * width)*4];
				hash -= hashPixel(*lp);
				u32 *rp = (u32*)&rgba[((x + ZONE) + (y + jj) * width)*4];
				hash += hashPixel(*rp);
			}
		}
	}

	return WE_OK;
}

void ImageLZWriter::write(ImageWriter &writer) {
	int match_count = (int)_exact_matches.size();

#ifdef CAT_COLLECT_STATS
	u32 bitcount = 16;
	Stats.bytes_overhead_uncompressed = match_count * 10;
#endif

	writer.writeBits(match_count, 16);

	// If no matches to record,
	if (match_count <= 0) {
		// Just stop here
		return;
	}

	// Collect frequency statistics
	u16 last_dx = 0, last_dy = 0;
	u16 freqs[256];
	FreqHistogram<256> hist;

	for (int ii = 0; ii < match_count; ++ii) {
		Match *m = &_exact_matches[ii];

		// Apply some context modeling for better compression
		u16 edx = m->dx;
		u16 esy = m->dy - m->sy;
		u16 edy = m->dy - last_dy;
		if (edy == 0) {
			edx -= last_dx;
		}

		hist.add((u8)m->sx);
		hist.add((u8)(m->sx >> 8));
		hist.add((u8)esy);
		hist.add((u8)(esy >> 8));
		hist.add((u8)edx);
		hist.add((u8)(edx >> 8));
		hist.add((u8)edy);
		hist.add((u8)(edy >> 8));
		hist.add(m->w);
		hist.add(m->h);

		last_dx = m->dx;
		last_dy = m->dy;
	}

	u16 codes[256];
	u8 codelens[256];
	hist.generateHuffman(codes, codelens);

	for (int ii = 0; ii < 256; ++ii) {
		u8 len = codelens[ii];

		if (len >= 15) {
			writer.writeBits(15, 4);
			writer.writeBit(len - 15);
#ifdef CAT_COLLECT_STATS
			bitcount++;
#endif
		} else {
			writer.writeBits(len, 4);
		}

#ifdef CAT_COLLECT_STATS
		bitcount += 4;
#endif
	}

	// Reset last for encoding
	last_dx = 0;
	last_dy = 0;

	for (int ii = 0; ii < match_count; ++ii) {
		Match *m = &_exact_matches[ii];

		// Apply some context modeling for better compression
		u16 edx = m->dx;
		u16 esy = m->dy - m->sy;
		u16 edy = m->dy - last_dy;
		if (edy == 0) {
			edx -= last_dx;
		}

		u8 sym = (u8)m->sx;
		writer.writeBits(codes[sym], codelens[sym]);
#ifdef CAT_COLLECT_STATS
		bitcount += codelens[sym];
#endif
		sym = (u8)(m->sx >> 8);
		writer.writeBits(codes[sym], codelens[sym]);
#ifdef CAT_COLLECT_STATS
		bitcount += codelens[sym];
#endif
		sym = (u8)esy;
		writer.writeBits(codes[sym], codelens[sym]);
#ifdef CAT_COLLECT_STATS
		bitcount += codelens[sym];
#endif
		sym = (u8)(esy >> 8);
		writer.writeBits(codes[sym], codelens[sym]);
#ifdef CAT_COLLECT_STATS
		bitcount += codelens[sym];
#endif
		sym = (u8)edx;
		writer.writeBits(codes[sym], codelens[sym]);
#ifdef CAT_COLLECT_STATS
		bitcount += codelens[sym];
#endif
		sym = (u8)(edx >> 8);
		writer.writeBits(codes[sym], codelens[sym]);
#ifdef CAT_COLLECT_STATS
		bitcount += codelens[sym];
#endif
		sym = (u8)edy;
		writer.writeBits(codes[sym], codelens[sym]);
#ifdef CAT_COLLECT_STATS
		bitcount += codelens[sym];
#endif
		sym = (u8)(edy >> 8);
		writer.writeBits(codes[sym], codelens[sym]);
#ifdef CAT_COLLECT_STATS
		bitcount += codelens[sym];
#endif
		sym = m->w;
		writer.writeBits(codes[sym], codelens[sym]);
#ifdef CAT_COLLECT_STATS
		bitcount += codelens[sym];
#endif
		sym = m->h;
		writer.writeBits(codes[sym], codelens[sym]);
#ifdef CAT_COLLECT_STATS
		bitcount += codelens[sym];
#endif

		last_dx = m->dx;
		last_dy = m->dy;
	}

#ifdef CAT_COLLECT_STATS
	Stats.huff_bits = bitcount;
	Stats.covered_percent = Stats.covered * 100. / (double)(_width * _height);
	Stats.bytes_saved = Stats.covered * 3;
	Stats.bytes_overhead = bitcount / 8;
	Stats.compression_ratio = Stats.bytes_saved / (double)Stats.bytes_overhead;
#endif
}

#ifdef CAT_COLLECT_STATS

bool ImageLZWriter::dumpStats() {
	CAT_INFO("stats") << "(LZ Compress) Initial matches : " << Stats.initial_matches;
	CAT_INFO("stats") << "(LZ Compress) Initial collisions : " << Stats.collisions;
	CAT_INFO("stats") << "(LZ Compress) Matched amount : " << Stats.covered_percent << "% of file is redundant";
	CAT_INFO("stats") << "(LZ Compress) Bytes saved : " << Stats.bytes_saved << " bytes";
	CAT_INFO("stats") << "(LZ Compress) Raw overhead : " << Stats.bytes_overhead_uncompressed << " bytes before compression";
	CAT_INFO("stats") << "(LZ Compress) Compressed overhead : " << Stats.bytes_overhead << " bytes to transmit";
	CAT_INFO("stats") << "(LZ Compress) Compression ratio : " << Stats.compression_ratio << ":1";

	return true;
}

#endif

