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

#include "MonoWriter.hpp"
#include "../decoder/Enforcer.hpp"
#include "FilterScorer.hpp"
#include "EntropyEstimator.hpp"
using namespace cat;


//// MonoWriter

void MonoWriter::cleanup() {
	if (_tiles) {
		delete []_tiles;
		_tiles = 0;
	}
	if (_tile_row_filters) {
		delete []_tile_row_filters;
		_tile_row_filters = 0;
	}
	if (_filter_encoder) {
		delete _filter_encoder;
		_filter_encoder = 0;
	}
}

void MonoWriter::maskTiles() {
	const u16 tile_size_x = _tile_size_x, tile_size_y = _tile_size_y;
	const u16 size_x = _params.size_x, size_y = _params.size_y;
	u8 *p = _tiles;

	// For each tile,
	for (u16 y = 0; y < size_y; y += tile_size_y) {
		for (u16 x = 0; x < size_x; x += tile_size_x) {

			// For each pixel in the tile,
			u16 py = y, cy = tile_size_y;
			while (cy-- > 0 && py < size_y) {
				u16 px = x, cx = tile_size_x;
				while (cx-- > 0 && px < size_x) {
					// If it is not masked,
					if (!_params.mask(px, py)) {
						// We need to do this tile
						*p++ = TODO_TILE;
						goto next_tile;
					}
				}
				++py;
			}

			// Tile is masked out entirely
			*p++ = MASK_TILE;
next_tile:;
		}
	}
}

void MonoWriter::designPaletteFilters() {
	CAT_INANE("2D") << "Designing palette filters for " << _tiles_x << "x" << _tiles_y << "...";

	_sympal_filters = 0;

	const u16 tile_size_x = _tile_size_x, tile_size_y = _tile_size_y;
	const u16 size_x = _params.size_x, size_y = _params.size_y;
	u8 *p = _tiles;
	const u8 *topleft = _params.data;

	u32 hist[MAX_SYMS] = { 0 };

	// For each tile,
	for (u16 y = 0; y < size_y; y += tile_size_y) {
		for (u16 x = 0; x < size_x; x += tile_size_x, ++p, topleft += tile_size_x) {
			// If tile is masked,
			if (*p == MASK_TILE) {
				continue;
			}

			bool uniform = true;
			bool seen = false;
			u8 uniform_value = 0;

			// For each pixel in the tile,
			const u8 *scanline = topleft;
			u16 py = y, cy = tile_size_y;
			while (cy-- > 0 && py < size_y) {
				const u8 *data = scanline;
				u16 px = x, cx = tile_size_x;
				while (cx-- > 0 && px < size_x) {
					// If pixel is not masked,
					if (!_params.mask(px, py)) {
						const u8 value = *data;

						if (!seen) {
							uniform_value = value;
							seen = true;
						} else if (value != uniform_value) {
							uniform = false;
							cy = 0;
							break;
						}
					}
					++data;
				}
				++py;
				scanline += size_x;
			}

			// If uniform data,
			if (uniform) {
				hist[data_value]++;
			}
		}
	}

	// Determine threshold
	u32 sympal_thresh = _params.sympal_thresh * _tiles_count;	

	// For each histogram bin,
	for (int sym = 0, num_syms = _params.num_syms; sym < num_syms; ++sym) {
		u32 coverage = hist[sym];

		// If filter is worth adding,
		if (coverage > sympal_thresh) {
			// Add it
			_sympal[_sympal_filters++] = (u8)sym;

			CAT_INANE("2D") << " - Added symbol palette filter for symbol " << (int)sym;

			// If we ran out of room,
			if (_sympal_filters >= MAX_PALETTE) {
				break;
			}
		}
	}

	// Initialize filter map
	for (int ii = 0; ii < _sympal_filters; ++ii) {
		_sympal_filter_map[ii] = UNUSED_SYMPAL;
	}
}

void MonoWriter::designFilters() {
	CAT_INANE("2D") << "Designing filters for " << _tiles_x << "x" << _tiles_y << "...";

	const u16 tile_size_x = _tile_size_x, tile_size_y = _tile_size_y;
	const u16 size_x = _params.size_x, size_y = _params.size_y;
	const u16 num_syms = _params.num_syms;
	u8 *p = _tiles;
	const u8 *topleft = _params.data;

	FilterScorer scores, awards;
	scores.init(MF_COUNT + _sympal_filters);
	awards.init(MF_COUNT + _sympal_filters);
	awards.reset();

	// For each tile,
	for (u16 y = 0; y < size_y; y += tile_size_y) {
		for (u16 x = 0; x < size_x; x += tile_size_x, ++p, topleft += tile_size_x) {
			// If tile is masked,
			if (*p == MASK_TILE) {
				continue;
			}

			scores.reset();

			bool uniform = true;
			bool seen = false;
			u8 uniform_value = 0;

			// For each pixel in the tile,
			const u8 *scanline = topleft;
			u16 py = y, cy = tile_size_y;
			while (cy-- > 0 && py < size_y) {
				const u8 *data = scanline;
				u16 px = x, cx = tile_size_x;
				while (cx-- > 0 && px < size_x) {
					// If pixel is not masked,
					if (!_params.mask(px, py)) {
						const u8 value = *data;

						if (!seen) {
							uniform_value = value;
							seen = true;
						} else if (value != uniform_value) {
							uniform = false;
						}

						for (int f = 0; f < MF_COUNT; ++f) {
							// TODO: Specialize for num_syms power-of-two
							u8 prediction = MONO_FILTERS[f](data, x, y, size_x) % num_syms;
							u8 residual = value - prediction;
							u8 score = RESIDUAL_SCORE[residual]; // lower = better

							scores.add(f, score);
						}
					}
					++data;
				}
				++py;
				scanline += size_x;
			}

			// If data is uniform,
			int offset = 0;
			if (uniform) {
				// Find the matching filter
				for (int f = 0; f < _sympal_filters; ++f) {
					if (_sympal[f] == uniform_value) {
						// Award it top points
						awards.add(MF_COUNT + f, _params.AWARDS[0]);
						offset = 1;

						// Mark it as a palette filter tile so we can find it faster later if this palette filter gets chosen
						*p = MF_COUNT + f;
						break;
					}
				}
			}

			// Sort top few filters for awards
			FilterScorer::Score *top = scores.getTop(_params.award_count, true);
			for (int ii = offset; ii < _params.award_count; ++ii) {
				awards.add(top[ii - offset].index, _params.AWARDS[ii]);
			}
		}
	}

	EntropyEstimator ee;
	ee.init();

	// Copy the first MF_FIXED filters
	for (int f = 0; f < MF_FIXED; ++f) {
		_filters[f] = MONO_FILTERS[f];
		_filter_indices[f] = f;
	}

	// Adding one bit adds cost to each tile
	int bit_cost = _tiles_count;

	// Decide how many filters to sort by score
	int count = _params.max_filters + MF_FIXED;
	if (count > MF_COUNT) {
		count = MF_COUNT;
	}

	// Calculate min coverage threshold
	int filter_thresh = _params.filter_thresh * _tiles_count;
	int coverage = 0;

	// Prepare to reduce the sympal set size
	int sympal_f = 0;

	// Choose remaining filters until coverage is acceptable
	int normal_f = MF_FIXED; // Next normal filter index
	int filters_set = MF_FIXED; // Total filters
	FilterScorer::Score *top = awards.getTop(count, true);

	// For each of the sorted filter scores,
	for (int ii = 0; ii < count; ++ii) {
		int index = top[ii].index;
		int score = top[ii].score;

		// Calculate approximate bytes covered
		int covered = score / _params.AWARDS[0];

		// NOTE: Interesting interaction with fixed filters that are not chosen
		coverage += covered;

		// If coverage is satisifed,
		if (coverage >= filter_thresh) {
			// We're done here
			break;
		}

		// If filter is not fixed,
		if (index >= MF_FIXED) {
			// If filter is a sympal,
			if (index >= MF_COUNT) {
				// Map it from sympal filter index to new filter index
				int sympal_filter = index - MF_COUNT;
				_sympal_filter_map[sympal_filter] = sympal_f;
				++sympal_f;
			} else {
				_filters[normal_f] = MONO_FILTERS[index];
				_filter_indices[normal_f] = index;
				++normal_f;
			}

			++filters_set;
			if (filters_set >= MAX_FILTERS) {
				break;
			}
		}
	}

	// Record counts
	_normal_filter_count = normal_f;
	_sympal_filter_count = sympal_f;
	_filter_count = filters_set;

	CAT_DEBUG_ENFORCE(_filter_count == _normal_filter_count + _sympal_filter_count);

	CAT_INANE("2D") << "Chose " << _filter_count << " filters : " << _sympal_filter_count << " of which are palettes";
}

void MonoWriter::designPaletteTiles() {
	if (_sympal_filter_count < 0) {
		CAT_INANE("2D") << "No palette filters selected";
		return;
	}

	CAT_INANE("2D") << "Designing palette tiles for " << _tiles_x << "x" << _tiles_y << "...";

	const u16 tile_size_x = _tile_size_x, tile_size_y = _tile_size_y;
	const u16 size_x = _params.size_x, size_y = _params.size_y;
	u8 *p = _tiles;
	const u8 *topleft = _params.data;

	// For each tile,
	for (u16 y = 0; y < size_y; y += tile_size_y) {
		for (u16 x = 0; x < size_x; x += tile_size_x, ++p, topleft += tile_size_x) {
			const u8 value = *p;

			// If tile is masked,
			if (value == MASK_TILE) {
				continue;
			}

			// If this tile was initially paletted,
			if (value >= MF_COUNT) {
				// Look up the new filter value
				u8 filter = _sympal_filter_map[value - MF_COUNT];

				// If it was used,
				if (filter != UNUSED_SYMPAL) {
					// Prefer it over any other filter type
					*p = MF_COUNT + filter;
				} else {
					// Unlock it for use
					*p = TODO_TILE;
				}
			}
		}
	}
}

void MonoWriter::designTiles() {
	CAT_INANE("2D") << "Designing tiles for " << _tiles_x << "x" << _tiles_y << "...";

	const u16 tile_size_x = _tile_size_x, tile_size_y = _tile_size_y;
	const u16 size_x = _params.size_x, size_y = _params.size_y;
	const u16 num_syms = _params.num_syms;
	u8 *p = _tiles;

	EntropyEstimator ee;
	ee.init();

	const u32 code_stride = _tile_size_x * _tile_size_y;
	u8 *codes = new u8[code_stride * _filter_count];

	// Until revisits are done,
	int passes = 0;
	int revisitCount = _knobs->mono_revisitCount;
	while (passes < MAX_PASSES) {
		// For each tile,
		const u8 *topleft = _params.data;
		int ty = 0;
		for (u16 y = 0; y < size_y; y += tile_size_y, ++ty) {
			int tx = 0;
			for (u16 x = 0; x < size_x; x += tile_size_x, ++p, topleft += tile_size_x, ++tx) {
				// If tile is masked or sympal,
				if (*p >= MF_COUNT) {
					continue;
				}

				// If we are on the second or later pass,
				if (passes > 0) {
					// If just finished revisiting old zones,
					if (--revisitCount < 0) {
						// Done!
						delete []codes;
						return;
					}

					int code_count = 0;
					const int old_filter = *p;

					// If old filter is not a sympal,
					if (_filter_indices[old_filter] < MF_COUNT) {
						// For each pixel in the tile,
						const u8 *scanline = topleft;
						u16 py = y, cy = tile_size_y;
						while (cy-- > 0 && py < size_y) {
							const u8 *data = scanline;
							u16 px = x, cx = tile_size_x;
							while (cx-- > 0 && px < size_x) {
								// If pixel is not masked,
								if (!_params.mask(px, py)) {
									const u8 value = *data;

									u8 prediction = _filters[old_filter](data, x, y, size_x) % num_syms;
									u8 residual = (value + num_syms - prediction) % num_syms;

									codes[code_count++] = residual;
								}
								++data;
							}
							++py;
							scanline += size_x;
						}

						ee.subtract(codes, code_count);
					}
				}

				int code_count = 0;

				// For each pixel in the tile,
				const u8 *scanline = topleft;
				u16 py = y, cy = tile_size_y;
				while (cy-- > 0 && py < size_y) {
					const u8 *data = scanline;
					u16 px = x, cx = tile_size_x;
					while (cx-- > 0 && px < size_x) {
						// If pixel is not masked,
						if (!_params.mask(px, py)) {
							const u8 value = *data;

							u8 *dest = codes + code_count;
							for (int f = 0; f < _filter_count; ++f) {
								// TODO: Specialize for num_syms power-of-two
								u8 prediction = _filters[f](data, x, y, size_x) % num_syms;
								u8 residual = (value + num_syms - prediction) % num_syms;

								*dest = residual;
								dest += code_stride;
							}

							++code_count;
						}
						++data;
					}
					++py;
					scanline += size_x;
				}

				// Read neighbor tiles
				int a = MASK_TILE; // left
				int b = MASK_TILE; // up
				int c = MASK_TILE; // up-left
				int d = MASK_TILE; // up-right

				if (ty > 0) {
					b = p[-_tiles_x];

					if (tx > 0) {
						c = p[-_tiles_x - 1];
					}

					if (tx < _tiles_x-1) {
						d = p[-_tiles_x + 1];
					}
				}

				if (tx > 0) {
					a = p[-1];
				}

				// Evaluate entropy of codes
				u8 *src = codes;
				int lowest_entropy = 0x7fffffff;
				int bestFilterIndex = 0;

				const int NEIGHBOR_REWARD = 1;
				for (int f = 0; f < _filter_count; ++f) {
					int entropy = ee.entropy(src, code_count);

					// Nudge scoring based on neighbors
					if (entropy == 0) {
						entropy -= NEIGHBOR_REWARD;
					}
					if (f == a) {
						entropy -= NEIGHBOR_REWARD;
					}
					if (f == b) {
						entropy -= NEIGHBOR_REWARD;
					}
					if (f == c) {
						entropy -= NEIGHBOR_REWARD;
					}
					if (f == d) {
						entropy -= NEIGHBOR_REWARD;
					}

					if (lowest_entropy > entropy) {
						lowest_entropy = entropy;
						bestFilterIndex = f;
					}

					src += code_stride;
				}

				*p = (u8)bestFilterIndex;
			}
		}

		CAT_INANE("2D") << "Revisiting filter selections from the top... " << revisitCount << " left";
	}

	delete []codes;
}

void MonoWriter::designRowFilters() {
	CAT_INANE("2D") << "Designing row filters for " << _tiles_x << "x" << _tiles_y << "...";

	const int tiles_x = _tiles_x, tiles_y = _tiles_y;
	const u16 num_filters = _filter_count;
	u8 *p = _tiles;

	EntropyEstimator ee;
	ee.init();

	u32 total_entropy;

	u8 *codes = new u8[RF_COUNT * tiles_x];

	// For each pass through,
	int passes = 0;
	while (passes < MAX_ROW_PASSES) {
		total_entropy = 0;

		// For each tile,
		for (int ty = 0; ty < tiles_y; ++ty) {
			for (int tx = 0; tx < tiles_x; ++tx) {
				u8 f = p[0];

				// If tile is not masked,
				if (f != MASK_TILE) {
					// Get neighbors
					u8 a = 0, b = 0, c = 0;
					if (tx > 0) {
						a = p[-1];
						if (ty > 0) {
							c = p[-tiles_x-1];
						}
					}
					if (ty > 0) {
						b = p[-tiles_x];
					}

					// RF_NOOP
					codes[tx] = f;

					// RF_A
					codes[tx + tiles_x] = (f + num_filters - a) % num_filters;

					// RF_B
					codes[tx + tiles_x*2] = (f + num_filters - b) % num_filters;

					// RF_C
					codes[tx + tiles_x*3] = (f + num_filters - c) % num_filters;
				}

				++p;
			}

			// If on the second or later pass,
			if (passes > 0) {
				// Subtract out the previous winner
				ee.subtract(codes + tiles_x * _tile_row_filters[ty], tiles_x);
			}

			// Calculate entropy for each of the row filter options
			u32 e0 = ee.entropy(codes, tiles_x);
			u32 e1 = ee.entropy(codes + tiles_x, tiles_x);
			u32 e2 = ee.entropy(codes + tiles_x*2, tiles_x);
			u32 e3 = ee.entropy(codes + tiles_x*3, tiles_x);

			// Find the best one
			u32 best_e = e0;
			u32 best_i = 0;
			if (best_e > e1) {
				best_e = e1;
				best_i = 1;
			}
			if (best_e > e2) {
				best_e = e2;
				best_i = 2;
			}
			if (best_e > e3) {
				best_e = e3;
				best_i = 3;
			}

			_tile_row_filters[ty] = best_i;
			total_entropy += best_e;

			// Add the best option into the running histogram
			ee.add(codes + tiles_x * best_i, tiles_x);
		}

		++passes;
	}

	_row_filter_entropy = total_entropy;

	delete []codes;
}

void MonoWriter::recurseCompress() {
	if (_tiles_count < RECURSIVE_THRESH) {
		CAT_INANE("2D") << "Stopping below recursive threshold for " << _tiles_x << "x" << _tiles_y << "...";
	} else {
		CAT_INANE("2D") << "Recursively compressing tiles for " << _tiles_x << "x" << _tiles_y << "...";

		_filter_encoder = new MonoWriter;

		Parameters params;
		params.knobs = _params.knobs;
		params.data = _tiles;
		params.num_syms = _filter_count;
		params.size_x = _tiles_x;
		params.size_y = _tiles_y;
	}
}

void MonoWriter::designChaos() {
	CAT_INANE("2D") << "Designing chaos...";

	EntropyEstimator ee[MAX_CHAOS_LEVELS];

	// For each chaos level,
	for (int chaos_levels = 1; chaos_levels < MAX_CHAOS_LEVELS; ++chaos_levels) {
		// Reset entropy estimator
		for (int ii = 0; ii < chaos_levels; ++ii) {
			ee[ii].init();
		}
	}
}

u32 MonoWriter::process(const Parameters &params) {
	cleanup();

	// Initialize
	_params = params;
	_filter_encoder = 0;

	// Determine best tile size to use
	u32 best_entropy = 0x7fffffff;
	u32 best_bits = params.max_bits;

	// For each bit count to try,
	for (int bits = params.min_bits; bits <= params.max_bits; ++bits) {
		CAT_INANE("2D") << "Trying bits = " << bits << "...";

		// Init with bits
		_tile_bits_x = bits;
		_tile_bits_y = bits;
		_tile_size_x = 1 << bits;
		_tile_size_y = 1 << bits;
		_tiles_x = (_params.size_x + _tile_size_x - 1) >> bits;
		_tiles_y = (_params.size_y + _tile_size_y - 1) >> bits;

		// Allocate tile memory
		_tiles_count = _tiles_x * _tiles_y;
		_tiles = new u8[_tiles_count];
		_tile_row_filters = new u8[_tiles_y];

		// Process
		maskTiles();
		designPaletteFilters();
		designFilters();
		designPaletteTiles();
		designTiles();
		designRowFilters();
		recurseCompress();
		designChaos();
	}
}

void MonoWriter::writeTables(ImageWriter &writer) {
	// Tile size bits
	// Filter count and choices after MF_FIXED
	// Bit : row filters or recurse write tables
}

void MonoWriter::write(u16 x, u16 y, ImageWriter &writer) {
}

