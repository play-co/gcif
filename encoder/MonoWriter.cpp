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

	if (encoders) {
		delete encoders;
		encoders = 0;
	}
}

void MonoWriterProfile::init(u16 xsize, u16 ysize, u16 bits) {
	// Init with bits
	tile_bits_x = bits;
	tile_bits_y = bits;
	tile_xsize = 1 << bits;
	tile_ysize = 1 << bits;
	tiles_x = (xsize + tile_xsize - 1) >> bits;
	tiles_y = (ysize + tile_ysize - 1) >> bits;

	// Allocate tile memory
	tiles_count = tiles_x * tiles_y;
	tiles.resizeZero(tiles_count);
	mask.resize(tiles_count);

	// Allocate residual memory
	const u32 residuals_memory = xsize * ysize;
	residuals.resize(residuals_memory);

	filter_encoder = 0;
}


//// MonoWriter

void MonoWriter::cleanup() {
	if (_profile) {
		delete _profile;
		_profile = 0;
	}
}

void MonoWriter::priceResiduals() {
	CAT_INANE("Mono") << "Assigning approximate bit costs to residuals...";

	_prices.resize(_params.xsize * _params.ysize);

	_profile->encoders->chaos.start();

	for (int ii = 0, iiend = _profile->encoders->chaos.getBinCount(); ii < iiend; ++ii) {
		_profile->encoders->encoder[ii].reset();
	}

	// For each pixel of residuals,
	const u8 *residuals = _profile->residuals.get();
	u8 *prices = _prices.get();
	for (u16 y = 0; y < _params.ysize; ++y) {
		for (u16 x = 0; x < _params.xsize; ++x, ++residuals, ++prices) {
			// Get tile
			const u16 tx = x >> _profile->tile_bits_x;
			const u16 ty = y >> _profile->tile_bits_y;
			const u8 f = _profile->getTile(tx, ty);

			// If using sympal,
			if (_profile->filter_indices[f] >= SF_COUNT || _params.mask(x, y)) {
				_profile->encoders->chaos.zero(x);
				prices[0] = 0;
			} else {
				// Look up residual sym
				const u8 residual = residuals[0];

				// Calculate local chaos
				int chaos = _profile->encoders->chaos.get(x);
				_profile->encoders->chaos.store(x, residual, _params.num_syms);

				// Record price in bits
				int bits = _profile->encoders->encoder[chaos].price(residual);
				CAT_DEBUG_ENFORCE(bits >= 0 && bits < 256);
				prices[0] = static_cast<u8>( bits );
			}
		}
	}
}

void MonoWriter::designLZ() {
	CAT_INANE("Mono") << "Finding LZ77 matches for " << _params.xsize << "x" << _params.ysize << "...";

	LZMatchFinder::Parameters lz_params;
	lz_params.costs = _prices.get();
	lz_params.xsize = _params.xsize;
	lz_params.ysize = _params.ysize;
	lz_params.num_syms = _params.num_syms;
	lz_params.prematch_chain_limit = 2070;
	lz_params.inmatch_chain_limit = 512;

	// Find LZ matches
	_lz.init(_params.data, lz_params);
}

void MonoWriter::designRowFilters() {
	//CAT_INANE("Mono") << "Designing row filters for " << _params.xsize << "x" << _params.ysize << "...";

	const int xsize = _params.xsize, ysize = _params.ysize;
	const u16 num_syms = _params.num_syms;

	EntropyEstimator ee;
	ee.init();

	// Allocate temporary workspace
	const u32 codes_size = MonoReader::RF_COUNT * xsize;
	_ecodes.resize(codes_size);
	u8 *codes = _ecodes.get();

	// For each pass through,
	int passes = 0;
	while (passes < MAX_ROW_PASSES) {
		const u16 *order = _params.write_order;
		const u8 *data = _params.data;

		_one_row_filter = true;
		u8 last = 0;

		// For each tile,
		for (u16 y = 0; y < ysize; ++y) {
			u8 prev = 0;
			int code_count = 0;

			if (order) {
				u16 x;
				while ((x = *order++) != ORDER_SENTINEL) {
					u8 p = data[x];

					// RF_NOOP
					codes[code_count] = p;

					// RF_PREV
					u16 pprev = p + num_syms - prev;
					if (pprev >= num_syms) {
						pprev -= num_syms;
					}
					prev = p;

					CAT_DEBUG_ENFORCE(pprev < num_syms);

					codes[code_count + xsize] = (u8)pprev;

					++code_count;
				}

				data += xsize;
			} else {
				for (u16 x = 0; x < xsize; ++x, ++data) {
					// If pixel is LZ-masked,
					if (_params.lz_enable && _lz.masked(x, y)) {
						u8 p = data[0];
						prev = p;
						continue;
					}

					u8 p = data[0];

					if (!_params.mask(x, y)) {
						// RF_NOOP
						codes[code_count] = p;

						// RF_PREV
						u16 pprev = p + num_syms - prev;
						if (pprev >= num_syms) {
							pprev -= num_syms;
						}
						prev = p;

						CAT_DEBUG_ENFORCE(pprev < num_syms);

						codes[code_count + xsize] = (u8)pprev;

						++code_count;
					}
				}
			}

			// If on the second or later pass,
			if (passes > 0) {
				// Subtract out the previous winner
				if (_row_filters[y] == MonoReader::RF_NOOP) {
					ee.subtract(codes, code_count);
				} else {
					ee.subtract(codes + xsize, code_count);
				}
			}

			// Calculate entropy for each of the row filter options
			u32 e0 = ee.entropy(codes, code_count);
			u32 e1 = ee.entropy(codes + xsize, code_count);

			// Pick the better one
			u8 filter;
			if (e0 <= e1) {
				filter = MonoReader::RF_NOOP;
				ee.add(codes, code_count);
			} else {
				filter = MonoReader::RF_PREV;
				ee.add(codes + xsize, code_count);
			}

			// Store it	
			_row_filters[y] = filter;
			if (y == 0) {
				last = filter;
			} else if (last != filter) {
				_one_row_filter = false;
			}
		}

		++passes;
	}

	// Initialize row encoder
	{
		const u16 *order = _params.write_order;
		const u8 *data = _params.data;

		_row_filter_encoder.init(_params.num_syms + (_params.lz_enable ? LZReader::ESCAPE_SYMS : 0), ZRLE_SYMS);

		// Setup LZ
		LZMatchFinder::LZMatch *lzm = _lz.getHead();
		int offset = 0;

		// For each tile,
		for (u16 y = 0; y < ysize; ++y) {
			u8 prev = 0;
			const u8 rf = _row_filters[y];

			if (order) {
				u16 x;
				while ((x = *order++) != ORDER_SENTINEL) {
					u8 p = data[x];

					// If filtered,
					if (rf == MonoReader::RF_PREV) {
						u16 pprev = p + num_syms - prev;
						if (pprev >= num_syms) {
							pprev -= num_syms;
						}
						prev = p;
						p = (u8)pprev;
					}

					CAT_DEBUG_ENFORCE(p < num_syms);

					_row_filter_encoder.add(p);
				}

				data += xsize;
			} else {
				for (u16 x = 0; x < xsize; ++x, ++data, ++offset) {
					// If using LZ,
					if (_params.lz_enable) {
						// If LZ match is here,
						if (lzm && offset == lzm->offset) {
							_lz.train(lzm, _row_filter_encoder);
							lzm = lzm->next;
						}

						// If pixel is LZ masked,
						if (_lz.masked(x, y)) {
							u8 p = data[0];
							prev = p;
							continue;
						}
					}

					if (!_params.mask(x, y)) {
						u8 p = data[0];

						// If filtered,
						if (rf == MonoReader::RF_PREV) {
							u16 pprev = p + num_syms - prev;
							if (pprev >= num_syms) {
								pprev -= num_syms;
							}
							prev = p;
							p = (u8)pprev;
						}

						CAT_DEBUG_ENFORCE(p < num_syms);

						_row_filter_encoder.add(p);
					}
				}
			}
		}

		// Finalize the row filter encoder
		_row_filter_entropy = _row_filter_encoder.finalize();
	}
}

void MonoWriter::maskTiles() {
	const u16 tile_xsize = _profile->tile_xsize, tile_ysize = _profile->tile_ysize;
	const u16 xsize = _params.xsize, ysize = _params.ysize;
	u8 *m = _profile->mask.get();

	// For each tile,
	for (u16 y = 0; y < ysize; y += tile_ysize) {
		for (u16 x = 0; x < xsize; x += tile_xsize, ++m) {

			// For each element in the tile,
			u16 py = y, cy = tile_ysize;
			while (cy-- > 0 && py < ysize) {
				u16 px = x, cx = tile_xsize;
				while (cx-- > 0 && px < xsize) {
					// If it is not masked,
					if (!_params.mask(px, py)) {
						if (!_params.lz_enable || !_lz.masked(px, py)) {
							// We need to do this tile
							*m = 0;
							goto next_tile;
						}
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
	const u16 tile_xsize = _profile->tile_xsize, tile_ysize = _profile->tile_ysize;
	const u16 xsize = _params.xsize, ysize = _params.ysize;
	const u16 tile_bits_x = _profile->tile_bits_x, tile_bits_y = _profile->tile_bits_y;
	u8 *p = _profile->tiles.get();

	u32 hist[MAX_SYMS] = { 0 };

	// For each tile,
	const u8 *topleft_row = _params.data;
	for (u16 y = 0; y < ysize; y += tile_ysize) {
		const u8 *topleft = topleft_row;

		for (u16 x = 0; x < xsize; x += tile_xsize, ++p, topleft += tile_xsize) {
			// If tile is masked,
			if (IsMasked(x >> tile_bits_x, y >> tile_bits_y)) {
				continue;
			}

			bool uniform = true, seen = false;
			u8 uniform_value = 0;

			// For each element in the tile,
			const u8 *row = topleft;
			u16 py = y, cy = tile_ysize;
			while (cy-- > 0 && py < ysize) {
				const u8 *data = row;
				u16 px = x, cx = tile_xsize;
				while (cx-- > 0 && px < xsize) {
					// If element is not masked,
					if (!_params.mask(px, py)) {
						if (!_params.lz_enable || !_lz.masked(px, py)) {
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
					}
					++px;
					++data;
				}
				++py;
				row += xsize;
			}

			// If uniform data,
			if (uniform) {
				hist[uniform_value]++;
			}
		}

		topleft_row += _params.xsize * tile_ysize;
	}

	// Determine threshold
	u32 sympal_thresh = static_cast<u32>( _params.sympal_thresh * _profile->tiles_count);
	int sympal_count = 0;

	// For each histogram bin,
	for (int sym = 0, num_syms = _params.num_syms; sym < num_syms; ++sym) {
		u32 coverage = hist[sym];

		// If filter is worth adding,
		if (coverage > 0 && coverage >= sympal_thresh) {
			// Add it
			_profile->sympal[sympal_count++] = (u8)sym;

#ifdef CAT_DUMP_FILTERS
			CAT_INANE("Mono") << " - Added symbol palette filter for symbol " << (int)sym;
#endif

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
	const u16 tile_xsize = _profile->tile_xsize, tile_ysize = _profile->tile_ysize;
	const u16 xsize = _params.xsize, ysize = _params.ysize;
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
	for (u16 y = 0; y < ysize; y += tile_ysize) {
		const u8 *topleft = topleft_row;

		for (u16 x = 0; x < xsize; x += tile_xsize, ++p, topleft += tile_xsize) {
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
			u16 py = y, cy = tile_ysize;
			while (cy-- > 0 && py < ysize) {
				const u8 *data = row;
				u16 px = x, cx = tile_xsize;
				while (cx-- > 0 && px < xsize) {
					// If element is not masked,
					if (!_params.mask(px, py)) {
						if (!_params.lz_enable || !_lz.masked(px, py)) {
							const u8 value = *data;

							if (!seen) {
								uniform_value = value;
								seen = true;
							} else if (value != uniform_value) {
								uniform = false;
							}

							for (int f = 0; f < SF_COUNT; ++f) {
								u8 prediction = MONO_FILTERS[f].safe(data, num_syms, px, py, xsize);
								u16 residual = value + num_syms - prediction;
								if (residual >= num_syms) {
									residual -= num_syms;
								}
								u8 score = MonoChaos::ResidualScore(static_cast<u8>( residual ), num_syms); // lower = better

								scores.add(f, score);
							}
						}
					}
					++px;
					++data;
				}
				++py;
				row += xsize;
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

		topleft_row += _params.xsize * tile_ysize;
	}

	// Decide how many filters to sort by score
	int count = _params.max_filters;
	if (count > SF_COUNT) {
		count = SF_COUNT;
	}

	// Prepare to reduce the sympal set size
	int sympal_f = 0;

	// Choose remaining filters until coverage is acceptable
	int normal_f = 0; // Next normal filter index
	int filters_set = 0; // Total filters
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

		// If filter is a sympal,
		if (index >= SF_COUNT) {
			// Map it from sympal filter index to new filter index
			int sympal_filter = index - SF_COUNT;
			_sympal_filter_map[sympal_filter] = sympal_f;

			palette[sympal_f] = index;
			++sympal_f;

#ifdef CAT_DUMP_FILTERS
			CAT_INANE("Mono") << " + Added palette filter " << sympal_f << " for palette index " << sympal_filter << " Score " << score;
#endif
		} else {
			_profile->filters[normal_f] = MONO_FILTERS[index];
			_profile->filter_indices[normal_f] = index;
			++normal_f;

			coverage += score;

#ifdef CAT_DUMP_FILTERS
			CAT_INANE("Mono") << " - Added filter " << normal_f << " for filter index " << index << " Score " << score << " Coverage " << coverage;
#endif
		}

		++filters_set;
		if (filters_set >= max_filters) {
			break;
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

#ifdef CAT_DUMP_FILTERS
	CAT_INANE("Mono") << " + Chose " << _profile->filter_count << " filters : " << _profile->sympal_filter_count << " of which are palettes";
#endif
}

void MonoWriter::designPaletteTiles() {
	if (_profile->sympal_filter_count < 0) {
#ifdef CAT_DUMP_FILTERS
		CAT_INANE("Mono") << "No palette filters selected";
#endif
		return;
	}

#ifdef CAT_DUMP_FILTERS
	CAT_INANE("Mono") << "Designing palette tiles for " << _profile->tiles_x << "x" << _profile->tiles_y << "...";
#endif

	const u16 tile_xsize = _profile->tile_xsize, tile_ysize = _profile->tile_ysize;
	const u16 xsize = _params.xsize, ysize = _params.ysize;
	const u16 tile_bits_x = _profile->tile_bits_x, tile_bits_y = _profile->tile_bits_y;
	u8 *p = _profile->tiles.get();

	// For each tile,
	const u8 *topleft_row = _params.data;
	for (u16 y = 0; y < ysize; y += tile_ysize) {
		const u8 *topleft = topleft_row;

		for (u16 x = 0; x < xsize; x += tile_xsize, ++p, topleft += tile_xsize) {
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

		topleft_row += _params.xsize * tile_ysize;
	}
}

void MonoWriter::designTiles() {
	//CAT_INANE("Mono") << "Designing tiles for " << _profile->tiles_x << "x" << _profile->tiles_y << "...";

	const u16 tile_xsize = _profile->tile_xsize, tile_ysize = _profile->tile_ysize;
	const u16 xsize = _params.xsize, ysize = _params.ysize;
	const u16 num_syms = _params.num_syms;

	EntropyEstimator ee;
	ee.init();

	const u32 code_stride = tile_xsize * tile_ysize;
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

		for (u16 y = 0; y < ysize; y += tile_ysize, ++ty) {
			const u8 *topleft = topleft_row;
			int tx = 0;

			for (u16 x = 0; x < xsize; x += tile_xsize, ++p, topleft += tile_xsize, ++tx) {
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
						u16 py = y, cy = tile_ysize;
						while (cy-- > 0 && py < ysize) {
							const u8 *data = row;
							u16 px = x, cx = tile_xsize;
							while (cx-- > 0 && px < xsize) {
								// If element is not masked,
								if (!_params.mask(px, py)) {
									if (!_params.lz_enable || !_lz.masked(px, py)) {
										const u8 value = *data;

										u8 prediction = _profile->filters[old_filter].safe(data, num_syms, px, py, xsize);
										u16 residual = value + num_syms - prediction;
										if (residual >= num_syms) {
											residual -= num_syms;
										}

										codes[code_count++] = static_cast<u8>( residual );
									}
								}
								++px;
								++data;
							}
							++py;
							row += xsize;
						}

						ee.subtract(codes, code_count);
					}
				}

				int code_count = 0;

				// For each element in the tile,
				const u8 *row = topleft;
				u16 py = y, cy = tile_ysize;
				while (cy-- > 0 && py < ysize) {
					const u8 *data = row;
					u16 px = x, cx = tile_xsize;
					while (cx-- > 0 && px < xsize) {
						// If element is not masked,
						if (!_params.mask(px, py)) {
							if (!_params.lz_enable || !_lz.masked(px, py)) {
								const u8 value = *data;

								u8 *dest = codes + code_count;
								for (int f = 0, f_end = _profile->filter_count; f < f_end; ++f) {
									if (_profile->filter_indices[f] < SF_COUNT) {
										u8 prediction = _profile->filters[f].safe(data, num_syms, px, py, xsize);
										u16 residual = value + num_syms - prediction;
										if (residual >= num_syms) {
											residual -= num_syms;
										}

										*dest = static_cast<u8>( residual );
									}

									dest += code_stride;
								}

								++code_count;
							}
						}
						++px;
						++data;
					}
					++py;
					row += xsize;
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

			topleft_row += _params.xsize * tile_ysize;
		}

		++passes;

		//CAT_INANE("Mono") << "Revisiting filter selections from the top... " << revisitCount << " left";
	}
}

void MonoWriter::computeResiduals() {
	//CAT_INANE("Mono") << "Executing tiles to generate residual matrix...";

	const u16 xsize = _params.xsize, ysize = _params.ysize;
	const u16 tile_bits_x = _profile->tile_bits_x, tile_bits_y = _profile->tile_bits_y;
	const u16 num_syms = _params.num_syms;

	const u8 *data = _params.data;
	u8 *residuals = _profile->residuals.get();
	const u16 *order = _params.write_order;
	u8 *replay;

	// If random-access input data,
	if (order) {
		// Initialize replay matrix
		_replay.resizeZero(_params.xsize * _params.ysize);

		replay = _replay.get();
	}

	// For each row of pixels,
	for (u16 y = 0; y < ysize; ++y) {
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
				u8 prediction = _profile->filters[f].safe(&replay[x], num_syms, x, y, xsize);
				u16 residual = value + num_syms - prediction;
				if (residual >= num_syms) {
					residual -= num_syms;
				}

				// Store residual
				replay[x] = value;
				residuals[x] = static_cast<u8>( residual );
			}

			data += xsize;
			replay += xsize;
			residuals += xsize;
		} else {
			for (u16 x = 0; x < xsize; ++x, ++residuals, ++data) {
				// If pixel is LZ masked,
				if (_params.lz_enable && _lz.masked(x, y)) {
					continue;
				}

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
					u8 prediction = _profile->filters[f].safe(data, num_syms, x, y, xsize);
					u16 residual = value + num_syms - prediction;
					if (residual >= num_syms) {
						residual -= num_syms;
					}

					// Store residual
					residuals[0] = static_cast<u8>( residual );
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

void MonoWriter::generateWriteOrder(u16 xsize, u16 ysize, MaskDelegate mask, u16 tile_bits, std::vector<u16> &order) {
	// Ensure that vector is clear
	order.clear();

	// Generate write order data for recursive operation
	const u16 tile_mask = (1 << tile_bits) - 1;
	const u16 tiles = (xsize + tile_mask) >> tile_bits;

	SmartArray<u8> seen;
	seen.resize(tiles);

	// For each pixel row,
	for (u16 y = 0; y < ysize; ++y) {
		// If starting a tile row,
		if ((y & tile_mask) == 0) {
			seen.fill_00();

			// After the first tile row,
			if (y > 0) {
				order.push_back(ORDER_SENTINEL);
			}
		}

		// For each pixel,
		for (u16 x = 0; x < xsize; ++x) {
			// If pixel is not masked out,
			if (!mask(x, y)) {
				// If tile seen for the first time,
				u16 tx = x >> tile_bits;
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
	_profile->write_order.clear();

	// Generate write order data for recursive operation
	const u16 tile_bits_x = _profile->tile_bits_x;
	const u16 tile_mask_y = _profile->tile_ysize - 1;
	const u16 *order = _params.write_order;

	// For each pixel row,
	for (u16 y = 0; y < _params.ysize; ++y) {
		// If starting a tile row,
		if ((y & tile_mask_y) == 0) {
			_tile_seen.fill_00();

			// After the first tile row,
			if (y > 0) {
				_profile->write_order.push_back(ORDER_SENTINEL);
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

					_profile->write_order.push_back(tx);
				}
			}
		} else {
			// For each pixel,
			for (u16 x = 0, xsize = _params.xsize; x < xsize; ++x) {
				// If pixel is LZ matched,
				if (_params.lz_enable && _lz.masked(x, y)) {
					continue;
				}

				// If pixel is not masked out,
				if (!_params.mask(x, y)) {
					// If tile seen for the first time,
					u16 tx = x >> tile_bits_x;
					if (_tile_seen[tx] == 0) {
						_tile_seen[tx] = 1;

						_profile->write_order.push_back(tx);
					}
				}
			}
		}
	}

	_profile->write_order.push_back(ORDER_SENTINEL);
}

bool MonoWriter::IsMasked(u16 x, u16 y) {
	return _profile->mask[x + y * _profile->tiles_x] != 0;
}

void MonoWriter::recurseCompress() {
	const u16 tiles_x = _profile->tiles_x, tiles_y = _profile->tiles_y;

	//CAT_INANE("Mono") << "Recursively compressing tiles for " << tiles_x << "x" << tiles_y << "...";

	_profile->filter_encoder = new MonoWriter;

	Parameters params = _params;
	params.data = _profile->tiles.get();
	params.num_syms = _profile->filter_count;
	params.xsize = tiles_x;
	params.ysize = tiles_y;
	params.mask.SetMember<MonoWriter, &MonoWriter::IsMasked>(this);
	params.write_order = &_profile->write_order[0];
	params.lz_enable = false;

	_profile->filter_encoder->init(params);
}

void MonoWriter::designChaos() {
	//CAT_INANE("Mono") << "Designing chaos...";

	u32 best_entropy = 0x7fffffff;

	MonoWriterProfile::Encoders *best = 0;
	MonoWriterProfile::Encoders *encoders = new MonoWriterProfile::Encoders;

	// For each chaos level,
	for (int chaos_levels = 1; chaos_levels < MAX_CHAOS_LEVELS; ++chaos_levels) {
		encoders->chaos.init(chaos_levels, _params.xsize);
		encoders->chaos.start();

		const u16 *order = _params.write_order;
		const u8 *residuals = _profile->residuals.get();

		// For each chaos level,
		for (int ii = 0; ii < chaos_levels; ++ii) {
			encoders->encoder[ii].init(_params.num_syms + (_params.lz_enable ? LZReader::ESCAPE_SYMS : 0), ZRLE_SYMS);
		}

		LZMatchFinder::LZMatch *lzm = _lz.getHead();
		int offset = 0;

		// For each row,
		for (u16 y = 0; y < _params.ysize; ++y) {
			const u16 ty = y >> _profile->tile_bits_y;

			// If random write order,
			if (order) {
				// After the first one,
				if (y > 0) {
					// Simulate zeroing the chaos residuals
					for (u16 x = 0; x < _params.xsize; ++x) {
						if (_params.mask(x, y - 1)) {
							encoders->chaos.zero(x);
						}
					}
				}

				u16 x;
				while ((x = *order++) != ORDER_SENTINEL) {
					CAT_DEBUG_ENFORCE(!_params.mask(x, y));

					const u16 tx = x >> _profile->tile_bits_x;
					CAT_DEBUG_ENFORCE(tx < _profile->tiles_x);

					const u8 f = _profile->getTile(tx, ty);
					CAT_DEBUG_ENFORCE(f < _profile->filter_count);

					// If masked or sympal,
					if (_profile->filter_indices[f] >= SF_COUNT) {
						encoders->chaos.zero(x);
					} else {
						// Get residual symbol
						u8 residual = residuals[x];

						// Get chaos bin
						int chaos = encoders->chaos.get(x);
						encoders->chaos.store(x, residual, _params.num_syms);

						// Add to histogram for this chaos bin
						encoders->encoder[chaos].add(residual);
					}
				}

				residuals += _params.xsize;
			} else {
				// For each column,
				for (u16 x = 0; x < _params.xsize; ++x, ++residuals, ++offset) {
					// If using LZ,
					if (_params.lz_enable) {
						// If LZ match is here,
						if (lzm && offset == lzm->offset) {
							int chaos = encoders->chaos.get(x);
							_lz.train(lzm, encoders->encoder[chaos]);
							lzm = lzm->next;
						}

						// If pixel is LZ masked,
						if (_lz.masked(x, y)) {
							encoders->chaos.zero(x);
							continue;
						}
					}

					const u16 tx = x >> _profile->tile_bits_x;
					CAT_DEBUG_ENFORCE(tx < _profile->tiles_x);

					if (_params.mask(x, y)) {
						encoders->chaos.zero(x);
					} else {
						const u8 f = _profile->getTile(tx, ty);
						CAT_DEBUG_ENFORCE(f < _profile->filter_count);

						// If masked or sympal,
						if (_profile->filter_indices[f] >= SF_COUNT) {
							encoders->chaos.zero(x);
						} else {
							// Get residual symbol
							u8 residual = residuals[0];

							// Get chaos bin
							int chaos = encoders->chaos.get(x);
							encoders->chaos.store(x, residual, _params.num_syms);

							// Add to histogram for this chaos bin
							encoders->encoder[chaos].add(residual);
						}
					}
				}
			}
		}

		// For each chaos level,
		u32 entropy = 0;
		for (int ii = 0; ii < chaos_levels; ++ii) {
			entropy += encoders->encoder[ii].finalize();
		}

		//CAT_WARN("CHAOS") << chaos_levels << " -> " << entropy;

		// If this is the best chaos levels so far,
		if (best_entropy > entropy + 128) {
			best_entropy = entropy;
			MonoWriterProfile::Encoders *temp = best;
			best = encoders;
			best->bits = entropy;
			if (temp) {
				encoders = temp;
			} else {
				encoders = new MonoWriterProfile::Encoders;
			}
		}

		// If we have not found a better one in 4 moves,
		if (chaos_levels - best->chaos.getBinCount() >= 2) {
			// Stop early to save time
			break;
		}
	}

	_profile->encoders = best;

	if (encoders) {
		delete encoders;
	}
}

u32 MonoWriter::simulate() {
	u32 bits = 1;

	// If using row filters,
	if (_use_row_filters) {
		// This is precomputed
		bits += _row_filter_entropy;
	} else {
		// Chaos overhead
		bits += 4 + _profile->encoders->chaos.getBinCount() * 5 * _params.num_syms;

		// Tile bits overhead
		bits += _tile_bits_field_bc;

		// Sympal choice overhead
		bits += 4 + 8 * _profile->sympal_filter_count;

		// Filter choice overhead
		bits += 5 + 7 * _profile->filter_count;

		// If not using row encoders for filters,
		bits += _profile->filter_encoder->simulate();

		// Simulate residuals
		bits += _profile->encoders->bits;
	}

	return bits;
}

void MonoWriter::init(const Parameters &params) {
	cleanup();

	// Initialize
	_params = params;

	// Only allow LZ to be enabled when write order is not specified
	CAT_DEBUG_ENFORCE(!params.lz_enable || !params.write_order);

	_row_filters.resize(_params.ysize);

	CAT_DEBUG_ENFORCE(params.num_syms > 0);

	// Calculate bits to represent tile bits field
	u32 range = _params.max_bits - _params.min_bits;
	int bits_bc = 0;
	if (range > 0) {
		bits_bc = BSR32(range) + 1;
	}
	_tile_bits_field_bc = bits_bc;

	//CAT_INANE("Mono") << "!! Monochrome filter processing started for " << _params.xsize << "x" << _params.ysize << " data matrix...";

	// Try to reuse the same profile object
	MonoWriterProfile *profile = new MonoWriterProfile;
	u32 best_entropy = 0x7fffffff;
	MonoWriterProfile *best_profile = 0;

	const u32 pixel_count = _params.xsize * _params.ysize;

	// If LZ77 is enabled,
	if (params.lz_enable && pixel_count >= LZ_THRESH) {
		_params.lz_enable = false;

		// Do a fast trial of filtering without LZ masking to measure the cost per bit
		profile->init(params.xsize, params.ysize, params.min_bits);
		_profile = profile;

		// Compute residuals for the tightest filters to get a good upper bound
		maskTiles();
		designPaletteFilters();
		designFilters();
		designPaletteTiles();
		designTiles();
		computeResiduals();
		optimizeTiles();
		designChaos();
		priceResiduals();

		// Design LZ matches
		designLZ();

		_params.lz_enable = true;
	} else {
		_params.lz_enable = false;
	}

	// Try simple row filter first
	designRowFilters();

	// If the data is too small to bother with tiles,
	if (pixel_count >= TILE_THRESH) {
		// Disable it for now
		_use_row_filters = false;

		// For each tile size to try,
		for (int bits = params.min_bits; bits <= params.max_bits; ++bits) {
			// Set up a profile
			profile->init(params.xsize, params.ysize, bits);
			_profile = profile;

			// Generate tile-based encoder
			maskTiles();
			designPaletteFilters();
			designFilters();
			designPaletteTiles();
			designTiles();
			computeResiduals();
			optimizeTiles();
			generateWriteOrder();
			recurseCompress();
			designChaos();

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
	}

	// Check if row filters should be used instead of tiles
	_use_row_filters = (best_entropy >= _row_filter_entropy);
}

int MonoWriter::writeTables(ImageWriter &writer) {
	// Initialize stats
	Stats.basic_overhead_bits = 1;
	Stats.encoder_overhead_bits = 0;
	Stats.filter_overhead_bits = 0;
	Stats.data_bits = 0;
	Stats.lz_bits = 0;

#ifdef CAT_DEBUG
	_next_write_pixel_order = _params.write_order;
#endif

	// Write LZ enable bit
	writer.writeBit(_params.lz_enable ? 1 : 0);

	// If not using write profile,
	if (_use_row_filters) {
		writer.writeBit(0);

		// If only using one row filter,
		if (_one_row_filter) {
			writer.writeBit(1);
			writer.writeBit(_row_filters[0]);
		} else {
			writer.writeBit(0);
		}

		DESYNC_TABLE();

		// Write row filter encoder overhead
		Stats.filter_overhead_bits += _row_filter_encoder.writeTables(writer);

		DESYNC_TABLE();
	} else {
		// Enable encoders
		writer.writeBit(1);

		// Write tile size
		CAT_DEBUG_ENFORCE(_profile->tile_bits_x == _profile->tile_bits_y);	// Square regions only for now

		if (_tile_bits_field_bc > 0) {
			writer.writeBits(_profile->tile_bits_x - _params.min_bits, _tile_bits_field_bc);
			Stats.basic_overhead_bits += _tile_bits_field_bc;
		}

		DESYNC_TABLE();

		// Sympal filters
		CAT_DEBUG_ENFORCE(MAX_PALETTE == 15);

		writer.writeBits(_profile->sympal_filter_count, 4);
		for (int f = 0, f_end = _profile->sympal_filter_count; f < f_end; ++f) {
			writer.writeBits(_profile->sympal[f], 8);
			Stats.basic_overhead_bits += 8;
		}

		DESYNC_TABLE();

		// Normal filters
		CAT_DEBUG_ENFORCE(MAX_FILTERS <= 32);
		CAT_DEBUG_ENFORCE(SF_COUNT + MAX_PALETTE <= 128);
		CAT_DEBUG_ENFORCE(_profile->filter_count > 0);

		writer.writeBits(_profile->filter_count - 1, 5);
		Stats.basic_overhead_bits += 5;
		for (int f = 0, f_end = _profile->filter_count; f < f_end; ++f) {
			writer.writeBits(_profile->filter_indices[f], 7);
			Stats.basic_overhead_bits += 7;
		}

		DESYNC_TABLE();

		// Write chaos levels
		CAT_DEBUG_ENFORCE(MAX_CHAOS_LEVELS <= 16);

		writer.writeBits(_profile->encoders->chaos.getBinCount() - 1, 4);
		Stats.basic_overhead_bits += 4;

		DESYNC_TABLE();

		// Write encoder tables
		for (int ii = 0, iiend = _profile->encoders->chaos.getBinCount(); ii < iiend; ++ii) {
			Stats.encoder_overhead_bits += _profile->encoders->encoder[ii].writeTables(writer);
		}

		DESYNC_TABLE();

		// Recurse write tables
		Stats.filter_overhead_bits += _profile->filter_encoder->writeTables(writer);

		// Initialize tile seen array
		_tile_seen.resize(_profile->tiles_x);

#ifdef CAT_DEBUG
		generateWriteOrder();
		_next_write_tile_order = &_profile->write_order[0];
#endif

		_profile->encoders->chaos.start();

		for (int ii = 0, iiend = _profile->encoders->chaos.getBinCount(); ii < iiend; ++ii) {
			_profile->encoders->encoder[ii].reset();
		}
	}

	DESYNC_TABLE();

	if (_params.lz_enable) {
		Stats.lz_table_bits = _lz.writeTables(writer);
		_lz_next = _lz.getHead();
	} else {
		Stats.lz_table_bits = 0;
	}

	DESYNC_TABLE();

	return Stats.encoder_overhead_bits + Stats.basic_overhead_bits + Stats.filter_overhead_bits + Stats.lz_table_bits;
}

int MonoWriter::writeRowHeader(u16 y, ImageWriter &writer) {
	CAT_DEBUG_ENFORCE(y < _params.ysize);

	int bits = 0;

	// If using row filters,
	if (_use_row_filters) {
		CAT_DEBUG_ENFORCE(MonoReader::RF_COUNT == 2);
		CAT_DEBUG_ENFORCE(_row_filters[y] < 2);

		// If different row filters,
		if (!_one_row_filter) {
			// Write out chosen row filter
			writer.writeBit(_row_filters[y]);
			bits++;
		}

		// Clear prev filter
		_prev_filter = 0;
	} else {
		// If at the start of a tile row,
		const u16 tile_mask_y = _profile->tile_ysize - 1;
		if ((y & tile_mask_y) == 0) {
			// After the first row,
			if (y > 0) {
				// For each pixel in seen row,
				for (u16 tx = 0; tx < _profile->tiles_x; ++tx) {
					if (_tile_seen[tx] == 0) {
						_profile->filter_encoder->zero(tx);
					}
				}
#ifdef CAT_DEBUG
				// Skip sentinel at row ends
				++_next_write_tile_order;
#endif
			}

			// Clear tile seen
			_tile_seen.fill_00();

			// Recurse start row
			u16 ty = y >> _profile->tile_bits_y;
			bits += _profile->filter_encoder->writeRowHeader(ty, writer);
		}
	}

#ifdef CAT_DEBUG
	if (_next_write_pixel_order) {
		if (y > 0) {
			_next_write_pixel_order++;
		}
	}
#endif

	DESYNC(0, y);

	Stats.filter_overhead_bits += bits;
	return bits;
}

void MonoWriter::zero(u16 x) {
	if (!_use_row_filters) {
		_profile->encoders->chaos.zero(x);
	}
}

u8 MonoWriter::writeFilter(u16 x, u16 y, ImageWriter &writer, int &overhead_bits) {
	// Get tile
	const u16 tx = x >> _profile->tile_bits_x;
	const u16 ty = y >> _profile->tile_bits_y;

	CAT_DEBUG_ENFORCE(!IsMasked(tx, ty));

	// If tile not seen yet,
	if (_tile_seen[tx] == 0) {
		_tile_seen[tx] = 1;

		CAT_DEBUG_ENFORCE(*_next_write_tile_order++ == tx);

		// Pass filter write down the tree
		overhead_bits += _profile->filter_encoder->write(tx, ty, writer);

		DESYNC(x, y);
	}

	return _profile->getTile(tx, ty);
}

int MonoWriter::write(u16 x, u16 y, ImageWriter &writer) {
	int overhead_bits = 0, data_bits = 0;

	CAT_DEBUG_ENFORCE(x < _params.xsize && y < _params.ysize);
	CAT_DEBUG_ENFORCE(!_params.mask(x, y));
	CAT_DEBUG_ENFORCE(!_next_write_pixel_order || *_next_write_pixel_order++ == x);

	DESYNC(x, y);

	const int offset = x + _params.xsize * y;
	const u8 *data = _params.data + offset;

	// If using LZ,
	if (_params.lz_enable) {
		// If next match is here,
		if (_lz_next && _lz_next->offset == offset) {
			CAT_DEBUG_ENFORCE(x + _lz_next->length <= _params.xsize);
			CAT_DEBUG_ENFORCE(offset >= _lz_next->distance);

			// If using row filters,
			int lz_bits;
			if (_use_row_filters) {
				lz_bits = _lz.write(_lz_next, _row_filter_encoder, writer);

				// Set filter to the last one in the match region
				_prev_filter = data[_lz_next->length - 1];
			} else {
				// Write filter before writing LZ to sync with decoder
				writeFilter(x, y, writer, overhead_bits);

				int chaos = _profile->encoders->chaos.get(x);
				lz_bits = _lz.write(_lz_next, _profile->encoders->encoder[chaos], writer);

				// Zero chaos for match region
				_profile->encoders->chaos.zeroRegion(x, _lz_next->length);
			}

			CAT_DEBUG_ENFORCE(_lz.masked(x, y));

			Stats.lz_bits += lz_bits;
			_lz_next = _lz_next->next;

			return lz_bits;
		}

		// If this is a masked byte,
		if (_lz.masked(x, y)) {
			return 0;
		}

		// TODO: Update bit stats
	}

	// If using row filters,
	if (_use_row_filters) {
		// Calculate row filter residual for filter data (filter of filters at tree leaf)
		u8 p = *data;
		u16 rf = p;

		// If this row is filtered,
		if (_row_filters[y] == MonoReader::RF_PREV) {
			const u16 num_syms = _params.num_syms;
			rf += num_syms - _prev_filter;
			if (rf >= num_syms) {
				rf -= num_syms;
			}
			_prev_filter = p;
		}

		// Write encoded pixel
		data_bits += _row_filter_encoder.write(rf, writer);
	} else {
		u8 f = writeFilter(x, y, writer, overhead_bits);

		// If using sympal,
		if (_profile->filter_indices[f] >= SF_COUNT) {
			_profile->encoders->chaos.zero(x);
		} else {
			// Look up residual sym
			u8 residual = _profile->residuals[offset];

			// Calculate local chaos
			int chaos = _profile->encoders->chaos.get(x);
			_profile->encoders->chaos.store(x, residual, _params.num_syms);

			// Write the residual value
			data_bits += _profile->encoders->encoder[chaos].write(residual, writer);
		}
	}

	DESYNC(x, y);

	// Update stats
	Stats.filter_overhead_bits += overhead_bits;
	Stats.data_bits += data_bits;

	return overhead_bits + data_bits;
}

void MonoWriter::dumpStats() {
	if (_use_row_filters) {
		CAT_INANE("Mono") << "Using row-filtered encoder for " << _params.xsize << "x" << _params.ysize << " image";
	} else {
		CAT_INANE("Mono") << "Designed monochrome writer using " << _profile->tiles_x << "x" << _profile->tiles_y << " tiles to express " << _profile->filter_count << " (" << _profile->sympal_filter_count << " palette) filters for " << _params.xsize << "x" << _params.ysize << " image with " << _profile->encoders->chaos.getBinCount() << " chaos bins";
	}

	CAT_INANE("Mono") << " -   Basic Overhead : " << Stats.basic_overhead_bits << " bits (" << Stats.basic_overhead_bits/8 << " bytes)";
	CAT_INANE("Mono") << " - Encoder Overhead : " << Stats.encoder_overhead_bits << " bits (" << Stats.encoder_overhead_bits/8 << " bytes)";
	CAT_INANE("Mono") << " -  Filter Overhead : " << Stats.filter_overhead_bits << " bits (" << Stats.filter_overhead_bits/8 << " bytes)";
	CAT_INANE("Mono") << " -  Monochrome Data : " << Stats.data_bits << " bits (" << Stats.data_bits/8 << " bytes)";

	if (_params.lz_enable) {
		CAT_INANE("Mono") << " -         LZ Table : " << Stats.lz_table_bits << " bits (" << Stats.lz_table_bits/8 << " bytes)";
		CAT_INANE("Mono") << " -          LZ Data : " << Stats.lz_bits << " bits (" << Stats.lz_bits/8 << " bytes)";
	}

	if (!_use_row_filters) {
		CAT_INANE("Mono") << " - Recursively using filter encoder:";
		_profile->filter_encoder->dumpStats();
	}
}

