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

#include "ImageRGBAWriter.hpp"
#include "../decoder/BitMath.hpp"
#include "../decoder/Filters.hpp"
#include "EntropyEstimator.hpp"
#include "Log.hpp"
#include "ImageLZWriter.hpp"
using namespace cat;

#include <vector>
using namespace std;

#include "../decoder/lz4.h"
#include "lz4hc.h"
#include "Log.hpp"
#include "HuffmanEncoder.hpp"
#include "lodepng.h"

#ifdef CAT_DESYNCH_CHECKS
#define DESYNC(x, y) writer.writeBits(x ^ 12345, 16); writer.writeBits(y ^ 54321, 16);
#define DESYNC_FILTER(x, y) writer.writeBits(x ^ 31337, 16); writer.writeBits(y ^ 31415, 16);
#else
#define DESYNC(x, y)
#define DESYNC_FILTER(x, y)
#endif


//// ImageRGBAWriter

void ImageRGBAWriter::clear() {
	if (_sf_tiles) {
		delete []_sf_tiles;
		_sf_tiles = 0;
	}
	if (_cf_tiles) {
		delete []_cf_tiles;
		_cf_tiles = 0;
	}
	if (_seen_filter) {
		delete []_seen_filter;
		_seen_filter = 0;
	}
}

void ImageRGBAWriter::maskTiles() {
	const u16 tile_size_x = _tile_size_x, tile_size_y = _tile_size_y;
	const u16 size_x = _size_x, size_y = _size_y;
	u8 *sf = _sf_tiles;
	u8 *cf = _cf_tiles;

	// For each tile,
	for (u16 y = 0; y < size_y; y += tile_size_y) {
		for (u16 x = 0; x < size_x; x += tile_size_x) {

			// For each element in the tile,
			u16 py = y, cy = tile_size_y;
			while (cy-- > 0 && py < size_y) {
				u16 px = x, cx = tile_size_x;
				while (cx-- > 0 && px < size_x) {
					// If it is not masked,
					if (!IsMasked(px, py)) {
						// We need to do this tile
						*cf++ = TODO_TILE;
						*sf++ = TODO_TILE;
						goto next_tile;
					}
				}
				++py;
			}

			// Tile is masked out entirely
			*cf++ = MASK_TILE;
			*sf++ = MASK_TILE;
next_tile:;
		}
	}
}

void ImageRGBAWriter::designFilters() {
	// If disabled,
	if (!_knobs->cm_designFilters) {
		CAT_INANE("CM") << "Skipping filter design";
		return;
	}

	/* Inputs: A, B, C, D same as described in Filters.hpp
	 *
	 * PRED = (a*A + b*B + c*C + d*D) / 2
	 * a,b,c,d = {-4, -3, -2, -1, 0, 1, 2, 3, 4}
	 */

	FilterScorer scores;
	scores.init(SF_COUNT);

	int hist[SF_COUNT] = {0};
	u8 FPT[3];

	CAT_INANE("CM") << "Designing filters...";

	const u16 tile_size_x = _tile_size_x, tile_size_y = _tile_size_y;
	const u16 size_x = _size_x, size_y = _size_y;

	u8 *sf = _sf_tiles;
	u8 *cf = _cf_tiles;
	const u8 *topleft = _rgba;
	for (int y = 0; y < _size_y; y += _tile_size_y) {
		for (int x = 0; x < _size_x; x += _tile_size_x, ++sf, ++cf, topleft += _tile_size_x * 4) {
			if (*sf == MASK_TILE) {
				continue;
			}

			scores.reset();

			// For each element in the tile,
			const u8 *row = topleft;
			u16 py = y, cy = tile_size_y;
			while (cy-- > 0 && py < size_y) {
				const u8 *data = row;
				u16 px = x, cx = tile_size_x;
				while (cx-- > 0 && px < size_x) {
					// If element is not masked,
					if (!IsMasked(px, py)) {
						const u8 r = data[0];
						const u8 g = data[1];
						const u8 b = data[2];

						for (int f = 0; f < SF_COUNT; ++f) {
							const u8 *pred = RGBA_FILTERS[f].safe(data, FPT, x, y, size_x);

							u8 rr = r - pred[0];
							u8 rg = g - pred[1];
							u8 rb = b - pred[2];

							int score = RGBAChaos::ResidualScore(rr);
							score += RGBAChaos::ResidualScore(rg);
							score += RGBAChaos::ResidualScore(rb);

							scores.add(f, score);
						}
					}
					data += 4;
				}
				++py;
				row += size_x * 4;
			}

			FilterScorer::Score *top = scores.getTop(4, true);
			hist[top[0].index] += 5;
			hist[top[1].index] += 3;
			hist[top[2].index] += 1;
			hist[top[3].index] += 1;
		}
	}

	// Copy fixed functions
	for (int jj = 0; jj < SF_FIXED; ++jj) {
		_sf_indices[jj] = jj;
		_sf[jj] = RGBA_FILTERS[jj];
	}

	// Replace filters
	for (int jj = 0; jj < SF_COUNT; ++jj) {
		// Find worst default filter
		int lowest_sf = 0x7fffffffUL, lowest_index = 0;

		for (int ii = 0; ii < SF_COUNT; ++ii) {
			if (hist[ii] < lowest_sf) {
				lowest_sf = hist[ii];
				lowest_index = ii;
			}
		}

		// Find best custom filter
		int best_tap = -1, highest_index = -1;

		for (int ii = 0; ii < TAPPED_COUNT; ++ii) {
			int score = bestHist[ii + SF_COUNT];

			if (score > best_tap) {
				best_tap = score;
				highest_index = ii;
			}
		}

		// If it not an improvement,
		if (best_tap <= lowest_sf) {
			break;
		}

		// Verify it is good enough to bother with
		double ratio = best_tap / (double)lowest_sf;
		if (ratio < _knobs->cm_minTapQuality) {
			break;
		}

		// Insert it at this location
		const int a = SpatialFilterSet::FILTER_TAPS[highest_index][0];
		const int b = SpatialFilterSet::FILTER_TAPS[highest_index][1];
		const int c = SpatialFilterSet::FILTER_TAPS[highest_index][2];
		const int d = SpatialFilterSet::FILTER_TAPS[highest_index][3];

		CAT_INANE("CM") << "Replacing default filter " << lowest_index << " with tapped filter " << highest_index << " that is " << ratio << "x more preferable : PRED = (" << a << "A + " << b << "B + " << c << "C + " << d << "D) / 2";

		_filter_replacements.push_back((lowest_index << 16) | highest_index);

		_sf_set.replace(lowest_index, highest_index);

		// Install grave markers
		bestHist[lowest_index] = 0x7fffffffUL;
		bestHist[highest_index + SF_COUNT] = 0;
	}
}

void ImageRGBAWriter::decideFilters() {
	EntropyEstimator ee[3];
	ee[0].init();
	ee[1].init();
	ee[2].init();

	FilterScorer scores;
	scores.init(SF_COUNT * CF_COUNT);

	const int width = _width;

	if (!_knobs->cm_disableEntropy) {
		CAT_INANE("CM") << "Scoring filters using " << _knobs->cm_filterSelectFuzz << " entropy-based trials...";
	} else {
		CAT_INANE("CM") << "Scoring filters using L1-norm...";
	}

	int passes = 0;
	int revisitCount = _knobs->cm_revisitCount;
	u8 FPT[3];

	for (;;) {
		for (int y = 0; y < _height; y += FILTER_ZONE_SIZE_H) {
			for (int x = 0; x < width; x += FILTER_ZONE_SIZE_W) {
				// If this zone is skipped,
				const u16 filter = getSpatialFilter(x, y);
				if (filter == UNUSED_FILTER) {
					continue;
				}

				// Determine best filter combination to use
				int bestSF = 0, bestCF = 0;

				// If we are on the second or later pass,
				if (passes > 0) {
					// If just finished revisiting old zones,
					if (--revisitCount < 0) {
						// Done!
						return;
					}

					bestSF = (u8)(filter >> 8);
					bestCF = (u8)filter;

					u8 codes[3][FILTER_ZONE_SIZE_W*FILTER_ZONE_SIZE_H];
					int count = 0;

					// For each pixel in the zone,
					for (int yy = 0; yy < FILTER_ZONE_SIZE_H; ++yy) {
						for (int xx = 0; xx < FILTER_ZONE_SIZE_W; ++xx) {
							int px = x + xx, py = y + yy;
							if (px >= _width || py >= _height) {
								continue;
							}
							if (_mask->masked(px, py)) {
								continue;
							}
							if (_lz->visited(px, py)) {
								continue;
							}

							const u8 *p = _rgba + (px + py * width) * 4;

							const u8 *pred = FPT;
							_sf_set.get(bestSF).safe(p, &pred, px, py, width);
							u8 temp[3];
							for (int jj = 0; jj < 3; ++jj) {
								temp[jj] = p[jj] - pred[jj];
							}

							u8 yuv[3];
							RGB2YUV_FILTERS[bestCF](temp, yuv);

							codes[0][count] = yuv[0];
							codes[1][count] = yuv[1];
							codes[2][count] = yuv[2];
							++count;
						}
					}

					// Subtract old choice back out
					ee[0].subtract(codes[0], count);
					ee[1].subtract(codes[1], count);
					ee[2].subtract(codes[2], count);
				}

				scores.reset();

				// For each pixel in the zone,
				for (int yy = 0; yy < FILTER_ZONE_SIZE_H; ++yy) {
					for (int xx = 0; xx < FILTER_ZONE_SIZE_W; ++xx) {
						int px = x + xx, py = y + yy;
						if (px >= _width || py >= _height) {
							continue;
						}
						if (_mask->masked(px, py)) {
							continue;
						}
						if (_lz->visited(px, py)) {
							continue;
						}

						const u8 *p = _rgba + (px + py * width) * 4;

						for (int ii = 0; ii < SF_COUNT; ++ii) {
							const u8 *pred = FPT;
							_sf_set.get(ii).safe(p, &pred, px, py, width);
							u8 temp[3];
							for (int jj = 0; jj < 3; ++jj) {
								temp[jj] = p[jj] - pred[jj];
							}

							for (int jj = 0; jj < CF_COUNT; ++jj) {
								u8 yuv[3];
								RGB2YUV_FILTERS[jj](temp, yuv);

								int error = scoreYUV(yuv);

								scores.add(ii + SF_COUNT*jj, error);
							}
						}
					}
				}

				FilterScorer::Score *lowest = scores.getLowest();

				if (_knobs->cm_disableEntropy ||
						lowest->score <= _knobs->cm_maxEntropySkip) {
					bestSF = lowest->index % SF_COUNT;
					bestCF = lowest->index / SF_COUNT;

					if (!_knobs->cm_disableEntropy) {
						u8 codes[3][FILTER_ZONE_SIZE_W*FILTER_ZONE_SIZE_H];
						int count = 0;

						// Record this choice
						for (int yy = 0; yy < FILTER_ZONE_SIZE_H; ++yy) {
							for (int xx = 0; xx < FILTER_ZONE_SIZE_W; ++xx) {
								int px = x + xx, py = y + yy;
								if (px >= _width || py >= _height) {
									continue;
								}
								if (_mask->masked(px, py)) {
									continue;
								}
								if (_lz->visited(px, py)) {
									continue;
								}

								const u8 *p = _rgba + (px + py * width) * 4;
								const u8 *pred = FPT;
								_sf_set.get(bestSF).safe(p, &pred, px, py, width);
								u8 temp[3];
								for (int jj = 0; jj < 3; ++jj) {
									temp[jj] = p[jj] - pred[jj];
								}

								u8 yuv[3];
								RGB2YUV_FILTERS[bestCF](temp, yuv);

								codes[0][count] = yuv[0];
								codes[1][count] = yuv[1];
								codes[2][count] = yuv[2];
								++count;
							}
						}

						ee[0].add(codes[0], count);
						ee[1].add(codes[1], count);
						ee[2].add(codes[2], count);
					}
				} else {
					int TOP_COUNT = _knobs->cm_filterSelectFuzz;
					if (TOP_COUNT > SF_COUNT*CF_COUNT) {
						TOP_COUNT = SF_COUNT*CF_COUNT;
					}

					FilterScorer::Score *top = scores.getTop(TOP_COUNT, _knobs->cm_sortFilters);

					u32 best_entropy = 0x7fffffff; // lower = better
					u8 best_codes[3][FILTER_ZONE_SIZE_W * FILTER_ZONE_SIZE_H];
					int best_count;

					for (int ii = 0; ii < TOP_COUNT; ++ii) {
						const int index = top[ii].index;
						u8 sf = index % SF_COUNT;
						u8 cf = index / SF_COUNT;

						u8 codes[3][FILTER_ZONE_SIZE_W*FILTER_ZONE_SIZE_H];
						int count = 0;

						for (int yy = 0; yy < FILTER_ZONE_SIZE_H; ++yy) {
							for (int xx = 0; xx < FILTER_ZONE_SIZE_W; ++xx) {
								int px = x + xx, py = y + yy;
								if (px >= _width || py >= _height) {
									continue;
								}
								if (_mask->masked(px, py)) {
									continue;
								}
								if (_lz->visited(px, py)) {
									continue;
								}

								const u8 *p = _rgba + (px + py * width) * 4;
								const u8 *pred = FPT;
								_sf_set.get(sf).safe(p, &pred, px, py, width);
								u8 temp[3];
								for (int jj = 0; jj < 3; ++jj) {
									temp[jj] = p[jj] - pred[jj];
								}

								u8 yuv[3];
								RGB2YUV_FILTERS[cf](temp, yuv);

								codes[0][count] = yuv[0];
								codes[1][count] = yuv[1];
								codes[2][count] = yuv[2];
								++count;
							}
						}

						u32 entropy = ee[0].entropy(codes[0], count)
							+ ee[1].entropy(codes[1], count)
							+ ee[2].entropy(codes[2], count);

						if (best_entropy > entropy) {
							best_entropy = entropy;
							memcpy(best_codes, codes, sizeof(best_codes));
							best_count = count;

							bestSF = sf;
							bestCF = cf;
						}
					}

					ee[0].add(best_codes[0], best_count);
					ee[1].add(best_codes[1], best_count);
					ee[2].add(best_codes[2], best_count);
				}

				// Set filter for this zone
				setSpatialFilter(x, y, ((u16)bestSF << 8) | bestCF);
			}
		}

		// After good statistics are collected, revisit the first few zones
		if (revisitCount <= 0) {
			// Exit now!
			return;
		}

		if (passes < 4) {
			CAT_INANE("CM") << "Revisiting filter selections from the top... " << revisitCount << " left";
		}
		++passes;
	}
}

void ImageRGBAWriter::initializeEncoders() {
	// Find number of pixels to encode
	int chaos_count = 0;
	for (int y = 0; y < _height; ++y) {
		for (int x = 0; x < _width; ++x) {
			if (!_lz->visited(x, y) && !_mask->masked(x, y)) {
				++chaos_count;
			}
		}
	}

#ifdef CAT_COLLECT_STATS
	Stats.chaos_count = chaos_count;
#endif

	// If it is above a threshold,
	if (chaos_count >= _knobs->cm_chaosThresh) {
		CAT_DEBUG_ENFORCE(CHAOS_LEVELS_MAX == 8);

		// Use more chaos levels for better compression
		_chaos_levels = CHAOS_LEVELS_MAX;
		_chaos_table = CHAOS_TABLE_8;
	} else {
		_chaos_levels = 1;
		_chaos_table = CHAOS_TABLE_1;
	}

	const int width = _width;

	// For each scanline,
	const u8 *p = _rgba;
	u8 *lastStart = _chaos + COLOR_PLANES;
	CAT_CLR(_chaos, _chaos_size);

	const u8 *CHAOS_TABLE = _chaos_table;
	u8 FPT[3];

	for (int y = 0; y < _height; ++y) {
		u8 *last = lastStart;

		// Zero left
		last[0 - 4] = 0;
		last[1 - 4] = 0;
		last[2 - 4] = 0;
		last[3 - 4] = 0;

		// For each pixel,
		for (int x = 0; x < width; ++x) {
			// If not masked out,
			if (!_lz->visited(x, y) && !_mask->masked(x, y)) {
				// Get filter for this pixel
				const u16 filter = getSpatialFilter(x, y);
				const u8 cf = (u8)filter;
				const u8 sf = (u8)(filter >> 8);

				// Apply spatial filter
				const u8 *pred = FPT;
				_sf_set.get(sf).safe(p, &pred, x, y, width);
				u8 temp[3];
				for (int jj = 0; jj < 3; ++jj) {
					temp[jj] = p[jj] - pred[jj];
				}

				// Apply color filter
				u8 yuv[COLOR_PLANES];
				RGB2YUV_FILTERS[cf](temp, yuv);
				if (x > 0) {
					yuv[3] = p[-1] - p[3];
				} else {
					yuv[3] = 255 - p[3];
				}

				u8 chaos = CHAOS_TABLE[CHAOS_SCORE[last[0 - 4]] + CHAOS_SCORE[last[0]]];
				_y_encoder[chaos].add(yuv[0]);
				chaos = CHAOS_TABLE[CHAOS_SCORE[last[1 - 4]] + CHAOS_SCORE[last[1]]];
				_u_encoder[chaos].add(yuv[1]);
				chaos = CHAOS_TABLE[CHAOS_SCORE[last[2 - 4]] + CHAOS_SCORE[last[2]]];
				_v_encoder[chaos].add(yuv[2]);
				chaos = CHAOS_TABLE[CHAOS_SCORE[last[3 - 4]] + CHAOS_SCORE[last[3]]];
				_a_encoder[chaos].add(yuv[3]);

				for (int c = 0; c < COLOR_PLANES; ++c) {
					last[c] = yuv[c];
				}
			} else {
				for (int c = 0; c < COLOR_PLANES; ++c) {
					last[c] = 0;
				}
			}

			// Next pixel
			last += COLOR_PLANES;
			p += 4;
		}
	}

	// Finalize
	for (int jj = 0; jj < _chaos.getBinCount(); ++jj) {
		_y_encoder[jj].finalize();
		_u_encoder[jj].finalize();
		_v_encoder[jj].finalize();
		_a_encoder[jj].finalize();
	}
}

bool ImageRGBAWriter::IsMasked(u16 x, u16 y) {
	return _mask->masked(x, y) && _lz->visited(x, y);
}

int ImageRGBAWriter::init(const u8 *rgba, int size_x, int size_y, ImageMaskWriter &mask, ImageLZWriter &lz, const GCIFKnobs *knobs) {
	int err;

	_knobs = knobs;
	_rgba = rgba;
	_mask = &mask;
	_lz = &lz;

	if (size_x < 0 || size_y < 0) {
		return GCIF_WE_BAD_DIMS;
	}

	if ((!knobs->cm_disableEntropy && knobs->cm_filterSelectFuzz <= 0)) {
		return GCIF_WE_BAD_PARAMS;
	}

	_size_x = size_x;
	_size_y = size_y;

	// Use constant tile size of 4x4 for now
	_tile_bits_x = 2;
	_tile_bits_y = 2;
	_tile_size_x = 1 << _tile_bits_x;
	_tile_size_y = 1 << _tile_bits_y;
	_tiles_x = (_size_x + _tile_size_x - 1) >> _tile_bits_x;
	_tiles_y = (_size_y + _tile_size_y - 1) >> _tile_bits_y;

	const int tiles_size = _tiles_x * _tiles_y;
	if (!_sf_tiles || tiles_size > _tiles_alloc) {
		if (_sf_tiles) {
			delete []_sf_tiles;
		}
		_sf_tiles = new u8[tiles_size];
		_tiles_alloc = tiles_size;
	}
	if (!_cf_tiles || tiles_size > _tiles_alloc) {
		if (_cf_tiles) {
			delete []_cf_tiles;
		}
		_cf_tiles = new u8[tiles_size];
		_tiles_alloc = tiles_size;
	}

	if (!_seen_filter || _tiles_x > _seen_filter_alloc) {
		if (!_seen_filter) {
			delete []_seen_filter;
		}
		_seen_filter = new u8[_tiles_x];
		_seen_filter_alloc = _tiles_x;
	}

	// Process
	maskFilters();
	designFilters();
	designTiles();
	compressFilters();
	initializeEncoders();

	return GCIF_WE_OK;
}

bool ImageRGBAWriter::writeFilters(ImageWriter &writer) {
	const int rep_count = static_cast<int>( _filter_replacements.size() );

	CAT_DEBUG_ENFORCE(SF_COUNT < 32);
	CAT_DEBUG_ENFORCE(SpatialFilterSet::TAPPED_COUNT < 128);

	writer.writeBits(rep_count, 5);
	int bits = 5;

	for (int ii = 0; ii < rep_count; ++ii) {
		u32 filter = _filter_replacements[ii];

		u16 def = (u16)(filter >> 16);
		u16 cust = (u16)filter;

		writer.writeBits(def, 5);
		writer.writeBits(cust, 7);
		bits += 12;
	}

	// Write out filter huffman tables
	int cf_table_bits = _cf_encoder.writeTable(writer);
	int sf_table_bits = _sf_encoder.writeTable(writer);

#ifdef CAT_COLLECT_STATS
	Stats.filter_table_bits[0] = sf_table_bits + bits;
	Stats.filter_table_bits[1] = cf_table_bits;
#endif // CAT_COLLECT_STATS

	return true;
}

bool ImageRGBAWriter::writeChaos(ImageWriter &writer) {
#ifdef CAT_COLLECT_STATS
	int overhead_bits = 0;
	int bitcount[COLOR_PLANES] = {0};
	int filter_table_bits[2] = {0};
#endif

	CAT_DEBUG_ENFORCE(_chaos_levels <= 8);

	writer.writeBits(_chaos_levels - 1, 3);

	int bits = 3;

	for (int jj = 0; jj < _chaos_levels; ++jj) {
		bits += _y_encoder[jj].writeTables(writer);
		bits += _u_encoder[jj].writeTables(writer);
		bits += _v_encoder[jj].writeTables(writer);
		bits += _a_encoder[jj].writeTables(writer);
	}
#ifdef CAT_COLLECT_STATS
	overhead_bits += bits;
#endif

	const int width = _width;

	// For each scanline,
	const u8 *p = _rgba;
	u8 *lastStart = _chaos + COLOR_PLANES;
	CAT_CLR(_chaos, _chaos_size);

	const u8 *CHAOS_TABLE = _chaos_table;
	u8 FPT[3];

	for (int y = 0; y < _height; ++y) {
		u8 *last = lastStart;

		// Zero left
		last[0 - 4] = 0;
		last[1 - 4] = 0;
		last[2 - 4] = 0;
		last[3 - 4] = 0;

		// If it is time to clear the seen filters,
		if ((y & FILTER_ZONE_SIZE_MASK_H) == 0) {
			CAT_CLR(_seen_filter, _filter_stride);
		}

		// For each pixel,
		for (int x = 0; x < width; ++x) {
			DESYNC(x, y);

			// If not masked out,
			if (!_lz->visited(x, y) && !_mask->masked(x, y)) {
				// Get filter for this pixel
				u16 filter = getSpatialFilter(x, y);
				CAT_DEBUG_ENFORCE(filter != UNUSED_FILTER);
				u8 cf = (u8)filter;
				u8 sf = (u8)(filter >> 8);

				// If it is time to write out the filter information,
				if (!_seen_filter[x >> FILTER_ZONE_SIZE_SHIFT_W]) {
					_seen_filter[x >> FILTER_ZONE_SIZE_SHIFT_W] = true;

					int cf_bits = _cf_encoder.writeSymbol(cf, writer);
					DESYNC_FILTER(x, y);
					int sf_bits = _sf_encoder.writeSymbol(sf, writer);
					DESYNC_FILTER(x, y);

#ifdef CAT_COLLECT_STATS
					filter_table_bits[0] += sf_bits;
					filter_table_bits[1] += cf_bits;
#endif
				}

				// Apply spatial filter
				const u8 *pred = FPT;
				_sf_set.get(sf).safe(p, &pred, x, y, width);
				u8 temp[3];
				for (int jj = 0; jj < 3; ++jj) {
					temp[jj] = p[jj] - pred[jj];
				}

				// Apply color filter
				u8 YUVA[COLOR_PLANES];
				RGB2YUV_FILTERS[cf](temp, YUVA);
				if (x > 0) {
					YUVA[3] = p[-1] - p[3];
				} else {
					YUVA[3] = 255 - p[3];
				}

				u8 chaos = CHAOS_TABLE[CHAOS_SCORE[last[0 - 4]] + CHAOS_SCORE[last[0]]];

				int bits = _y_encoder[chaos].write(YUVA[0], writer);
				DESYNC(x, y);
#ifdef CAT_COLLECT_STATS
				bitcount[0] += bits;
#endif
				chaos = CHAOS_TABLE[CHAOS_SCORE[last[1 - 4]] + CHAOS_SCORE[last[1]]];
				bits = _u_encoder[chaos].write(YUVA[1], writer);
				DESYNC(x, y);
#ifdef CAT_COLLECT_STATS
				bitcount[1] += bits;
#endif
				chaos = CHAOS_TABLE[CHAOS_SCORE[last[2 - 4]] + CHAOS_SCORE[last[2]]];
				bits = _v_encoder[chaos].write(YUVA[2], writer);
				DESYNC(x, y);
#ifdef CAT_COLLECT_STATS
				bitcount[2] += bits;
#endif
				chaos = CHAOS_TABLE[CHAOS_SCORE[last[3 - 4]] + CHAOS_SCORE[last[3]]];
				bits = _a_encoder[chaos].write(YUVA[3], writer);
				DESYNC(x, y);
#ifdef CAT_COLLECT_STATS
				bitcount[3] += bits;
#endif

				for (int c = 0; c < COLOR_PLANES; ++c) {
					last[c] = YUVA[c];
				}
			} else {
				for (int c = 0; c < COLOR_PLANES; ++c) {
					last[c] = 0;
				}
			}

			// Next pixel
			last += COLOR_PLANES;
			p += 4;
		}
	}

#ifdef CAT_COLLECT_STATS
	for (int ii = 0; ii < COLOR_PLANES; ++ii) {
		Stats.rgb_bits[ii] = bitcount[ii];
	}
	Stats.chaos_overhead_bits = overhead_bits;
	Stats.filter_compressed_bits[0] = filter_table_bits[0];
	Stats.filter_compressed_bits[1] = filter_table_bits[1];
#endif

	return true;
}

void ImageRGBAWriter::write(ImageWriter &writer) {
	CAT_INANE("CM") << "Writing encoded pixel data...";

	writeFilters(writer);
	writeChaos(writer);

#ifdef CAT_COLLECT_STATS
	int total = 0;
	for (int ii = 0; ii < 2; ++ii) {
		total += Stats.filter_table_bits[ii];
		total += Stats.filter_compressed_bits[ii];
	}
	for (int ii = 0; ii < COLOR_PLANES; ++ii) {
		total += Stats.rgb_bits[ii];
	}
	total += Stats.chaos_overhead_bits;
	Stats.chaos_bits = total;
	total += _lz->Stats.huff_bits;
	total += _mask->Stats.compressedDataBits;
	Stats.total_bits = total;

	Stats.overall_compression_ratio = _width * _height * 4 * 8 / (double)Stats.total_bits;

	Stats.chaos_compression_ratio = Stats.chaos_count * COLOR_PLANES * 8 / (double)Stats.chaos_bits;
#endif
}

#ifdef CAT_COLLECT_STATS

bool ImageRGBAWriter::dumpStats() {
	CAT_INANE("stats") << "(RGBA Compress) Spatial Filter Table Size : " <<  Stats.filter_table_bits[0] << " bits (" << Stats.filter_table_bits[0]/8 << " bytes)";
	CAT_INANE("stats") << "(RGBA Compress) Spatial Filter Compressed Size : " <<  Stats.filter_compressed_bits[0] << " bits (" << Stats.filter_compressed_bits[0]/8 << " bytes)";

	CAT_INANE("stats") << "(RGBA Compress) Color Filter Table Size : " <<  Stats.filter_table_bits[1] << " bits (" << Stats.filter_table_bits[1]/8 << " bytes)";
	CAT_INANE("stats") << "(RGBA Compress) Color Filter Compressed Size : " <<  Stats.filter_compressed_bits[1] << " bits (" << Stats.filter_compressed_bits[1]/8 << " bytes)";

	CAT_INANE("stats") << "(RGBA Compress) Y-Channel Compressed Size : " <<  Stats.rgb_bits[0] << " bits (" << Stats.rgb_bits[0]/8 << " bytes)";
	CAT_INANE("stats") << "(RGBA Compress) U-Channel Compressed Size : " <<  Stats.rgb_bits[1] << " bits (" << Stats.rgb_bits[1]/8 << " bytes)";
	CAT_INANE("stats") << "(RGBA Compress) V-Channel Compressed Size : " <<  Stats.rgb_bits[2] << " bits (" << Stats.rgb_bits[2]/8 << " bytes)";
	CAT_INANE("stats") << "(RGBA Compress) A-Channel Compressed Size : " <<  Stats.rgb_bits[3] << " bits (" << Stats.rgb_bits[3]/8 << " bytes)";

	CAT_INANE("stats") << "(RGBA Compress) YUVA Overhead Size : " << Stats.chaos_overhead_bits << " bits (" << Stats.chaos_overhead_bits/8 << " bytes)";
	CAT_INANE("stats") << "(RGBA Compress) Chaos pixel count : " << Stats.chaos_count << " pixels";
	CAT_INANE("stats") << "(RGBA Compress) Chaos compression ratio : " << Stats.chaos_compression_ratio << ":1";
	CAT_INANE("stats") << "(RGBA Compress) Overall size : " << Stats.total_bits << " bits (" << Stats.total_bits/8 << " bytes)";
	CAT_INANE("stats") << "(RGBA Compress) Overall compression ratio : " << Stats.overall_compression_ratio << ":1";
	CAT_INANE("stats") << "(RGBA Compress) Image dimensions were : " << _width << " x " << _height << " pixels";

	return true;
}

#endif

