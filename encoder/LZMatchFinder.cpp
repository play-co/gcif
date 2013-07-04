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

#include "LZMatchFinder.hpp"
#include "../decoder/BitMath.hpp"
#include "Log.hpp"
using namespace cat;

bool RGBAMatchFinder::findMatches(const u32 *rgba, int xsize, int ysize, ImageMaskWriter *image_mask) {
	// Setup mask
	const u32 mask_color = image_mask->getColor();
	const bool using_mask = image_mask->enabled();

	// Allocate and zero the table and chain
	const int pixels = xsize * ysize;
	SmartArray<u32> table, chain;
	table.resizeZero(HASH_SIZE);
	chain.resizeZero(pixels);

	// Clear mask
	_size_x = xsize;
	const int mask_size = (xsize * ysize + 31) / 32;
	_mask.resizeZero(mask_size);

	// For each pixel, stopping just before the last pixel:
	const u32 *rgba_now = rgba;
	u16 x = 0, y = 0;
	for (int ii = 0, iiend = pixels - 1; ii < iiend;) {
		const u32 hash = HashPixels(rgba_now);
		u16 best_length = MIN_MATCH - 1;
		u32 best_distance = 0;

		// If not masked,
		if (!image_mask->masked(x, y)) {
			// For each hash collision,
			for (u32 node = table[hash]; node != 0; node = chain[node - 1]) {
				--node; // Fix node from table data

				// If distance is beyond the window size,
				u32 distance = ii - node;
				if (distance > WIN_SIZE) {
					// Stop searching here
					break;
				}

				// Fast reject potential matches that are too short
				const u32 *rgba_node = rgba + node;
				if (rgba_node[best_length] != rgba_now[best_length]) {
					continue;
				}

				// Find match length
				int match_len = 0;
				for (int jj = 0; jj < MAX_MATCH && rgba_node[jj] == rgba_now[jj];) {
					++jj;

					if (!using_mask || rgba_now[match_len] != mask_color) {
						match_len = jj;
					}
				}

				// Future matches will be farther away (more expensive in distance)
				// so they should be at least as long as previous matches to be considered
				if (match_len > best_length) {
					best_distance = distance;
					best_length = match_len;

					// If length is at the limit,
					if (match_len >= MAX_MATCH) {
						// Stop here
						break;
					}
				}
			}
		}

		// Insert current pixel
		chain[ii] = table[hash] + 1;
		table[hash] = ++ii;
		++rgba_now;
		++x;

		// If a best node was found,
		if (best_distance > 0) {
			const int offset = ii - 1;

			// Calculate saved bit count
			const s32 distance_bits = best_distance < 8 ? 0 : BSR32(best_distance >> 2);
			const s32 saved_bits = best_length * SAVED_PIXEL_BITS;
			const s32 length_bits = best_length < 8 ? 0 : BSR32(best_length >> 2);
			const s32 cost_bits = DIST_PREFIX_COST + distance_bits + LEN_PREFIX_COST + length_bits;
			const s32 score = saved_bits - cost_bits;

			// If score is good,
			if (score > 0) {
				_matches.push_back(LZMatch(offset, best_distance, best_length));
				//CAT_WARN("RGBATEST") << offset << " : " << best_distance << ", " << best_length;

				mask(offset);

				// Insert matched pixels
				for (int jj = 1; jj < best_length; ++jj) {
					mask(ii);
					const u32 matched_hash = HashPixels(rgba_now);
					chain[ii] = table[matched_hash] + 1;
					table[matched_hash] = ++ii;
					++rgba_now;
				}

				x += best_length - 1;
			}
		}

		while (x >= xsize) {
			x -= xsize;
			++y;
		}
	}

	_matches.push_back(LZMatch(GUARD_OFFSET, 0, 0));

	return true;
}

bool MonoMatchFinder::findMatches(const u8 *mono, int xsize, int ysize, ImageMaskWriter *image_mask) {
	// Setup mask
	const u8 mask_color = static_cast<u8>( image_mask->getColor() );
	const bool using_mask = image_mask->enabled();

	// Allocate and zero the table and chain
	const int pixels = xsize * ysize;
	SmartArray<u32> table, chain;
	table.resizeZero(HASH_SIZE);
	chain.resizeZero(pixels);

	// Clear mask
	_size_x = xsize;
	const int mask_size = (xsize * ysize + 31) / 32;
	_mask.resizeZero(mask_size);

	// For each pixel, stopping just before the last pixel:
	const u8 *mono_now = mono;
	u16 x = 0, y = 0;
	for (int ii = 0, iiend = pixels - 1; ii < iiend; ++mono_now) {
		const u32 hash = HashPixels(mono_now);
		u16 best_length = MIN_MATCH - 1;
		u32 best_distance = 0;

		// If not masked,
		if (!image_mask->masked(x, y)) {
			// For each hash collision,
			for (u32 node = table[hash]; node != 0; node = chain[node - 1]) {
				--node; // Fix node from table data

				// If distance is beyond the window size,
				u32 distance = ii - node;
				if (distance > WIN_SIZE) {
					// Stop searching here
					break;
				}

				// Fast reject potential matches that are too short
				const u8 *mono_node = mono + node;
				if (mono_node[best_length] != mono_now[best_length]) {
					continue;
				}

				// Find match length
				int match_len = 0;
				for (int jj = 0; jj < MAX_MATCH && mono_node[jj] == mono_now[jj];) {
					++jj;

					if (!using_mask || mono_now[match_len] != mask_color) {
						match_len = jj;
					}
				}

				// Future matches will be farther away (more expensive in distance)
				// so they should be at least as long as previous matches to be considered
				if (match_len > best_length) {
					best_distance = distance;
					best_length = match_len;

					// If length is at the limit,
					if (match_len >= MAX_MATCH) {
						// Stop here
						break;
					}
				}
			}
		}

		// Insert current pixel
		chain[ii] = table[hash] + 1;
		table[hash] = ++ii;
		++mono_now;
		++x;

		// If a best node was found,
		if (best_distance > 0) {
			const int offset = ii - 1;

			// Calculate saved bit count
			const s32 distance_bits = best_distance < 8 ? 0 : BSR32(best_distance >> 2);
			const s32 saved_bits = best_length * SAVED_PIXEL_BITS;
			const s32 length_bits = best_length < 8 ? 0 : BSR32(best_length >> 2);
			const s32 cost_bits = DIST_PREFIX_COST + distance_bits + LEN_PREFIX_COST + length_bits;
			const s32 score = saved_bits - cost_bits;

			// If score is good,
			if (score > 0) {
				_matches.push_back(LZMatch(offset, best_distance, best_length));
				//CAT_WARN("RGBATEST") << offset << " : " << best_distance << ", " << best_length;

				// Insert matched pixels
				for (int jj = 1; jj < best_length; ++jj) {
					const u32 matched_hash = HashPixels(mono_now);
					chain[ii] = table[matched_hash] + 1;
					table[matched_hash] = ++ii;
					++mono_now;
				}

				x += best_length - 1;
			}
		}

		while (x >= xsize) {
			x -= xsize;
			++y;
		}
	}

	_matches.push_back(LZMatch(GUARD_OFFSET, 0, 0));

	return true;
}

