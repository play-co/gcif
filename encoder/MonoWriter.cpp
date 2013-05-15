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
#include "../decoder/BitMath.hpp"
using namespace cat;


#ifdef CAT_DESYNCH_CHECKS
#define DESYNC_TABLE() writer.writeWord(1234567);
#define DESYNC(x, y) writer.writeBits(x ^ 12345, 16); writer.writeBits(y ^ 54321, 16);
#else
#define DESYNC_TABLE()
#define DESYNC(x, y)
#endif


//// MonoWriterProfile

void MonoWriterProfile::cleanup() {
	if (filter_encoder) {
		delete filter_encoder;
		filter_encoder = 0;
	}
}

void MonoWriterProfile::init(u16 size_x, u16 size_y, u16 bits) {
	this->size_x = size_x;
	this->size_y = size_y;

	// Init with bits
	tile_bits_x = bits;
	tile_bits_y = bits;
	tile_size_x = 1 << bits;
	tile_size_y = 1 << bits;
	tiles_x = (size_x + tile_size_x - 1) >> bits;
	tiles_y = (size_y + tile_size_y - 1) >> bits;

	// Allocate tile memory
	tiles_count = tiles_x * tiles_y;
	tiles.resize(tiles_count);
	tiles.fill_00();
	tile_row_filters.resize(tiles_y);

	// Initialize mask
	mask.resize(tiles_count);
	mask.fill_00();

	// Allocate residual memory
	const u32 residuals_memory = size_x * size_y;
	residuals.resize(residuals_memory);

	filter_encoder = 0;
}

void MonoWriter::dumpStats() {
	if (_profile) {
		_profile->dumpStats();
	}
}

void MonoWriterProfile::dumpStats() {
	CAT_INANE("Mono") << "Designed monochrome writer using " << tiles_x << "x" << tiles_y << " tiles to express " << filter_count << " (" << sympal_filter_count << " palette) filters for " << size_x << "x" << size_y << " image";
	if (filter_encoder) {
		CAT_INANE("Mono") << " - Recursively using filter encoder:";
		filter_encoder->dumpStats();
	} else {
		CAT_INANE("Mono") << " - Using row filters";
	}
}



//// MonoWriter

void MonoWriter::cleanup() {
	if (_profile) {
		delete _profile;
		_profile = 0;
	}
}

void MonoWriter::maskTiles() {
	const u16 tile_size_x = _profile->tile_size_x, tile_size_y = _profile->tile_size_y;
	const u16 size_x = _params.size_x, size_y = _params.size_y;
	u8 *m = _profile->mask.get();

	// For each tile,
	for (u16 y = 0; y < size_y; y += tile_size_y) {
		for (u16 x = 0; x < size_x; x += tile_size_x, ++m) {

			// For each element in the tile,
			u16 py = y, cy = tile_size_y;
			while (cy-- > 0 && py < size_y) {
				u16 px = x, cx = tile_size_x;
				while (cx-- > 0 && px < size_x) {
					// If it is not masked,
					if (!_params.mask(px, py)) {
						// We need to do this tile
						goto next_tile;
					}
					++px;
				}
				++py;
			}

			// Tile is masked out entirely
			*m = 1;
next_tile:;
		}
	}
}

void MonoWriter::designPaletteFilters() {
	const u16 tile_size_x = _profile->tile_size_x, tile_size_y = _profile->tile_size_y;
	const u16 size_x = _params.size_x, size_y = _params.size_y;
	const u16 tile_bits_x = _profile->tile_bits_x, tile_bits_y = _profile->tile_bits_y;
	u8 *p = _profile->tiles.get();

	u32 hist[MAX_SYMS] = { 0 };

	// For each tile,
	const u8 *topleft_row = _params.data;
	for (u16 y = 0; y < size_y; y += tile_size_y) {
		const u8 *topleft = topleft_row;

		for (u16 x = 0; x < size_x; x += tile_size_x, ++p, topleft += tile_size_x) {
			// If tile is masked,
			if (IsMasked(x >> tile_bits_x, y >> tile_bits_y)) {
				continue;
			}

			bool uniform = true;
			bool seen = false;
			u8 uniform_value = 0;

			// For each element in the tile,
			const u8 *row = topleft;
			u16 py = y, cy = tile_size_y;
			while (cy-- > 0 && py < size_y) {
				const u8 *data = row;
				u16 px = x, cx = tile_size_x;
				while (cx-- > 0 && px < size_x) {
					// If element is not masked,
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
					++px;
					++data;
				}
				++py;
				row += size_x;
			}

			// If uniform data,
			if (uniform) {
				hist[uniform_value]++;
			}
		}

		topleft_row += _params.size_x * tile_size_y;
	}

	// Determine threshold
	u32 sympal_thresh = _params.sympal_thresh * _profile->tiles_count;
	int sympal_count = 0;

	// For each histogram bin,
	for (int sym = 0, num_syms = _params.num_syms; sym < num_syms; ++sym) {
		u32 coverage = hist[sym];

		// If filter is worth adding,
		if (coverage >= sympal_thresh) {
			// Add it
			_profile->sympal[sympal_count++] = (u8)sym;

			//CAT_INANE("Mono") << " - Added symbol palette filter for symbol " << (int)sym;

			// If we ran out of room,
			if (sympal_count >= MAX_PALETTE) {
				break;
			}
		}
	}

	// Initialize filter map
	for (int ii = 0; ii < sympal_count; ++ii) {
		_sympal_filter_map[ii] = UNUSED_SYMPAL;
	}

	_profile->sympal_filter_count = sympal_count;
}

void MonoWriter::designFilters() {
	const u16 tile_size_x = _profile->tile_size_x, tile_size_y = _profile->tile_size_y;
	const u16 size_x = _params.size_x, size_y = _params.size_y;
	const u16 tile_bits_x = _profile->tile_bits_x, tile_bits_y = _profile->tile_bits_y;
	const u16 num_syms = _params.num_syms;
	u8 *p = _profile->tiles.get();

	FilterScorer scores, awards;
	scores.init(SF_COUNT + _profile->sympal_filter_count);
	awards.init(SF_COUNT + _profile->sympal_filter_count);
	awards.reset();

	u32 total_score = 0;

	// For each tile,
	const u8 *topleft_row = _params.data;
	for (u16 y = 0; y < size_y; y += tile_size_y) {
		const u8 *topleft = topleft_row;

		for (u16 x = 0; x < size_x; x += tile_size_x, ++p, topleft += tile_size_x) {
			// If tile is masked,
			if (IsMasked(x >> tile_bits_x, y >> tile_bits_y)) {
				continue;
			}

			scores.reset();

			bool uniform = true;
			bool seen = false;
			u8 uniform_value = 0;

			// For each element in the tile,
			const u8 *row = topleft;
			u16 py = y, cy = tile_size_y;
			while (cy-- > 0 && py < size_y) {
				const u8 *data = row;
				u16 px = x, cx = tile_size_x;
				while (cx-- > 0 && px < size_x) {
					// If element is not masked,
					if (!_params.mask(px, py)) {
						const u8 value = *data;

						if (!seen) {
							uniform_value = value;
							seen = true;
						} else if (value != uniform_value) {
							uniform = false;
						}

						for (int f = 0; f < SF_COUNT; ++f) {
							u8 prediction = MONO_FILTERS[f].safe(data, num_syms - 1, px, py, size_x);
							int residual = value + num_syms - prediction;
							if (residual >= num_syms) {
								residual -= num_syms;
							}
							u8 score = MonoChaos::ResidualScore(residual, num_syms); // lower = better

							scores.add(f, score);
						}
					}
					++px;
					++data;
				}
				++py;
				row += size_x;
			}

			// If data is uniform,
			int offset = 0;
			if (uniform) {
				// Find the matching filter
				for (int f = 0, f_end = _profile->sympal_filter_count; f < f_end; ++f) {
					if (_profile->sympal[f] == uniform_value) {
						// Award it top points
						awards.add(SF_COUNT + f, _params.AWARDS[0]);
						offset = 1;

						// Mark it as a palette filter tile so we can find it faster later if this palette filter gets chosen
						*p = SF_COUNT + f;
						break;
					}
				}
			}

			// Sort top few filters for awards
			FilterScorer::Score *top = scores.getLow(_params.award_count, true);
			for (int ii = offset; ii < _params.award_count; ++ii) {
				int index = top[ii - offset].index;
				awards.add(index, _params.AWARDS[ii]);

				if (index < SF_COUNT) {
					total_score += _params.AWARDS[ii];
				}
			}
		}

		topleft_row += _params.size_x * tile_size_y;
	}

	// Copy the first SF_FIXED filters
	for (int f = 0; f < SF_FIXED; ++f) {
		_profile->filters[f] = MONO_FILTERS[f];
		_profile->filter_indices[f] = f;
	}

	// Decide how many filters to sort by score
	int count = _params.max_filters + SF_FIXED;
	if (count > SF_COUNT) {
		count = SF_COUNT;
	}

	// Prepare to reduce the sympal set size
	int sympal_f = 0;

	// Choose remaining filters until coverage is acceptable
	int normal_f = SF_FIXED; // Next normal filter index
	int filters_set = SF_FIXED; // Total filters
	FilterScorer::Score *top = awards.getHigh(count, true);

	u8 palette[MAX_PALETTE];

	int max_filters = _params.max_filters;
	const int filter_limit = _profile->tiles_x * _profile->tiles_y;
	if (max_filters > filter_limit) {
		max_filters = filter_limit;
	}

	// For each of the sorted filter scores,
	u32 coverage = 0;
	for (int ii = 0; ii < count; ++ii) {
		int index = top[ii].index;
		int score = top[ii].score;

		// If filter is not fixed,
		if (index >= SF_FIXED) {
			// If filter is a sympal,
			if (index >= SF_COUNT) {
				// Map it from sympal filter index to new filter index
				int sympal_filter = index - SF_COUNT;
				_sympal_filter_map[sympal_filter] = sympal_f;

				palette[sympal_f] = index;
				++sympal_f;

				//CAT_INANE("Mono") << " + Added palette filter " << sympal_f << " for palette index " << sympal_filter << " Score " << score;
			} else {
				_profile->filters[normal_f] = MONO_FILTERS[index];
				_profile->filter_indices[normal_f] = index;
				++normal_f;

				coverage += score;

				//CAT_INANE("Mono") << " - Added filter " << normal_f << " for filter index " << index << " Score " << score << " Coverage " << coverage;
			}

			++filters_set;
			if (filters_set >= max_filters) {
				break;
			}
		} else {
			//CAT_INANE("Mono") << " - Added fixed filter " << normal_f << " for filter index " << index << " Score " << score;
			coverage += score;
		}

		float coverage_ratio = coverage / (float)total_score;

		// If coverage is sufficient,
		if (coverage_ratio >= _params.filter_cover_thresh) {
			if (score / (float)total_score < _params.filter_inc_thresh) {
				break;
			}
		}
	}

	// Insert filter indices at the end
	for (int ii = 0; ii < sympal_f; ++ii) {
		_profile->filter_indices[ii + normal_f] = palette[ii];
	}

	// Record counts
	_profile->normal_filter_count = normal_f;
	_profile->sympal_filter_count = sympal_f;
	_profile->filter_count = filters_set;

	CAT_DEBUG_ENFORCE(_profile->filter_count == _profile->normal_filter_count + _profile->sympal_filter_count);

	//CAT_INANE("Mono") << " + Chose " << _profile->filter_count << " filters : " << _profile->sympal_filter_count << " of which are palettes";
}

void MonoWriter::designPaletteTiles() {
	if (_profile->sympal_filter_count < 0) {
		//CAT_INANE("Mono") << "No palette filters selected";
		return;
	}

	//CAT_INANE("Mono") << "Designing palette tiles for " << _profile->tiles_x << "x" << _profile->tiles_y << "...";

	const u16 tile_size_x = _profile->tile_size_x, tile_size_y = _profile->tile_size_y;
	const u16 size_x = _params.size_x, size_y = _params.size_y;
	const u16 tile_bits_x = _profile->tile_bits_x, tile_bits_y = _profile->tile_bits_y;
	u8 *p = _profile->tiles.get();

	// For each tile,
	const u8 *topleft_row = _params.data;
	for (u16 y = 0; y < size_y; y += tile_size_y) {
		const u8 *topleft = topleft_row;

		for (u16 x = 0; x < size_x; x += tile_size_x, ++p, topleft += tile_size_x) {
			// If tile is masked,
			if (IsMasked(x >> tile_bits_x, y >> tile_bits_y)) {
				continue;
			}

			const u8 value = *p;

			// If this tile was initially paletted,
			if (value >= SF_COUNT) {
				// Look up the new filter value
				u8 filter = _sympal_filter_map[value - SF_COUNT];

				// If it was used,
				if (filter != UNUSED_SYMPAL) {
					// Prefer it over any other filter type
					*p = _profile->normal_filter_count + filter;
				} else {
					// Unlock it for use
					*p = 0;
				}
			}
		}

		topleft_row += _params.size_x * tile_size_y;
	}
}

void MonoWriter::designTiles() {
	//CAT_INANE("Mono") << "Designing tiles for " << _profile->tiles_x << "x" << _profile->tiles_y << "...";

	const u16 tile_size_x = _profile->tile_size_x, tile_size_y = _profile->tile_size_y;
	const u16 size_x = _params.size_x, size_y = _params.size_y;
	const u16 num_syms = _params.num_syms;

	EntropyEstimator ee;
	ee.init();

	const u32 code_stride = tile_size_x * tile_size_y;
	const u32 codes_size = code_stride * _profile->filter_count;
	_ecodes.resize(codes_size);
	u8 *codes = _ecodes.get();

	// Until revisits are done,
	int passes = 0;
	int revisitCount = _params.knobs->mono_revisitCount;
	while (passes < MAX_PASSES) {
		// For each tile,
		u8 *p = _profile->tiles.get();
		const u8 *topleft_row = _params.data;
		int ty = 0;

		for (u16 y = 0; y < size_y; y += tile_size_y, ++ty) {
			const u8 *topleft = topleft_row;
			int tx = 0;

			for (u16 x = 0; x < size_x; x += tile_size_x, ++p, topleft += tile_size_x, ++tx) {
				// If tile is masked,
				if (IsMasked(tx, ty)) {
					continue;
				}

				const u8 old_filter = *p;

				// If tile is sympal,
				if (old_filter >= _profile->normal_filter_count) {
					continue;
				}

				// If we are on the second or later pass,
				if (passes > 0) {
					// If just finished revisiting old zones,
					if (--revisitCount < 0) {
						// Done!
						return;
					}

					int code_count = 0;

					// If old filter is not a sympal,
					if (_profile->filter_indices[old_filter] < SF_COUNT) {
						// For each element in the tile,
						const u8 *row = topleft;
						u16 py = y, cy = tile_size_y;
						while (cy-- > 0 && py < size_y) {
							const u8 *data = row;
							u16 px = x, cx = tile_size_x;
							while (cx-- > 0 && px < size_x) {
								// If element is not masked,
								if (!_params.mask(px, py)) {
									const u8 value = *data;

									u8 prediction = _profile->filters[old_filter].safe(data, num_syms - 1, px, py, size_x);
									int residual = value + num_syms - prediction;
									if (residual >= num_syms) {
										residual -= num_syms;
									}

									codes[code_count++] = residual;
								}
								++px;
								++data;
							}
							++py;
							row += size_x;
						}

						ee.subtract(codes, code_count);
					}
				}

				int code_count = 0;

				// For each element in the tile,
				const u8 *row = topleft;
				u16 py = y, cy = tile_size_y;
				while (cy-- > 0 && py < size_y) {
					const u8 *data = row;
					u16 px = x, cx = tile_size_x;
					while (cx-- > 0 && px < size_x) {
						// If element is not masked,
						if (!_params.mask(px, py)) {
							const u8 value = *data;

							u8 *dest = codes + code_count;
							for (int f = 0, f_end = _profile->filter_count; f < f_end; ++f) {
								if (_profile->filter_indices[f] < SF_COUNT) {
									u8 prediction = _profile->filters[f].safe(data, num_syms - 1, px, py, size_x);
									int residual = value + num_syms - prediction;
									if (residual >= num_syms) {
										residual -= num_syms;
									}

									*dest = residual;
								}

								dest += code_stride;
							}

							++code_count;
						}
						++px;
						++data;
					}
					++py;
					row += size_x;
				}

				// Read neighbor tiles
				const u8 INVALID_TILE = 255; // Filter index never goes this high
				int a = INVALID_TILE; // left
				int b = INVALID_TILE; // up
				int c = INVALID_TILE; // up-left
				int d = INVALID_TILE; // up-right

				const int tiles_x = _profile->tiles_x;

				if (ty > 0) {
					b = p[-tiles_x];

					if (tx > 0) {
						c = p[-tiles_x - 1];
					}

					if (tx < tiles_x-1) {
						d = p[-tiles_x + 1];
					}
				}

				if (tx > 0) {
					a = p[-1];
				}

				// Evaluate entropy of codes
				u8 *src = codes;
				u8 *best_src = src;
				int lowest_entropy = 0x7fffffff;
				int bestFilterIndex = 0;

				const int NEIGHBOR_REWARD = 1;
				for (int f = 0, f_end = _profile->filter_count; f < f_end; ++f) {
					if (_profile->filter_indices[f] < SF_COUNT) {
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
							best_src = src;
						}
					}

					src += code_stride;
				}

				ee.add(best_src, code_count);

				*p = (u8)bestFilterIndex;
			}

			topleft_row += _params.size_x * tile_size_y;
		}

		++passes;

		//CAT_INANE("Mono") << "Revisiting filter selections from the top... " << revisitCount << " left";
	}
}

void MonoWriter::computeResiduals() {
	//CAT_INANE("Mono") << "Executing tiles to generate residual matrix...";

	const u16 size_x = _params.size_x, size_y = _params.size_y;
	const u16 tile_bits_x = _profile->tile_bits_x, tile_bits_y = _profile->tile_bits_y;
	const u16 num_syms = _params.num_syms;

	const u8 *data = _params.data;
	u8 *residuals = _profile->residuals.get();
	const u16 *order = _pixel_write_order;
	u8 *replay;

	// If random-access input data,
	if (order) {
		// Initialize replay matrix
		_replay.resize(_params.size_x * _params.size_y);
		_replay.fill_00();

		replay = _replay.get();
	}

	// For each row of pixels,
	for (u16 y = 0; y < size_y; ++y) {
		u16 ty = y >> tile_bits_y;

		// If random-access write order,
		if (order) {
			u16 x;
			while ((x = *order++) != ORDER_SENTINEL) {
				u16 tx = x >> tile_bits_x;
				u8 f = _profile->getTile(tx, ty);

				// If type is sympal,
				if (f >= _profile->normal_filter_count) {
					continue;
				}

				// Read input data
				u8 value = data[x];

				// Run spatial filter to arrive at a prediction
				u8 prediction = _profile->filters[f].safe(&replay[x], num_syms - 1, x, y, size_x);
				int residual = value + num_syms - prediction;
				if (residual >= num_syms) {
					residual -= num_syms;
				}

				// Store residual
				replay[x] = value;
				residuals[x] = residual;
			}

			data += size_x;
			replay += size_x;
			residuals += size_x;
		} else {
			for (u16 x = 0; x < size_x; ++x, ++residuals, ++data) {
				// If element is not masked,
				if (!_params.mask(x, y)) {
					// Grab filter for this tile
					u16 tx = x >> tile_bits_x;
					u8 f = _profile->getTile(tx, ty);

					// If type is sympal,
					if (f >= _profile->normal_filter_count) {
						continue;
					}

					// Read input data
					u8 value = data[0];

					// Run spatial filter to arrive at a prediction
					u8 prediction = _profile->filters[f].safe(data, num_syms - 1, x, y, size_x);
					int residual = value + num_syms - prediction;
					if (residual >= num_syms) {
						residual -= num_syms;
					}

					// Store residual
					residuals[0] = residual;
				}
			}
		}
	}
}

void MonoWriter::optimizeTiles() {
	//CAT_INANE("Mono") << "Optimizing tiles for " << _profile->tiles_x << "x" << _profile->tiles_y << "...";

	_optimizer.process(_profile->tiles.get(), _profile->tiles_x, _profile->tiles_y, _profile->filter_count,
			PaletteOptimizer::MaskDelegate::FromMember<MonoWriter, &MonoWriter::IsMasked>(this));

	// Overwrite original tiles with optimized tiles
	const u8 *src = _optimizer.getOptimizedImage();
	memcpy(_profile->tiles.get(), src, _profile->tiles_count);

	// Update filter indices
	int filter_indices[MAX_FILTERS];
	for (int ii = 0, iiend = _profile->filter_count; ii < iiend; ++ii) {
		filter_indices[_optimizer.forward(ii)] = _profile->filter_indices[ii];
	}
	memcpy(_profile->filter_indices, filter_indices, sizeof(_profile->filter_indices));
}

void MonoWriter::generateWriteOrder(u16 size_x, u16 size_y, MaskDelegate mask, u16 tile_shift_bits, std::vector<u16> &order) {
	// Ensure that vector is clear
	order.clear();

	// Generate write order data for recursive operation
	const u16 tile_bits_x = tile_shift_bits;
	const u16 tile_mask_y = (1 << tile_bits_x) - 1;
	const u16 tiles_x = (size_x + tile_mask_y) >> tile_bits_x;

	SmartArray<u8> seen;
	seen.resize(tiles_x);

	// For each pixel row,
	for (u16 y = 0; y < size_y; ++y) {
		// If starting a tile row,
		if ((y & tile_mask_y) == 0) {
			seen.fill_00();

			// After the first tile row,
			if (y > 0) {
				order.push_back(ORDER_SENTINEL);
			}
		}

		// For each pixel,
		for (u16 x = 0; x < size_x; ++x) {
			// If pixel is not masked out,
			if (!mask(x, y)) {
				// If tile seen for the first time,
				u16 tx = x >> tile_bits_x;
				if (seen[tx] == 0) {
					seen[tx] = 1;

					order.push_back(tx);
				}
			}
		}
	}

	order.push_back(ORDER_SENTINEL);
}

void MonoWriter::generateWriteOrder() {
	// Initialize tile seen array
	_tile_seen.resize(_profile->tiles_x);

	// Clear tile write order
	_tile_write_order.clear();

	// Generate write order data for recursive operation
	const u16 tile_bits_x = _profile->tile_bits_x;
	const u16 tile_mask_y = _profile->tile_size_y - 1;
	const u16 *order = _pixel_write_order;

	// For each pixel row,
	for (u16 y = 0; y < _params.size_y; ++y) {
		// If starting a tile row,
		if ((y & tile_mask_y) == 0) {
			_tile_seen.fill_00();

			// After the first tile row,
			if (y > 0) {
				_tile_write_order.push_back(ORDER_SENTINEL);
			}
		}

		// If write order was specified by caller,
		if (order) {
			// For each x,
			u16 x;
			while ((x = *order++) != ORDER_SENTINEL) {
				CAT_DEBUG_ENFORCE(!_params.mask(x, y));

				// If tile seen for the first time,
				u16 tx = x >> tile_bits_x;
				if (_tile_seen[tx] == 0) {
					_tile_seen[tx] = 1;

					_tile_write_order.push_back(tx);
				}
			}
		} else {
			// For each pixel,
			for (u16 x = 0, size_x = _params.size_x; x < size_x; ++x) {
				// If pixel is not masked out,
				if (!_params.mask(x, y)) {
					// If tile seen for the first time,
					u16 tx = x >> tile_bits_x;
					if (_tile_seen[tx] == 0) {
						_tile_seen[tx] = 1;

						_tile_write_order.push_back(tx);
					}
				}
			}
		}
	}

	_tile_write_order.push_back(ORDER_SENTINEL);
}

void MonoWriter::designRowFilters() {
	//CAT_INANE("Mono") << "Designing row filters for " << _profile->tiles_x << "x" << _profile->tiles_y << "...";

	const int tiles_x = _profile->tiles_x, tiles_y = _profile->tiles_y;
	const u16 filter_count = _profile->filter_count;

	u32 total_entropy;
	EntropyEstimator ee;
	ee.init();

	// Allocate temporary workspace
	const u32 codes_size = MonoReader::RF_COUNT * tiles_x;
	_ecodes.resize(codes_size);
	u8 *codes = _ecodes.get();

	// For each pass through,
	int passes = 0;
	while (passes < MAX_ROW_PASSES) {
		total_entropy = 0;

		const u16 *order = &_tile_write_order[0];

		// For each tile,
		for (u16 ty = 0; ty < tiles_y; ++ty) {
			u8 prev = 0;
			int code_count = 0;

			u16 tx;
			while ((tx = *order++) != ORDER_SENTINEL) {
				u8 f = _profile->getTile(tx, ty);

				CAT_DEBUG_ENFORCE(f < filter_count);

				// RF_NOOP
				codes[code_count] = f;

				// RF_PREV
				u8 fprev = f + filter_count - prev;
				if (fprev >= filter_count) {
					fprev -= filter_count;
				}
				prev = f;

				CAT_DEBUG_ENFORCE(fprev < filter_count);

				codes[code_count + tiles_x] = fprev;

				++code_count;
			}

			// If on the second or later pass,
			if (passes > 0) {
				// Subtract out the previous winner
				if (_profile->tile_row_filters[ty] == MonoReader::RF_NOOP) {
					ee.subtract(codes, code_count);
				} else {
					ee.subtract(codes + tiles_x, code_count);
				}
			}

			// Calculate entropy for each of the row filter options
			u32 e0 = ee.entropy(codes, code_count);
			u32 e1 = ee.entropy(codes + tiles_x, code_count);

			// Pick the better one
			if (e0 <= e1) {
				_profile->tile_row_filters[ty] = MonoReader::RF_NOOP;
				total_entropy += 1 + e0; // + 1 bit per row for header
				ee.add(codes, code_count);
			} else {
				_profile->tile_row_filters[ty] = MonoReader::RF_PREV;
				total_entropy += 1 + e0; // + 1 bit per row for header
				ee.add(codes + tiles_x, code_count);
			}
		}

		++passes;
	}

	_profile->row_filter_entropy = total_entropy;
}

bool MonoWriter::IsMasked(u16 x, u16 y) {
	return _profile->mask[x + y * _profile->tiles_x] != 0;
}

void MonoWriter::recurseCompress() {
	const u16 tiles_x = _profile->tiles_x, tiles_y = _profile->tiles_y;

	if (_profile->tiles_count < RECURSE_THRESH_COUNT) {
		//CAT_INANE("Mono") << "Stopping below recursive threshold for " << tiles_x << "x" << tiles_y << "...";
	} else {
		//CAT_INANE("Mono") << "Recursively compressing tiles for " << tiles_x << "x" << tiles_y << "...";

		_profile->filter_encoder = new MonoWriter;

		Parameters params = _params;
		params.data = _profile->tiles.get();
		params.num_syms = _profile->filter_count;
		params.size_x = tiles_x;
		params.size_y = tiles_y;

		// Hook up our mask function
		params.mask.SetMember<MonoWriter, &MonoWriter::IsMasked>(this);

		// Recurse!
		u32 recurse_entropy = _profile->filter_encoder->process(params, &_tile_write_order[0]);

		// If it does not win over row filters,
		if (recurse_entropy > _profile->row_filter_entropy) {
			//CAT_INANE("Mono") << "Recursive filter did not win over simple row filters: " << recurse_entropy << " > " << _profile->row_filter_entropy;

			delete _profile->filter_encoder;
			_profile->filter_encoder = 0;
		} else {
			//CAT_INANE("Mono") << "Recursive filter won over simple row filters: " << recurse_entropy << " <= " << _profile->row_filter_entropy;
		}
	}
}

void MonoWriter::designChaos() {
	//CAT_INANE("Mono") << "Designing chaos...";

	EntropyEstimator ee[MAX_CHAOS_LEVELS];

	u32 best_entropy = 0x7fffffff;
	int best_chaos_levels = 1;

	// For each chaos level,
	for (int chaos_levels = 1; chaos_levels < MAX_CHAOS_LEVELS; ++chaos_levels) {
		_profile->chaos.init(chaos_levels, _params.size_x);

		// Reset entropy estimator
		for (int ii = 0; ii < chaos_levels; ++ii) {
			ee[ii].init();
		}

		_profile->chaos.start();

		const u16 *order = _pixel_write_order;

		// For each row,
		const u8 *residuals = _profile->residuals.get();
		for (u16 y = 0; y < _params.size_y; ++y) {
			// If random write order,
			if (order) {
				u16 x;
				while ((x = *order++) != ORDER_SENTINEL) {
					const u8 f = _profile->getTile(x >> _profile->tile_bits_x, y >> _profile->tile_bits_y);

					CAT_DEBUG_ENFORCE(f < _profile->filter_count);

					// If masked or sympal,
					if (_profile->filter_indices[f] >= SF_COUNT) {
						_profile->chaos.zero(x);
					} else {
						// Get residual symbol
						u8 residual = residuals[x];

						// Get chaos bin
						int chaos = _profile->chaos.get(x);
						_profile->chaos.store(x, residual, _params.num_syms);

						// Add to histogram for this chaos bin
						ee[chaos].addSingle(residual);
					}
				}

				residuals += _params.size_x;
			} else {
				// For each column,
				for (u16 x = 0; x < _params.size_x; ++x, ++residuals) {
					const u8 f = _profile->getTile(x >> _profile->tile_bits_x, y >> _profile->tile_bits_y);

					CAT_DEBUG_ENFORCE(f < _profile->filter_count);

					// If masked or sympal,
					if (_params.mask(x, y) || _profile->filter_indices[f] >= SF_COUNT) {
						_profile->chaos.zero(x);
					} else {
						// Get residual symbol
						u8 residual = *residuals;

						// Get chaos bin
						int chaos = _profile->chaos.get(x);
						_profile->chaos.store(x, residual, _params.num_syms);

						// Add to histogram for this chaos bin
						ee[chaos].addSingle(residual);
					}
				}
			}
		}

		// For each chaos level,
		u32 entropy = 0;
		for (int ii = 0; ii < chaos_levels; ++ii) {
			entropy += ee[ii].entropyOverall();

			// Approximate cost of adding an entropy level
			entropy += 5 * _params.num_syms;
		}

		// If this is the best chaos levels so far,
		if (best_entropy > entropy) {
			best_entropy = entropy;
			best_chaos_levels = chaos_levels;
		}
	}

	best_chaos_levels = 8;

	// Record the best option found
	_profile->chaos.init(best_chaos_levels, _params.size_x);
	_chaos_entropy = best_entropy;
}

void MonoWriter::initializeEncoders() {
	_profile->chaos.start();

	const u16 *order = _pixel_write_order;

	// For each row,
	const u8 *residuals = _profile->residuals.get();
	for (int y = 0; y < _params.size_y; ++y) {
		// If random write order,
		if (order) {
			u16 x;
			while ((x = *order++) != ORDER_SENTINEL) {
				const u8 f = _profile->getTile(x >> _profile->tile_bits_x, y >> _profile->tile_bits_y);

				CAT_DEBUG_ENFORCE(f < _profile->filter_count);

				// If masked or sympal,
				if (_profile->filter_indices[f] >= SF_COUNT) {
					_profile->chaos.zero(x);
				} else {
					// Get residual symbol
					u8 residual = residuals[x];

					// Get chaos bin
					int chaos = _profile->chaos.get(x);
					_profile->chaos.store(x, residual, _params.num_syms);

					// Add to histogram for this chaos bin
					_profile->encoder[chaos].add(residual);
				}
			}

			residuals += _params.size_x;
		} else {
			// For each column,
			for (u16 x = 0; x < _params.size_x; ++x, ++residuals) {
				const u8 f = _profile->getTile(x >> _profile->tile_bits_x, y >> _profile->tile_bits_y);

				CAT_DEBUG_ENFORCE(f < _profile->filter_count);

				// If masked or sympal,
				if (_params.mask(x, y) || _profile->filter_indices[f] >= SF_COUNT) {
					_profile->chaos.zero(x);
				} else {
					// Get residual symbol
					u8 residual = *residuals;

					// Get chaos bin
					int chaos = _profile->chaos.get(x);
					_profile->chaos.store(x, residual, _params.num_syms);

					// Add to histogram for this chaos bin
					_profile->encoder[chaos].add(residual);
				}
			}
		}
	}

	// For each chaos level,
	for (int ii = 0, iiend = _profile->chaos.getBinCount(); ii < iiend; ++ii) {
		_profile->encoder[ii].finalize();
	}

	// If using row encoders for filters,
	if (!_profile->filter_encoder) {
		const u16 filter_count = _profile->filter_count;

		const u16 *order = &_tile_write_order[0];

		// For each tile row,
		for (int ty = 0, tiles_y = _profile->tiles_y; ty < tiles_y; ++ty) {
			int tile_row_filter = _profile->tile_row_filters[ty];
			u8 prev = 0;

			u16 tx;
			while ((tx = *order++) != ORDER_SENTINEL) {
				u8 f = _profile->getTile(tx, ty);
				u8 rf = f;

				// If filtering this row,
				if (tile_row_filter == MonoReader::RF_PREV) {
					rf += filter_count - prev;
					if (rf >= filter_count) {
						rf -= filter_count;
					}
					prev = f;
				}

				CAT_DEBUG_ENFORCE(rf < filter_count);

				_profile->row_filter_encoder.add(rf);
			}
		}

		// Finalize the row filter encoder
		_profile->row_filter_encoder.finalize();
	}
}

u32 MonoWriter::simulate() {
	u32 bits = 0;

	// Chaos overhead
	bits += 4 + _profile->chaos.getBinCount() * 5 * _params.num_syms;

	// Tile bits overhead
	bits += _tile_bits_field_bc;

	// Sympal choice overhead
	bits += 4 + 8 * _profile->sympal_filter_count;

	// Filter choice overhead
	bits += 5 + 7 * _profile->filter_count;

	// If not using row encoders for filters,
	++bits;
	if (_profile->filter_encoder) {
		bits += _profile->filter_encoder->simulate();
	} else {
		// Reset encoder for recursive simulation
		_profile->row_filter_encoder.reset();

		const u16 filter_count = _profile->filter_count;
		const u16 *order = &_tile_write_order[0];

		// For each filter row,
		for (int ty = 0, tiles_y = _profile->tiles_y; ty < tiles_y; ++ty) {
			int tile_row_filter = _profile->tile_row_filters[ty];
			u8 prev = 0;

			u16 tx;
			while ((tx = *order++) != ORDER_SENTINEL) {
				u8 f = _profile->getTile(tx, ty);
				u8 rf = f;

				// If filtering this row,
				if (tile_row_filter == MonoReader::RF_PREV) {
					rf += filter_count - prev;
					if (rf >= filter_count) {
						rf -= filter_count;
					}
					prev = f;
				}

				CAT_DEBUG_ENFORCE(rf < filter_count);

				bits += _profile->row_filter_encoder.simulate(rf);
			}
		}
	}

	// Simulate residuals
	_profile->chaos.start();

	// Reset encoders for recursive simulation
	for (int ii = 0, iiend = _profile->chaos.getBinCount(); ii < iiend; ++ii) {
		_profile->encoder[ii].reset();
	}

	const u8 *residuals = _profile->residuals.get();
	const u16 *order = _pixel_write_order;

	// For each row,
	for (u16 y = 0; y < _params.size_y; ++y) {
		// If random write order,
		if (order) {
			u16 x;
			while ((x = *order++) != ORDER_SENTINEL) {
				const u8 f = _profile->getTile(x >> _profile->tile_bits_x, y >> _profile->tile_bits_y);

				CAT_DEBUG_ENFORCE(f < _profile->filter_count);

				// If masked or sympal,
				if (_profile->filter_indices[f] >= SF_COUNT) {
					_profile->chaos.zero(x);
				} else {
					// Get residual symbol
					u8 residual = residuals[x];

					// Get chaos bin
					int chaos = _profile->chaos.get(x);
					_profile->chaos.store(x, residual, _params.num_syms);

					// Add to histogram for this chaos bin
					bits += _profile->encoder[chaos].simulate(residual);
				}
			}

			residuals += _params.size_x;
		} else {
			// For each column,
			for (u16 x = 0; x < _params.size_x; ++x, ++residuals) {
				const u8 f = _profile->getTile(x >> _profile->tile_bits_x, y >> _profile->tile_bits_y);

				CAT_DEBUG_ENFORCE(f < _profile->filter_count);

				// If masked or sympal,
				if (_params.mask(x, y) || _profile->filter_indices[f] >= SF_COUNT) {
					_profile->chaos.zero(x);
				} else {
					// Get residual symbol
					u8 residual = *residuals;

					// Get chaos bin
					int chaos = _profile->chaos.get(x);
					_profile->chaos.store(x, residual, _params.num_syms);

					// Add to histogram for this chaos bin
					bits += _profile->encoder[chaos].simulate(residual);
				}
			}
		}
	}

	return bits;
}

u32 MonoWriter::process(const Parameters &params, const u16 *write_order) {
	cleanup();

	// Initialize
	_params = params;
	_pixel_write_order = write_order;

	CAT_DEBUG_ENFORCE(params.num_syms > 0);

	// Calculate bits to represent tile bits field
	u32 range = _params.max_bits - _params.min_bits;
	int bits_bc = 0;
	if (range > 0) {
		bits_bc = BSR32(range) + 1;
	}
	_tile_bits_field_bc = bits_bc;

	//CAT_INANE("Mono") << "!! Monochrome filter processing started for " << _params.size_x << "x" << _params.size_y << " data matrix...";

	// Try to reuse the same profile object
	MonoWriterProfile *profile = new MonoWriterProfile;
	u32 best_entropy = 0x7fffffff;
	MonoWriterProfile *best_profile = 0;

	// Determine best tile size to use
	for (int bits = params.min_bits; bits <= params.max_bits; ++bits) {
		// Set up a profile
		profile->init(params.size_x, params.size_y, bits);
		_profile = profile;

		maskTiles();
		designPaletteFilters();
		designFilters();
		designPaletteTiles();
		designTiles();
		computeResiduals();
		optimizeTiles();
		generateWriteOrder();
		designRowFilters();
		recurseCompress();
		designChaos();
		initializeEncoders();

		// Calculate bits required to represent the data with this tile size
		u32 entropy = simulate();

		// If this is the best profile found so far,
		if (best_entropy > entropy) {
			best_entropy = entropy;

			if (best_profile) {
				delete best_profile;
			}
			best_profile = profile;

			// If we need another profile to try,
			if (bits < params.max_bits) {
				profile = new MonoWriterProfile;
			} else {
				profile = 0;
			}
		} else {
			// Stop trying options
			break;
		}
	}
	_profile = best_profile;

	// If an unused profile was left over,
	if (profile != 0) {
		delete profile;
	}

	// Calculate bits to encode without any overhead
	_untouched_bits = BSR32(params.num_syms) + 1;
	u32 untouched_entropy = 8 + _params.size_x * _params.size_y * _untouched_bits;

	// If the best profile is better than doing nothing at all,
	if (best_entropy < untouched_entropy) {
		_untouched_bits = 0;
	}

	return best_entropy;
}

bool MonoWriter::init(const Parameters &params, const u16 *write_order) {
	process(params, write_order);

	return true;
}

int MonoWriter::writeTables(ImageWriter &writer) {
	// Initialize stats
	Stats.basic_overhead_bits = 0;
	Stats.encoder_overhead_bits = 0;
	Stats.filter_overhead_bits = 1;
	Stats.data_bits = 0;

	// If not using write profile,
	if (_untouched_bits) {
		writer.writeBit(0);

		return GCIF_RE_OK;
	}

	// Enable encoders
	writer.writeBit(1);

	// Write tile size
	{
		CAT_DEBUG_ENFORCE(_profile->tile_bits_x == _profile->tile_bits_y);	// Square regions only for now

		if (_tile_bits_field_bc > 0) {
			writer.writeBits(_profile->tile_bits_x - _params.min_bits, _tile_bits_field_bc);
			Stats.basic_overhead_bits += _tile_bits_field_bc;
		}
	}

	DESYNC_TABLE();

	// Sympal filters
	{
		CAT_DEBUG_ENFORCE(MAX_PALETTE <= 15);

		writer.writeBits(_profile->sympal_filter_count, 4);
		for (int f = 0, f_end = _profile->sympal_filter_count; f < f_end; ++f) {
			writer.writeBits(_profile->sympal[f], 8);
			Stats.basic_overhead_bits += 8;
		}
	}

	DESYNC_TABLE();

	// Normal filters
	{
		CAT_DEBUG_ENFORCE(MAX_FILTERS <= 32);
		CAT_DEBUG_ENFORCE(SF_COUNT + MAX_PALETTE <= 128);

		writer.writeBits(_profile->filter_count - SF_FIXED, 5);
		for (int f = SF_FIXED, f_end = _profile->filter_count; f < f_end; ++f) {
			writer.writeBits(_profile->filter_indices[f], 7);
			Stats.basic_overhead_bits += 7;
		}
	}

	DESYNC_TABLE();

	// Write chaos levels
	{
		CAT_DEBUG_ENFORCE(MAX_CHAOS_LEVELS <= 16);

		writer.writeBits(_profile->chaos.getBinCount() - 1, 4);
		Stats.basic_overhead_bits += 4;
	}

	DESYNC_TABLE();

	// Write encoder tables
	{
		for (int ii = 0, iiend = _profile->chaos.getBinCount(); ii < iiend; ++ii) {
			Stats.encoder_overhead_bits += _profile->encoder[ii].writeTables(writer);
		}
	}

	DESYNC_TABLE();

	// Bit : row filters or recurse write tables
	{
		// If we decided to recurse,
		if (_profile->filter_encoder) {
			writer.writeBit(1);

			// Recurse write tables
			Stats.filter_overhead_bits += _profile->filter_encoder->writeTables(writer);
		} else {
			writer.writeBit(0);

			// Will write row filters at this depth
			Stats.filter_overhead_bits += _profile->row_filter_encoder.writeTables(writer);
		}
	}

	DESYNC_TABLE();

	initializeWriter();

	return Stats.encoder_overhead_bits + Stats.basic_overhead_bits + Stats.filter_overhead_bits;
}

void MonoWriter::initializeWriter() {
	// Note: Not called if encoders are disabled

	_profile->chaos.start();

	if (!_profile->filter_encoder) {
		_profile->row_filter_encoder.reset();
	}

	for (int ii = 0, iiend = _profile->chaos.getBinCount(); ii < iiend; ++ii) {
		_profile->encoder[ii].reset();
	}

#ifdef CAT_DEBUG
	generateWriteOrder();
	_next_write_tile_order = &_tile_write_order[0];
#endif
}

int MonoWriter::writeRowHeader(u16 y, ImageWriter &writer) {
	// If skipping encoder,
	if (_untouched_bits > 0) {
		return 0;
	}

	CAT_DEBUG_ENFORCE(y < _params.size_y);

	int bits = 0;

	// If at the start of a tile row,
	const u16 tile_mask_y = _profile->tile_size_y - 1;
	if ((y & tile_mask_y) == 0) {
		// Calculate tile y-coordinate
		u16 ty = y >> _profile->tile_bits_y;

		// If filter encoder is used instead of row filters,
		if (_profile->filter_encoder) {
			// After the first row,
			if (y > 0) {
				// For each pixel in seen row,
				for (u16 tx = 0; tx < _profile->tiles_x; ++tx) {
					if (!_tile_seen[tx]) {
						_profile->filter_encoder->zero(tx, ty);
					}
				}
			}

			// Recurse start row
			bits += _profile->filter_encoder->writeRowHeader(ty, writer);
		} else {
			CAT_DEBUG_ENFORCE(MonoReader::RF_COUNT <= 2);
			CAT_DEBUG_ENFORCE(_profile->tile_row_filters[ty] < 2);

			// Write out chosen row filter
			writer.writeBit(_profile->tile_row_filters[ty]);
			bits++;

			// Clear prev filter
			_prev_filter = 0;
		}

		// Clear tile seen
		_tile_seen.fill_00();

#ifdef CAT_DEBUG
		// After the first row,
		if (y > 0) {
			// Skip sentinel at row ends
			++_next_write_tile_order;
		}
#endif
	}

#ifdef CAT_DEBUG
	if (_pixel_write_order) {
		if (y > 0) {
			_pixel_write_order++;
		}
	}
#endif

	DESYNC(0, y);

	Stats.filter_overhead_bits += bits;
	return bits;
}

void MonoWriter::zero(u16 x, u16 y) {
	CAT_DEBUG_ENFORCE(_params.mask(x, y));

	_profile->chaos.zero(x);

	DESYNC(x, y);
}

int MonoWriter::write(u16 x, u16 y, ImageWriter &writer) {
	// If skipping encoder,
	if (_untouched_bits > 0) {
		writer.writeBits(_params.data[x + y * _params.size_x], _untouched_bits);
		return _untouched_bits;
	}

	int overhead_bits = 0, data_bits = 0;

	CAT_DEBUG_ENFORCE(x < _params.size_x && y < _params.size_y);
	CAT_DEBUG_ENFORCE(!_params.mask(x, y));
	CAT_DEBUG_ENFORCE(!_pixel_write_order || *_pixel_write_order++ == x);

	// Calculate tile coordinates
	u16 tx = x >> _profile->tile_bits_x;

	// Get tile
	u16 ty = y >> _profile->tile_bits_y;
	u8 *tile = _profile->tiles.get() + tx + ty * _profile->tiles_x;
	u8 f = tile[0];

	CAT_DEBUG_ENFORCE(!IsMasked(tx, ty));

	// If tile not seen yet,
	if (_tile_seen[tx] == 0) {
		_tile_seen[tx] = 1;

		CAT_DEBUG_ENFORCE(*_next_write_tile_order++ == tx);

		// If recursive filter encoded,
		if (_profile->filter_encoder) {
			// Pass filter write down the tree
			overhead_bits += _profile->filter_encoder->write(tx, ty, writer);
		} else {
			// Calculate row filter residual for filter data (filter of filters at tree leaf)
			u8 rf = f;

			if (_profile->tile_row_filters[ty] == MonoReader::RF_PREV) {
				const u16 filter_count = _profile->filter_count;
				rf += filter_count - _prev_filter;
				if (rf >= filter_count) {
					rf -= filter_count;
				}
				_prev_filter = f;
			}

			overhead_bits += _profile->row_filter_encoder.write(rf, writer);
		}

		Stats.filter_overhead_bits += overhead_bits;

		DESYNC(x, y);
	}

	// If using sympal,
	if (_profile->filter_indices[f] >= SF_COUNT) {
		_profile->chaos.zero(x);
	} else {
		// Look up residual sym
		u8 residual = _profile->residuals[x + y * _params.size_x];

		// Calculate local chaos
		int chaos = _profile->chaos.get(x);
		_profile->chaos.store(x, residual, _params.num_syms);

		// Write the residual value
		data_bits += _profile->encoder[chaos].write(residual, writer);

		Stats.data_bits += data_bits;
	}

	DESYNC(x, y);

	return overhead_bits + data_bits;
}

