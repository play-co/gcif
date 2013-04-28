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

#include "ImageLZWriter.hpp"
#include "EndianNeutral.hpp"
#include "EntropyEncoder.hpp"
#include "ImageLZReader.hpp"
#include "Log.hpp"
using namespace cat;

#include <algorithm> // std::sort


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
	//key <<= 8; // Remove alpha

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

int ImageLZWriter::initFromRGBA(const u8 *rgba, int width, int height, const GCIFKnobs *knobs) {
	clear();

	if (width < ZONEW || height < ZONEH) {
		return WE_BAD_DIMS;
	}

	_knobs = knobs;
	_rgba = rgba;
	_width = width;
	_height = height;

	if (knobs->lz_tableBits < 0 || knobs->lz_tableBits > 24 ||
		knobs->lz_nonzeroCoeff <= 0) {
		return WE_BAD_PARAMS;
	}

	_table_size = 1 << knobs->lz_tableBits;
	_table_mask = _table_size - 1;

	_table = new u32[_table_size];
	memset(_table, 0xff, _table_size * sizeof(u32));

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

	for (int ii = 0; ii < ZONEW; ++ii) {
		for (int jj = 0; jj < ZONEH; ++jj) {
			if (visited(x + ii, y + jj)) {
				return false;
			}

			u32 *p = (u32*)&rgba[((x + ii) + (y + jj) * width)*4];
			u32 *mp = (u32*)&rgba[((mx + ii) + (my + jj) * width)*4];

			// If RGB components match,
			//if (((getLE(*p) ^ getLE(*mp)) << 8) != 0) {
			if (getLE(*p) ^ getLE(*mp)) {
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
	while (w + 1 <= MAXW && sx + w < width && dx + w < width) {
		for (int jj = 0; jj < h; ++jj) {
			if (visited(dx + w, dy + jj)) {
				goto try_down;
			}

			u32 *sp = (u32*)&rgba[((sx + w) + (sy + jj) * width)*4];
			u32 *dp = (u32*)&rgba[((dx + w) + (dy + jj) * width)*4];

			// If RGB components match,
			//if (((getLE(*sp) ^ getLE(*dp)) << 8) != 0) {
			if (getLE(*sp) ^ getLE(*dp)) {
				goto try_down;
			}
		}

		// Widen width
		++w;
	}

	// Try expanding down
try_down:
	while (h + 1 <= MAXH && sy + h < height && dy + h < height) {
		for (int jj = 0; jj < w; ++jj) {
			if (visited(dx + jj, dy + h)) {
				goto try_left;
			}

			u32 *sp = (u32*)&rgba[((sx + jj) + (sy + h) * width)*4];
			u32 *dp = (u32*)&rgba[((dx + jj) + (dy + h) * width)*4];

			// If RGB components match,
			//if (((getLE(*sp) ^ getLE(*dp)) << 8) != 0) {
			if (getLE(*sp) ^ getLE(*dp)) {
				goto try_left;
			}
		}

		// Heighten height
		++h;
	}

	// Try expanding left
try_left:
	while (w + 1 <= MAXW && sx >= 1 && dx >= 1) {
		for (int jj = 0; jj < h; ++jj) {
			if (visited(dx - 1, dy + jj)) {
				goto try_up;
			}

			u32 *sp = (u32*)&rgba[((sx - 1) + (sy + jj) * width)*4];
			u32 *dp = (u32*)&rgba[((dx - 1) + (dy + jj) * width)*4];

			// If RGB components match,
			//if (((getLE(*sp) ^ getLE(*dp)) << 8) != 0) {
			if (getLE(*sp) ^ getLE(*dp)) {
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
	while (h + 1 <= MAXH && sy >= 1 && dy >= 1) {
		for (int jj = 0; jj < w; ++jj) {
			if (visited(dx + jj, dy - 1)) {
				return true;
			}

			u32 *sp = (u32*)&rgba[((sx + jj) + (sy - 1) * width)*4];
			u32 *dp = (u32*)&rgba[((dx + jj) + (dy - 1) * width)*4];

			// If RGB components match,
			//if (((getLE(*sp) ^ getLE(*dp)) << 8) != 0) {
			if (getLE(*sp) ^ getLE(*dp)) {
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
					sum += _knobs->lz_nonzeroCoeff;
				} else {
					sum += 1;
				}
			}
		}
	}

	return sum / _knobs->lz_nonzeroCoeff;
}

void ImageLZWriter::add(int unused, u16 sx, u16 sy, u16 dx, u16 dy, u16 w, u16 h) {
	// Mark pixels as visited
	for (int ii = 0; ii < w; ++ii) {
		for (int jj = 0; jj < h; ++jj) {
			visit(dx + ii, dy + jj);
		}
	}

	Match m = {
		sx, sy, dx, dy, w - ZONEW, h - ZONEH
	};

	_exact_matches.push_back(m);

#ifdef CAT_COLLECT_STATS
	Stats.covered += w * h;
#endif
}

int ImageLZWriter::match() {
	if (!_rgba) {
		return WE_BUG;
	}

	CAT_INANE("LZ") << "Searching for matches with " << _table_size << "-entry hash table...";

	const u8 *rgba = _rgba;
	const int width = _width;

#ifdef CAT_COLLECT_STATS
	Stats.collisions = 0;
	Stats.initial_matches = 0;
	Stats.covered = 0;
#endif

	// For each raster,
	for (u16 y = 0, yend = _height - ZONEH; y <= yend; ++y) {
		// Initialize a full hash block in the upper left of this row
		u32 hash = 0;
		for (int ii = 0; ii < ZONEW; ++ii) {
			for (int jj = 0; jj < ZONEH; ++jj) {
				u32 *p = (u32*)&rgba[(ii + jj * width)*4];
				hash += hashPixel(*p);
			}
		}

		// For each column,
		u16 x = 0, xend = width - ZONEW;
		for (;;) {
			// If a previous zone had this hash,
			u32 match = _table[hash & _table_mask];
			if (match != TABLE_NULL) {
				// Lookup the location for the potential match
				u16 sx = (u16)(match >> 16);
				u16 sy = (u16)match;

				// If the match was genuine,
				if (checkMatch(sx, sy, x, y)) {
#ifdef CAT_COLLECT_STATS
					++Stats.initial_matches;
#endif

					// See how far the match can be expanded
					u16 dx = x, dy = y, w = ZONEW, h = ZONEH;
					expandMatch(sx, sy, dx, dy, w, h);

					// If the match scores well,
					int unused = score(dx, dy, w, h);
					if (unused >= _knobs->lz_minScore) {
						// Accept it
						add(unused, sx, sy, dx, dy, w, h);

						if (_exact_matches.size() >= ImageLZReader::MAX_ZONE_COUNT) {
							CAT_WARN("lz") << "WARNING: Ran out of LZ zones so compression is not optimal.  Maybe we should allow more?  Please report this warning.";
							break;
						}
					}
				} else {
#ifdef CAT_COLLECT_STATS
					++Stats.collisions;
#endif
				}
			}

			// Insert this zone into the hash table
			_table[hash & _table_mask] = ((u32)x << 16) | y;

			// If exceeded end,
			if (++x >= xend) {
				break;
			}

			// Roll the hash to the next zone one pixel over
			for (int jj = 0; jj < ZONEH; ++jj) {
				u32 *lp = (u32*)&rgba[(x + (y + jj) * width)*4];
				hash -= hashPixel(*lp);
				u32 *rp = (u32*)&rgba[((x + ZONEW) + (y + jj) * width)*4];
				hash += hashPixel(*rp);
			}
		}
	}

	return WE_OK;
}

bool ImageLZWriter::matchSortCompare(const Match &i, const Match &j) {
	return i.dy < j.dy || (i.dy == j.dy && i.dx < j.dx);
}

void ImageLZWriter::sortMatches() {
	std::sort(_exact_matches.begin(), _exact_matches.end(), matchSortCompare);
}

void ImageLZWriter::write(ImageWriter &writer) {
	const int match_count = (int)_exact_matches.size();

	sortMatches();

#ifdef CAT_COLLECT_STATS
	Stats.match_count = match_count;
#endif

	writer.writeBits(match_count, 16);

	// If no matches to record,
	if (match_count <= 0) {
		// Just stop here
#ifdef CAT_COLLECT_STATS
		Stats.huff_bits = 16;
		Stats.covered_percent = 0;
		Stats.bytes_saved = 0;
		Stats.bytes_overhead = Stats.huff_bits / 8;
		Stats.compression_ratio = 0;
#endif
		return;
	}

	bool compressing = match_count >= _knobs->lz_huffThresh;
	writer.writeBit(compressing);

	if (!compressing) {
		u16 last_dx = 0, last_dy = 0;

		int bits = 0;
		for (int ii = 0; ii < match_count; ++ii) {
			Match *m = &_exact_matches[ii];

			//CAT_WARN("LZ") << m->sx << ", " << m->sy << " -> " << m->dx << ", " << m->dy << " [" << (m->w + ZONEW) << ", " << (m->h + ZONEH) << "]";

			// Apply some context modeling for better compression
			u16 edx = m->dx;
			u16 esy = m->dy - m->sy;
			u16 edy = m->dy - last_dy;
			if (edy == 0) {
				edx -= last_dx;
			}

			bits += writer.write9(m->sx);
			bits += writer.write9(esy);
			bits += writer.write9(edx);
			bits += writer.write9(edy);
			writer.writeBits(m->w, 8);
			writer.writeBits(m->h, 8);
			bits += 8+8;

			last_dx = m->dx;
			last_dy = m->dy;
		}
#ifdef CAT_COLLECT_STATS
		Stats.huff_bits = bits + 1 + 16;
		Stats.covered_percent = Stats.covered * 100. / (double)(_width * _height);
		Stats.bytes_saved = Stats.covered * 4;
		Stats.bytes_overhead = Stats.huff_bits / 8;
		Stats.compression_ratio = Stats.bytes_saved / (double)Stats.bytes_overhead;
#endif

		return;
	}

	// Collect frequency statistics
	u16 last_dx = 0, last_dy = 0;

	EntropyEncoder<256, ENCODER_ZRLE_SYMS> encoder;

	for (int ii = 0; ii < match_count; ++ii) {
		Match *m = &_exact_matches[ii];

		// Apply some context modeling for better compression
		u16 edx = m->dx;
		u16 esy = m->dy - m->sy;
		u16 edy = m->dy - last_dy;
		if (edy == 0) {
			edx -= last_dx;
		}

		encoder.add((u8)m->sx);
		encoder.add((u8)(m->sx >> 8));
		encoder.add((u8)esy);
		encoder.add((u8)(esy >> 8));
		encoder.add((u8)edx);
		encoder.add((u8)(edx >> 8));
		encoder.add((u8)edy);
		encoder.add((u8)(edy >> 8));
		encoder.add(m->w);
		encoder.add(m->h);

		last_dx = m->dx;
		last_dy = m->dy;
	}

	encoder.finalize();

	int bits = encoder.writeTables(writer) + 1 + 16;

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
		bits += encoder.write(sym, writer);
		sym = (u8)(m->sx >> 8);
		bits += encoder.write(sym, writer);
		sym = (u8)esy;
		bits += encoder.write(sym, writer);
		sym = (u8)(esy >> 8);
		bits += encoder.write(sym, writer);
		sym = (u8)edx;
		bits += encoder.write(sym, writer);
		sym = (u8)(edx >> 8);
		bits += encoder.write(sym, writer);
		sym = (u8)edy;
		bits += encoder.write(sym, writer);
		sym = (u8)(edy >> 8);
		bits += encoder.write(sym, writer);
		sym = m->w;
		bits += encoder.write(sym, writer);
		sym = m->h;
		bits += encoder.write(sym, writer);

		last_dx = m->dx;
		last_dy = m->dy;
	}

#ifdef CAT_COLLECT_STATS
	Stats.huff_bits = bits;
	Stats.covered_percent = Stats.covered * 100. / (double)(_width * _height);
	Stats.bytes_saved = Stats.covered * 4;
	Stats.bytes_overhead = bits / 8;
	Stats.compression_ratio = Stats.bytes_saved / (double)Stats.bytes_overhead;
#endif
}

bool ImageLZWriter::findExtent(int x, int y, int &w, int &h) {
	for (int ii = 0, iiend = _exact_matches.size(); ii < iiend; ++ii) {
		Match *m = &_exact_matches[ii];
		const int mw = m->w + ZONEW;
		const int mh = m->h + ZONEH;

		if (m->dx <= x && m->dy <= y && m->dx + mw > x && m->dy + mh > y) {
			w = mw + m->dx - x;
			h = mh + m->dy - y;
			return true;
		}
	}

	return false;
}

#ifdef CAT_COLLECT_STATS

bool ImageLZWriter::dumpStats() {
	CAT_INANE("stats") << "(LZ Compress) Initial collisions : " << Stats.collisions;
	CAT_INANE("stats") << "(LZ Compress) Initial matches : " << Stats.initial_matches << " used " << Stats.match_count;
	CAT_INANE("stats") << "(LZ Compress) Matched amount : " << Stats.covered_percent << "% of file is redundant (" << Stats.covered << " of " << _width * _height << " pixels)";
	CAT_INANE("stats") << "(LZ Compress) Bytes saved : " << Stats.bytes_saved << " bytes";
	CAT_INANE("stats") << "(LZ Compress) Compressed overhead : " << Stats.bytes_overhead << " bytes to transmit";
	CAT_INANE("stats") << "(LZ Compress) Compression ratio : " << Stats.compression_ratio << ":1";

	return true;
}

#endif

