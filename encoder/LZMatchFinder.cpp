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
using namespace cat;

void LZMatchFinder::scanRGBA(const u32 *rgba, int xsize, int ysize) {
	const int pixels = xsize * ysize;

	// Allocate and zero the table and chain
	_table.resizeZero(HASH_SIZE);
	_chain.resizeZero(pixels);

	// For each pixel, stopping just before the last pixel:
	const u32 *rgba_now = rgba;
	for (int ii = 0, iiend = pixels - 1; ii < iiend; ++ii, ++rgba_now) {
		const u32 hash = HashPixels(rgba_now);
		int min_match = MIN_MATCH - 1;
		u32 best_node = 0;
		u32 best_score = 0;

		// For each hash collision,
		for (u32 node = _table[hash]; node != 0; node = _chain[node]) {
			--node; // Fix node from table data

			// If distance is beyond the window size,
			u32 distance = ii - node;
			if (distance > WIN_SIZE) {
				// Stop searching here
				break;
			}

			// Find match length
			int match_len = 0;
			const u32 *rgba_node = rgba + node;
			for (; match_len < MAX_MATCH && rgba_node[match_len] == rgba_now[match_len]; ++match_len);

			// If match is at least as long as required,
			if (min_match < match_len) {
				// Score it
				u32 distance_bits = BSR32(distance) - 2;
				u32 length_bits = BSR32(match_len) - 2;
				u32 score = APPROX_PREFIX_COST + distance_bits + APPROX_PREFIX_COST + length_bits;

				// If it has the best score,
				if (best_score > score) {
					// Use this one
					best_score = score;
					min_match = match_len;
					best_node = node;
				}
			}
		}

		// If a best node was found,
		if (best_node) {
		}

		// Insert at front of chain for this hash
		_chain[ii] = _table[hash] + 1;
		_table[hash] = ii;
	}
}

