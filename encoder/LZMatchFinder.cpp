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
		u16 best_length = 0;
		u16 min_match = MIN_MATCH;
		u32 best_distance = 0;
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
			if (match_len >= min_match) {
				/*
				 * Encoding cost in bits:
				 * ~LEN_PREFIX_COST bits for Y-channel escape code and length bit range
				 * ~log2(length)-K bits for length extension bits
				 * log2(40) ~= DIST_PREFIX_COST bits for distance bit range
				 * ~log2(distance)-K bits for the distance extension bits
				 *
				 * Assuming the normal compression ratio of a 32-bit RGBA pixel is 3.6:1,
				 * it saves about SAVED_PIXEL_BITS bits per RGBA pixel that we can copy.
				 *
				 * Two pixels is about breaking even, though it can be a win if it's
				 * from the local neighborhood.  For decoding speed it is preferred to
				 * use LZ since it avoids a bunch of Huffman decodes.  And most of the
				 * big LZ wins are on computer-generated artwork where neighboring
				 * scanlines can be copied, so two-pixel copies are often useful.
				 */
				const s32 distance_bits = BSR32(distance) - 2;
				const s32 saved_bits = match_len * SAVED_PIXEL_BITS;
				const s32 length_bits = BSR32(match_len) - 2;
				const s32 cost_bits = DIST_PREFIX_COST + distance_bits + LEN_PREFIX_COST + length_bits;
				const s32 score = saved_bits - cost_bits;

				// If it has the best score,
				if (best_score < score) {
					// Use this one
					best_score = score;
					min_match = match_len;
					best_node = node;
				}
			}
		}

		// If a best node was found,
		if (best_length > 0) {
			_matches.push_back(LZMatch(ii, best_distance, best_length));
		}

		// Insert current and any matched pixels
		for (int jj = ii, jjend = ii + best_length; jj <= jjend; ++jj) {
			_chain[ii] = _table[hash] + 1;
			_table[hash] = ii;
		}

		// Skip matched pixels
		ii += best_length;
	}
}

