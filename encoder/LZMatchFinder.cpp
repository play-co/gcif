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

#include <iostream>
#include <iomanip>
using namespace std;


static void MatchEncode(u32 * CAT_RESTRICT recent, int recent_ii, LZMatchFinder::LZMatch * CAT_RESTRICT match, int xsize) {
	static const int LAST_COUNT = MonoMatchFinder::LAST_COUNT;

	match->emit_sdist = false;
	match->emit_ldist = false;
	match->extra_bits = 0;

	const u32 distance = match->distance;
	const u16 length = match->length;

	CAT_DEBUG_ENFORCE(distance > 0);
	CAT_DEBUG_ENFORCE(length >= 2);
	CAT_DEBUG_ENFORCE(length <= 256);
	CAT_DEBUG_ENFORCE(MonoMatchFinder::MIN_MATCH == 2);
	CAT_DEBUG_ENFORCE(MonoMatchFinder::MAX_MATCH == 256);

	// Try to represent the distance in the escape code:

	match->emit_len = true;
	match->len_code = length - 2;

	for (int ii = 0; ii < LAST_COUNT; ++ii) {
		if (distance == recent[(recent_ii + LAST_COUNT - 1 - ii) % LAST_COUNT]) {
			match->escape_code = ii;
			return;
		}
	}

	if (distance == 1) {
		match->escape_code = 4;
		return;
	}

	if (distance >= 3 && distance <= 6) {
		match->escape_code = distance + 2;
		return;
	}

	int start = xsize - 2;
	if (distance >= start && distance <= start + 4) {
		match->escape_code = distance - start + 9;
		return;
	}

	CAT_DEBUG_ENFORCE(distance == 2 || distance >= 7);

	if (length <= 9) {
		match->emit_len = false;
		match->escape_code = 14 + length - 2;
	} else {
		match->emit_len = true;
		match->escape_code = 22;
	}

	// Now the escape code depends on which distance Huffman code we are using:

	match->emit_sdist = true;

	if (distance == 2) {
		match->sdist_code = 0;
		return;
	}

	if (distance <= 16) {
		match->sdist_code = distance - 6;
		return;
	}

	for (int ii = xsize - 16; ii <= xsize - 3; ++ii) {
		if (distance == ii) {
			match->sdist_code = 11 + distance - (xsize - 16);
			return;
		}
	}

	for (int ii = xsize + 3; ii <= xsize + 16; ++ii) {
		if (distance == ii) {
			match->sdist_code = 25 + distance - (xsize + 3);
			return;
		}
	}

	u32 sdist = 39;
	for (int y = 2; y <= 8; ++y) {
		int ii = y * xsize - 8;
		for (int iiend = ii + 16; ii <= iiend; ++ii, ++sdist) {
			if (distance == ii) {
				match->sdist_code = sdist;
				return;
			}
		}
	}

	CAT_DEBUG_ENFORCE(sdist == 158);

	// It is a long distance, so increment the escape code

	match->escape_code += 9;
	match->emit_sdist = false;
	match->emit_ldist = true;

	// Encode distance directly:

	CAT_DEBUG_ENFORCE(distance >= 17);
	u32 D = distance - 17;
	int EB = BSR32((D >> 5) + 1) + 1;
	CAT_DEBUG_ENFORCE(EB <= 16);
	u32 C0 = ((1 << (EB - 1)) - 1) << 5;
	u32 Code = ((EB - 1) << 4) + ((D - C0) >> EB);
	CAT_DEBUG_ENFORCE(Code <= 255);
	match->ldist_code = static_cast<u16>( Code );

	u32 extra = (D - C0) & ((1 << EB) - 1);

	match->extra = extra;
	match->extra_bits = EB;

#ifdef CAT_DEBUG
	// Verify that the encoding is reversible
	{
		u32 dist_code = match->ldist_code;
		u32 EBT = (dist_code >> 4) + 1;
		CAT_DEBUG_ENFORCE(EBT == EB);
		u32 C0T = ((1 << (EB - 1)) - 1) << 5;
		CAT_DEBUG_ENFORCE(C0T == C0);
		u32 DT = ((dist_code - ((EB - 1) << 4)) << EB) + C0;
		if (match->extra_bits) {
			DT += match->extra;
		}
		CAT_DEBUG_ENFORCE(DT == D);
	}
#endif
}


//// LZMatchFinder Common Routines

void LZMatchFinder::init(Parameters &params) {
	CAT_DEBUG_ENFORCE(MIN_MATCH == 2);
	CAT_DEBUG_ENFORCE(MAX_MATCH == 256);

	// Store parameters
	_params = params;
	_pixels = _params.xsize * _params.ysize;
	_match_head = 0;

	// Initialize bitmask
	const int MASK_SIZE = (_pixels + 31) / 32;
	_mask.resizeZero(MASK_SIZE);
}

void LZMatchFinder::rejectMatches() {
	const u8 *costs = _params.costs;

	// Collect LZ distance symbol statistics
	FreqHistogram len_hist, sdist_hist, ldist_hist, escape_hist;
	escape_hist.init(_params.num_syms + ESCAPE_SYMS);
	len_hist.init(LEN_SYMS);
	sdist_hist.init(SDIST_SYMS);
	ldist_hist.init(LDIST_SYMS);

	// Calculate active pixels
	int active_pixels = 0;
	for (int ii = 0; ii < _pixels; ++ii) {
		const u8 cost = costs[ii];
		if (cost != 0) {
			++active_pixels;
		}
	}

	// Set up escape prices
	escape_hist.addMore(0, active_pixels);

	// Track recent distances
	u32 recent[LAST_COUNT];
	CAT_OBJCLR(recent);
	int recent_ii = 0;

	// For each match,
	for (int jj = 0; jj < _matches.size(); ++jj) {
		LZMatch * CAT_RESTRICT match = &_matches[jj];

		MatchEncode(recent, recent_ii, match, _params.xsize);

		// Fix escape code to start right after num_syms
		CAT_DEBUG_ENFORCE(match->escape_code < ESCAPE_SYMS);
		match->escape_code += _params.num_syms;
		escape_hist.add(match->escape_code);

		if (match->emit_len) {
			len_hist.add(match->len_code);
		}

		if (match->emit_sdist) {
			sdist_hist.add(match->sdist_code);
		} else {
			if (match->emit_ldist) {
				CAT_DEBUG_ENFORCE(!match->emit_sdist);
				ldist_hist.add(match->ldist_code);
			}
		}

		// Update recent distances
		recent[recent_ii++] = match->distance;
		if (recent_ii >= LAST_COUNT) {
			recent_ii = 0;
		}
	}

	// Estimate encoding costs for escape symbols
	HuffmanEncoder escape_encoder;
	escape_encoder.init(escape_hist);
	const u8 * CAT_RESTRICT escape_codelens = escape_encoder._codelens.get();

	// Initialize encoders
	_lz_len_encoder.init(len_hist);
	_lz_sdist_encoder.init(sdist_hist);
	_lz_ldist_encoder.init(ldist_hist);

	// Zero stats counters
	int rejects = 0, accepts = 0;

	// Reset histograms
	len_hist.zero();
	sdist_hist.zero();
	ldist_hist.zero();

	// Reset recent distances
	CAT_OBJCLR(recent);
	recent_ii = 0;

	// Zero linked list workspace
	LZMatch *prev = 0;

	// For each match,
	for (int jj = 0; jj < _matches.size(); ++jj) {
		LZMatch * CAT_RESTRICT match = &_matches[jj];

		// Re-encode the match since the recent distances have changed
		MatchEncode(recent, recent_ii, match, _params.xsize);

		// Estimate bit cost of LZ match representation:

		int bits = escape_codelens[match->escape_code] + match->extra_bits;

		if (match->emit_len) {
			bits += _lz_len_encoder.simulateWrite(match->len_code);
		}

		if (match->emit_sdist) {
			bits += _lz_sdist_encoder.simulateWrite(match->sdist_code);
		} else if (match->emit_ldist) {
			bits += _lz_ldist_encoder.simulateWrite(match->ldist_code);
		}

		// Verify it saves more than it costs:

		if (match->saved < bits) {
			++rejects;
		} else {
			++accepts;

			// Update recent distances
			recent[recent_ii++] = match->distance;
			if (recent_ii >= LAST_COUNT) {
				recent_ii = 0;
			}

			// Continue linked list
			if (prev) {
				prev->next = match;
			} else {
				_match_head = match;
			}
			prev = match;

			// Set up mask
			for (int jj = 0; jj < match->length; ++jj) {
				setMask(jj + match->offset);
			}

			if (match->emit_len) {
				len_hist.add(match->len_code);
			}

			if (match->emit_sdist) {
				sdist_hist.add(match->sdist_code);
			} else if (match->emit_ldist) {
				ldist_hist.add(match->ldist_code);
			}
		}
	}

	// Initialize Huffman encoders
	_lz_len_encoder.init(len_hist);
	_lz_sdist_encoder.init(sdist_hist);
	_lz_ldist_encoder.init(ldist_hist);

	CAT_INANE("Mono") << "Accepted " << accepts << " LZ matches. Rejected " << rejects;
}

int LZMatchFinder::scoreMatch(int distance, const u32 * CAT_RESTRICT recent, const u8 * CAT_RESTRICT costs, int &match_len, int &bits_saved) {
	bits_saved = 0;

	// Calculate bits saved and fix match length
	int fix_len = 0;
	for (int jj = 0; jj < match_len; ++jj) {
		int saved = costs[jj];

		if (saved > 0) {
			fix_len = jj + 1;
			bits_saved += saved;
		}
	}
	match_len = fix_len;

	// If minimum match length found,
	if (match_len < MIN_MATCH) {
		return 0;
	}

	// Add cost based on distance encoding
	int bits_cost;
	if (distance == recent[0]) {
		bits_cost = 6;
	} else if (distance == recent[1]) {
		bits_cost = 7;
	} else if (distance == recent[2]) {
		bits_cost = 7;
	} else if (distance == recent[3]) {
		bits_cost = 8;
	} else if (distance <= 6) {
		bits_cost = 8;
	} else if (distance >= _params.xsize*8 + 9) {
		bits_cost = 15 + BSR32(distance) - 4;
	} else if (distance >= _params.xsize*2 + 8) {
		bits_cost = 13;
	} else {
		bits_cost = 12;
	}

	return bits_saved - bits_cost;
}

void LZMatchFinder::train(LZMatch * CAT_RESTRICT match, EntropyEncoder &ee) {
	ee.add(match->escape_code);
}

int LZMatchFinder::writeTables(ImageWriter &writer) {
	int bits = 0;

	bits += _lz_len_encoder.writeTable(writer);
	bits += _lz_sdist_encoder.writeTable(writer);
	bits += _lz_ldist_encoder.writeTable(writer);

	return bits;
}

int LZMatchFinder::write(LZMatch * CAT_RESTRICT match, EntropyEncoder &ee, ImageWriter &writer) {
	int ee_bits = ee.write(match->escape_code, writer);

	int len_bits = 0, dist_bits = 0;

	if (match->emit_len) {
		len_bits += _lz_len_encoder.writeSymbol(match->len_code, writer);
	}

	if (match->emit_sdist) {
		dist_bits += _lz_sdist_encoder.writeSymbol(match->sdist_code, writer);
	}

	if (match->emit_ldist) {
		dist_bits += _lz_ldist_encoder.writeSymbol(match->ldist_code, writer);
	}

	if (match->extra_bits) {
		writer.writeBits(match->extra, match->extra_bits);
	}

	int bits = ee_bits + len_bits + dist_bits + match->extra_bits;

#ifdef CAT_DUMP_LZ
	if (match->length < 5) {
		CAT_WARN("Mono") << "ee=" << ee_bits << " len=" << len_bits << " dist=" << dist_bits << " extra=" << match->extra_bits << " : sum=" << bits << " LDIST=" << match->emit_ldist;
	}
#endif

	return bits;
}


//// RGBAMatchFinder

bool RGBAMatchFinder::findMatches(SuffixArray3_State * CAT_RESTRICT sa3state, const u32 * CAT_RESTRICT rgba) {
	// Allocate and zero the table and chain
	SmartArray<u32> table, chain;
	table.resizeZero(HASH_SIZE);
	chain.resizeZero(_pixels);

	// Clear mask
	const int mask_size = (_pixels + 31) / 32;
	_mask.resizeZero(mask_size);

	// Track recent distances
	u32 recent[LAST_COUNT];
	CAT_OBJCLR(recent);
	int recent_ii = 0;

	// Track number of pixels covered by previous matches as we walk
	int covered_pixels = 0;
	const int CHAIN_LIMIT = _params.chain_limit;

	// For each pixel, stopping just before the last pixel:
	const u32 * CAT_RESTRICT rgba_now = rgba;
	const u8 * CAT_RESTRICT costs = _params.costs;
	for (int ii = 0, iiend = _pixels - MIN_MATCH; ii <= iiend; ++ii, ++rgba_now, ++costs) {
		u16 best_length = MIN_MATCH - 1;
		u32 best_distance = 0;
		int best_score = 0, best_saved = 0;

		// Get min match hash
		const u32 hash = HashPixels(rgba_now);

		// If not masked,
		if (costs[0] > 0) {
			u32 node = table[hash];

			// If any matches exist,
			if (node != 0) {
				// Find longest match
				int longest_off_n, longest_off_p;
				int longest_ml_n, longest_ml_p;
				SuffixArray3_BestML(sa3state, ii << 2, longest_off_n, longest_off_p, longest_ml_n, longest_ml_p);

				// Round lengths down to next RGBA pixel
				if (longest_off_n & 3) {
					longest_ml_n -= 4 - (longest_off_n & 3);
				}
				longest_ml_n >>= 2;
				if (longest_off_p & 3) {
					longest_ml_p -= 4 - (longest_off_p & 3);
				}
				longest_ml_p >>= 2;

				// Round offsets up to next RGBA pixel
				longest_off_n = (longest_off_n + 3) >> 2;
				longest_off_p = (longest_off_p + 3) >> 2;

				// If longest match exists,
				if (longest_ml_n >= MIN_MATCH ||
					longest_ml_p >= MIN_MATCH) {
					// For each hash chain suggested start point,
					int limit = CHAIN_LIMIT; // up to a limited depth
					do {
						--node;

						// If distance is beyond the window size,
						u32 distance = ii - node;
						if (distance > WIN_SIZE) {
							// Stop searching here
							break;
						}

						// Unroll first two
						const u32 *rgba_node = rgba + node;
						if (rgba_node[0] == rgba_now[0] &&
							rgba_node[1] == rgba_now[1]) {
							// Find match length
							int match_len = 2;
							for (; match_len < MAX_MATCH && rgba_node[match_len] == rgba_now[match_len]; ++match_len);

							// Score match
							int bits_saved;
							int score = scoreMatch(distance, recent, costs, match_len, bits_saved);

							// If score is an improvement,
							if (match_len >= MIN_MATCH) {
								if (score > best_score || best_distance == 0) {
									best_distance = distance;
									best_length = match_len;
									best_score = score;
									best_saved = bits_saved;
								}
							}
						}

						// Next node
						node = chain[node];
					} while (node != 0 && --limit);

					// Calculate distance to it
					u32 longest_dist_n = ii - longest_off_n;
					u32 longest_dist_p = ii - longest_off_p;

					// If match length is too long,
					if (longest_ml_n > MAX_MATCH) {
						longest_ml_n = MAX_MATCH;
					}
					if (longest_ml_p > MAX_MATCH) {
						longest_ml_p = MAX_MATCH;
					}

					// Score match based on residuals
					int longest_saved_n, longest_saved_p;
					int longest_score_n, longest_score_p;

					if (longest_off_n < ii && longest_ml_n >= MIN_MATCH) {
						longest_score_n = scoreMatch(longest_dist_n, recent, costs, longest_ml_n, longest_saved_n);
						if (longest_score_n > best_score) {
							best_distance = longest_dist_n;
							best_length = longest_ml_n;
							best_score = longest_score_n;
							best_saved = longest_saved_n;
						}
					}
					if (longest_off_p < ii && longest_ml_p >= MIN_MATCH) {
						longest_score_p = scoreMatch(longest_dist_p, recent, costs, longest_ml_p, longest_saved_p);
						if (longest_score_p > best_score) {
							best_distance = longest_dist_p;
							best_length = longest_ml_p;
							best_score = longest_score_p;
							best_saved = longest_saved_p;
						}
					}

					if (best_score < 0) {
						best_distance = 0;
					}
				}
			}
		}

		// Insert current pixel to end of hash chain
		chain[ii] = table[hash] + 1;
		table[hash] = ii;

		// If a best node was found,
		if (best_distance > 0) {
			// Check if this match covers more pixels than the overlapping match
			if (best_length > covered_pixels) {
				if (covered_pixels <= 0) {
					// Update recent distances
					recent[recent_ii++] = best_distance;
					if (recent_ii >= LAST_COUNT) {
						recent_ii = 0;
					}

					_matches.push_back(LZMatch(ii, best_distance, best_length, best_saved));
					covered_pixels = best_length;
				} else {
					//CAT_WARN("LZextended") << ii << " : " << best_distance << ", " << best_length;
				}
			}
		}

		// Uncover one pixel
		if (covered_pixels > 0) {
			--covered_pixels;
		}
	}

	return true;
}

bool RGBAMatchFinder::init(const u32 * CAT_RESTRICT rgba, Parameters &params) {
	LZMatchFinder::init(params);

	SuffixArray3_State sa3state;
	SuffixArray3_Init(&sa3state, (u8*)rgba, _pixels * 4, (WIN_SIZE > _pixels ? _pixels : WIN_SIZE) * 4);

	if (!findMatches(&sa3state, rgba)) {
		return false;
	}

#ifdef CAT_DEBUG
	for (int ii = 0; ii < _matches.size(); ++ii) {
		int off = _matches[ii].offset;
		int len = _matches[ii].length;
		int dist = _matches[ii].distance;

		for (int jj = 0; jj < len; ++jj) {
			CAT_DEBUG_ENFORCE(rgba[off + jj] == rgba[off - dist + jj]);
		}
	}
#endif

	rejectMatches();

	return true;
}


//// MonoMatchFinder

bool MonoMatchFinder::findMatches(SuffixArray3_State * CAT_RESTRICT sa3state, const u8 * CAT_RESTRICT mono) {
	// Allocate and zero the table and chain
	SmartArray<u32> table, chain;
	table.resizeZero(HASH_SIZE);
	chain.resizeZero(_pixels);

	// Track recent distances
	u32 recent[LAST_COUNT];
	CAT_OBJCLR(recent);
	int recent_ii = 0;

	// Track number of pixels covered by previous matches as we walk
	int covered_pixels = 0;
	const int CHAIN_LIMIT = _params.chain_limit;

	// For each pixel, stopping just before the last pixel:
	const u8 * CAT_RESTRICT mono_now = mono;
	const u8 * CAT_RESTRICT costs = _params.costs;
	for (int ii = 0, iiend = _pixels - MIN_MATCH; ii <= iiend; ++ii, ++mono_now, ++costs) {
		u16 best_length = MIN_MATCH - 1;
		u32 best_distance = 0;
		int best_score = 0, best_saved = 0;

		// Get min match hash
		const u32 hash = HashPixels(mono_now);

		// If not masked,
		if (costs[0] > 0) {
			u32 node = table[hash];

			// If any matches exist,
			if (node != 0) {
				// Find longest match
				int longest_off_n, longest_off_p;
				int longest_ml_n, longest_ml_p;
				SuffixArray3_BestML(sa3state, ii, longest_off_n, longest_off_p, longest_ml_n, longest_ml_p);

				CAT_DEBUG_ENFORCE(longest_off_n < ii && longest_off_p < ii);

				// If longest match exists,
				if (longest_ml_n >= MIN_MATCH ||
					longest_ml_p >= MIN_MATCH) {
					// For each hash chain suggested start point,
					int limit = CHAIN_LIMIT; // up to a limited depth
					do {
						--node;

						// If distance is beyond the window size,
						u32 distance = ii - node;
						if (distance > WIN_SIZE) {
							// Stop searching here
							break;
						}

						// Unroll first two
						const u8 *mono_node = mono + node;
						if (mono_node[0] == mono_now[0] &&
							mono_node[1] == mono_now[1]) {
							// Find match length
							int match_len = 2;
							for (; match_len < MAX_MATCH && mono_node[match_len] == mono_now[match_len]; ++match_len);

							// Score match
							int bits_saved;
							int score = scoreMatch(distance, recent, costs, match_len, bits_saved);

							// If score is an improvement,
							if (match_len >= MIN_MATCH) {
								if (score > best_score || best_distance == 0) {
									best_distance = distance;
									best_length = match_len;
									best_score = score;
									best_saved = bits_saved;
								}
							}
						}

						// Next node
						node = chain[node];
					} while (node != 0 && --limit);

					// Calculate distance to it
					u32 longest_dist_n = ii - longest_off_n;
					u32 longest_dist_p = ii - longest_off_p;

					// If match length is too long,
					if (longest_ml_n > MAX_MATCH) {
						longest_ml_n = MAX_MATCH;
					}
					if (longest_ml_p > MAX_MATCH) {
						longest_ml_p = MAX_MATCH;
					}


					// Score match based on residuals
					int longest_saved_n, longest_saved_p;
					int longest_score_n, longest_score_p;

					if (longest_off_n < ii && longest_ml_n >= MIN_MATCH) {
						longest_score_n = scoreMatch(longest_dist_n, recent, costs, longest_ml_n, longest_saved_n);
						if (longest_score_n > best_score) {
							best_distance = longest_dist_n;
							best_length = longest_ml_n;
							best_score = longest_score_n;
							best_saved = longest_saved_n;
						}
					}
					if (longest_off_p < ii && longest_ml_p >= MIN_MATCH) {
						longest_score_p = scoreMatch(longest_dist_p, recent, costs, longest_ml_p, longest_saved_p);
						if (longest_score_p > best_score) {
							best_distance = longest_dist_p;
							best_length = longest_ml_p;
							best_score = longest_score_p;
							best_saved = longest_saved_p;
						}
					}


					if (best_score < 2) {
						best_distance = 0;
					}
				}
			}
		}

		// Insert current pixel to end of hash chain
		chain[ii] = table[hash] + 1;
		table[hash] = ii;

		// If a best node was found,
		if (best_distance > 0) {
			// Check if this match covers more pixels than the overlapping match
			if (best_length > covered_pixels) {
				if (covered_pixels <= 0) {
					// Update recent distances
					recent[recent_ii++] = best_distance;
					if (recent_ii >= LAST_COUNT) {
						recent_ii = 0;
					}

					_matches.push_back(LZMatch(ii, best_distance, best_length, best_saved));
					covered_pixels = best_length;
					//CAT_WARN("LZ") << ii << " : " << best_distance << ", " << best_length;
				} else {
					//CAT_WARN("LZextended") << ii << " : " << best_distance << ", " << best_length;
				}
			}
		}

		// Uncover one pixel
		if (covered_pixels > 0) {
			--covered_pixels;
		}
	}

	return true;
}

bool MonoMatchFinder::init(const u8 * CAT_RESTRICT mono, Parameters &params) {
	LZMatchFinder::init(params);

	SuffixArray3_State sa3state;
	SuffixArray3_Init(&sa3state, (u8*)mono, _pixels, (WIN_SIZE > _pixels ? _pixels : WIN_SIZE));

	if (!findMatches(&sa3state, mono)) {
		return false;
	}

#ifdef CAT_DEBUG
	for (int ii = 0; ii < _matches.size(); ++ii) {
		int off = _matches[ii].offset;
		int len = _matches[ii].length;
		int dist = _matches[ii].distance;

		for (int jj = 0; jj < len; ++jj) {
			CAT_DEBUG_ENFORCE(off >= dist);
			CAT_DEBUG_ENFORCE(mono[off + jj] == mono[off - dist + jj]);
		}
	}
#endif

	rejectMatches();

	return true;
}
