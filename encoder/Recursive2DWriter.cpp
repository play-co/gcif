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

#include "Recursive2DWriter.hpp"
#include "../decoder/Enforcer.hpp"
#include "FilterScorer.hpp"
using namespace cat;


//// Recursive2DWriter

void Recursive2DWriter::cleanup() {
	if (_tiles) {
		delete []_tiles;
		_tiles = 0;
	}
}

void Recursive2DWriter::maskTiles() {
	const u16 tile_size_x = _tile_size_x, tile_size_y = _tile_size_y;
	const u16 size_x = _params->size_x, size_y = _params->size_y;
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
					if (!_params->mask(px, py)) {
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

void Recursive2DWriter::designFilters() {
	CAT_INANE("2D") << "Designing filters...";

	const u16 tile_size_x = _tile_size_x, tile_size_y = _tile_size_y;
	const u16 size_x = _params->size_x, size_y = _params->size_y;
	const u16 num_syms = _params->num_syms;
	u8 *p = _tiles;

	FilterScorer scores, awards;
	scores.init(MF_COUNT);
	awards.init(MF_COUNT);
	awards.reset();

	// For each tile,
	const u8 *topleft = _params->data;
	for (u16 y = 0; y < size_y; y += tile_size_y) {
		for (u16 x = 0; x < size_x; x += tile_size_x, ++p, topleft += tile_size_x) {
			// If tile is masked,
			if (*p == MASK_TILE) {
				continue;
			}

			scores.reset();

			// For each pixel in the tile,
			const u8 *scanline = topleft;
			u16 py = y, cy = tile_size_y;
			while (cy-- > 0 && py < size_y) {
				const u8 *data = scanline;
				u16 px = x, cx = tile_size_x;
				while (cx-- > 0 && px < size_x) {
					// If pixel is not masked,
					if (!_params->mask(px, py)) {
						const u8 value = *data;

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

			// Sort top few filters for awards
			FilterScorer::Score *top = scores.getTop(AWARD_COUNT, true);
			for (int ii = 0; ii < AWARD_COUNT; ++ii) {
				awards.add(top[ii].index, _params->AWARDS[ii]);
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
	int count = _params->max_filters + MF_FIXED;
	if (count > MF_COUNT) {
		count = MF_COUNT;
	}

	// Choose remaining filters based on rewards
	int f = MF_FIXED;
	FilterScorer::Score *top = awards.getTop(count, true);
	for (int ii = 0; ii < count; ++ii) {
		int index = top[ii].index;
		int score = top[ii].score;

		// Calculate approximate bytes covered
		int covered = score / _params->AWARDS[0];

		// If score is under threshold,
		if (top[ii].score < score_threshold) {
			break;
		}

		// If filter is not fixed,
		if (index >= MF_FIXED) {
			_filters[f] = MONO_FILTERS[index];
			_filter_indices[f] = index;
			++f;
		}
	}
}

void Recursive2DWriter::designTiles() {
	CAT_INANE("2D") << "Designing tiles...";
}

void Recursive2DWriter::filterTiles() {
}

void Recursive2DWriter::recurseCompress() {
	CAT_INANE("2D") << "Recursing...";
	CAT_INANE("2D") << "...Out";
}

void Recursive2DWriter::designChaos() {
	CAT_INANE("2D") << "Designing chaos...";
}

bool Recursive2DWriter::process(const Parameters *params) {
	cleanup();

	_params = params;

	_tile_bits_x = _params->tile_bits_x;
	_tile_bits_y = _params->tile_bits_y;
	_tile_size_x = 1 << _tile_bits_x;
	_tile_size_y = 1 << _tile_bits_y;

	// Allocate tile memory
	_tiles_count = (_params->size_x >> _tile_bits_x) * (_params->size_y >> _tile_bits_y);
	_tiles = new u8[_tiles_count];

	maskTiles();

	designFilters();

	designTiles();

	filterTiles();

	recurseCompress();

	designChaos();
}

u32 Recursive2DWriter::simulate() {
}

void Recursive2DWriter::writeTables(ImageWriter &writer) {
}

void Recursive2DWriter::write(u16 x, u16 y, ImageWriter &writer) {
}

