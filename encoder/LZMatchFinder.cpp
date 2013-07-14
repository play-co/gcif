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


//// RGBAMatchFinder

bool RGBAMatchFinder::findMatches(const u32 *rgba, const u8 * CAT_RESTRICT residuals, int xsize, int ysize, ImageMaskWriter *image_mask) {
	// Setup mask
	const u32 mask_color = image_mask->getColor();
	const bool using_mask = image_mask->enabled();

	// Allocate and zero the table and chain
	const int pixels = xsize * ysize;
	SmartArray<u32> table, chain;
	table.resizeZero(HASH_SIZE);
	chain.resizeZero(pixels);

	// Clear mask
	_xsize = xsize;
	const int mask_size = (xsize * ysize + 31) / 32;
	_mask.resizeZero(mask_size);

	// Track recent distances
	u32 recent[LAST_COUNT];
	CAT_OBJCLR(recent);
	int recent_ii = 0;

	// For each pixel, stopping just before the last pixel:
	const u32 *rgba_now = rgba;
	u16 x = 0, y = 0;
	for (int ii = 0, iiend = pixels - MIN_MATCH; ii <= iiend;) {
		const u32 hash = HashPixels(rgba_now);
		u16 best_length = 1;
		u32 best_distance = 0;
		int best_score = 0;

		// If not masked,
		if (!using_mask || !image_mask->masked(x, y)) {
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

				u32 base_color = rgba_now[0];
				if (rgba_node[0] == base_color) {
					// Find match length
					int match_len = 1;
					for (; match_len < MAX_MATCH && rgba_node[match_len] == rgba_now[match_len]; ++match_len);

					// Future matches will be farther away (more expensive in distance)
					// so they should be at least as long as previous matches to be considered
					if (match_len > best_length) {
						const u8 * CAT_RESTRICT saved = residuals;
						int bitsSaved = 0;

						if (using_mask) {
							int fix_len = 0;
							for (int jj = 0; jj < match_len; ++jj, saved += 4) {
								if (rgba_now[jj] != mask_color) {
									fix_len = jj + 1;
									bitsSaved += saved[0];
								}
							}
							match_len = fix_len;
						} else {
							for (int jj = 0; jj < match_len; ++jj, saved += 4) {
								bitsSaved += saved[0];
							}
						}

						if (match_len >= 2) {
							int bitsCost = 0;
							if (distance == recent[0]) {
								bitsCost = 7;
							} else if (distance == recent[1]) {
								bitsCost = 7;
							} else if (distance == recent[2]) {
								bitsCost = 7;
							} else if (distance == recent[3]) {
								bitsCost = 7;
							} else if (distance >= _xsize*8 + 9) {
								bitsCost = 22;
							} else if (distance >= _xsize*2 + 8) {
								bitsCost = 12;
							} else {
								bitsCost = 10;
							}

							const int score = bitsSaved - bitsCost;
							if (score > best_score) {
								best_distance = distance;
								best_length = match_len;
								best_score = score;

								// If length is at the limit,
								if (match_len >= MAX_MATCH) {
									// Stop here
									break;
								}
							}
						}
					}
				}
			}
		}

		// Insert current pixel
		chain[ii] = table[hash] + 1;
		table[hash] = ++ii;
		++rgba_now;
		++x;
		residuals += 4;

		// If a best node was found,
		if (best_distance > 0) {
			const int offset = ii - 1;

			// Update recent distances
			recent[recent_ii++] = best_distance;
			if (recent_ii >= LAST_COUNT) {
				recent_ii = 0;
			}

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

			// Skip ahead
			--best_length;
			x += best_length;
			residuals += best_length << 2;
		}

		while (x >= xsize) {
			x -= xsize;
			++y;
		}
	}

	_matches.push_back(LZMatch(GUARD_OFFSET, 0, 0));

	return true;
}

static void RGBAEncode(u32 *recent, int recent_ii, LZMatchFinder::LZMatch *match, int xsize) {
	static const u16 SYM0 = ImageRGBAReader::NUM_LIT_SYMS;
	static const int LAST_COUNT = RGBAMatchFinder::LAST_COUNT;

	match->emit_sdist = false;
	match->emit_ldist = false;
	match->emit_dist1 = false;
	match->emit_dist2 = false;
	match->emit_dist3 = false;
	match->extra_bits = 0;

	const u32 distance = match->distance;
	const u16 length = match->length;

	CAT_DEBUG_ENFORCE(distance > 0);
	CAT_DEBUG_ENFORCE(length >= 2);
	CAT_DEBUG_ENFORCE(length <= 256);
	CAT_DEBUG_ENFORCE(RGBAMatchFinder::MIN_MATCH == 2);
	CAT_DEBUG_ENFORCE(RGBAMatchFinder::MAX_MATCH == 256);

	// Try to represent the distance in the escape code:

	match->emit_len = true;
	match->len_code = length - 2;

	for (int ii = 0; ii < LAST_COUNT; ++ii) {
		if (distance == recent[(recent_ii + LAST_COUNT - 1 - ii) % LAST_COUNT]) {
			match->escape_code = SYM0 + ii;
			return;
		}
	}

	if (distance == 1) {
		match->escape_code = SYM0 + 4;
		return;
	}

	if (distance >= 3 && distance <= 6) {
		match->escape_code = SYM0 + distance + 2;
		return;
	}

	int start = xsize - 2;
	if (distance >= start && distance <= start + 4) {
		match->escape_code = SYM0 + distance - start + 9;
		return;
	}

	CAT_DEBUG_ENFORCE(distance == 2 || distance >= 7);

	if (length <= 9) {
		match->emit_len = false;
		match->escape_code = SYM0 + 14 + length - 2;
	} else {
		match->emit_len = true;
		match->escape_code = SYM0 + 22;
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
	CAT_DEBUG_ENFORCE(EB <= 18);
	u32 C0 = ((1 << (EB - 1)) - 1) << 5;
	u32 Code = ((EB - 1) << 4) + ((D - C0) >> EB);
	CAT_DEBUG_ENFORCE(Code <= 288);
	match->ldist_code = static_cast<u16>( Code );

	u32 extra = (D - C0) & ((1 << EB) - 1);

	if (EB <= 7) {
		CAT_DEBUG_ENFORCE(extra <= 127);
		match->extra_bits = EB;
		match->extra = extra;
	} else if (EB <= 8 + 7) {
		match->extra_bits = EB - 8;
		match->extra = extra & ~(0xffffffff << (EB - 8));
		match->emit_dist3 = true;
		match->dist3_code = static_cast<u8>( extra >> (EB - 8) );
	} else {
		match->extra_bits = EB - 16;
		match->extra = extra & ~(0xffffffff << (EB - 16));
		match->emit_dist1 = true;
		match->dist1_code = static_cast<u8>( extra >> (EB - 16) );
		match->emit_dist2 = true;
		match->dist2_code = static_cast<u8>( extra >> (EB - 8) );
	}

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
		if (match->emit_dist3) {
			DT += match->dist3_code << match->extra_bits;
		}
		if (match->emit_dist1) {
			DT += match->dist1_code << match->extra_bits;
		}
		if (match->emit_dist2) {
			DT += match->dist2_code << (match->extra_bits + 8);
		}
		CAT_DEBUG_ENFORCE(DT == D);
	}
#endif
}

bool RGBAMatchFinder::init(const u32 * CAT_RESTRICT rgba, const u8 * CAT_RESTRICT residuals, int xsize, int ysize, ImageMaskWriter *mask) {
	CAT_DEBUG_ENFORCE(MIN_MATCH == 2);
	CAT_DEBUG_ENFORCE(MAX_MATCH == 256);

	_xsize = xsize;

	if (!findMatches(rgba, residuals, xsize, ysize, mask)) {
		return false;
	}

	reset();

	// Collect LZ distance symbol statistics
	FreqHistogram len_hist, sdist_hist, ldist_hist, dist1_hist, dist2_hist, dist3_hist;
	len_hist.init(ImageRGBAReader::LZ_LEN_SYMS);
	sdist_hist.init(ImageRGBAReader::LZ_SDIST_SYMS);
	ldist_hist.init(ImageRGBAReader::LZ_LDIST_SYMS);
	dist1_hist.init(ImageRGBAReader::LZ_DIST1_SYMS);
	dist2_hist.init(ImageRGBAReader::LZ_DIST2_SYMS);
	dist3_hist.init(ImageRGBAReader::LZ_DIST3_SYMS);

	// Track recent distances
	u32 recent[LAST_COUNT];
	CAT_OBJCLR(recent);
	int recent_ii = 0;

	// While not at the end of the match list,
	while (peekOffset() != LZMatchFinder::GUARD_OFFSET) {
		LZMatch *match = pop();

		RGBAEncode(recent, recent_ii, match, _xsize);

		CAT_DEBUG_ENFORCE(match->escape_code >= ImageRGBAReader::NUM_LIT_SYMS);
		CAT_DEBUG_ENFORCE(match->escape_code < ImageRGBAReader::NUM_Y_SYMS);

		if (match->emit_len) {
			len_hist.add(match->len_code);
		}

		if (match->emit_sdist) {
			CAT_DEBUG_ENFORCE(!match->emit_ldist && !match->emit_dist1 && !match->emit_dist2 && !match->emit_dist3);
			sdist_hist.add(match->sdist_code);
		} else {
			if (match->emit_ldist) {
				CAT_DEBUG_ENFORCE(!match->emit_sdist);
				ldist_hist.add(match->ldist_code);
			}

			if (match->emit_dist3) {
				CAT_DEBUG_ENFORCE(!match->emit_dist1 && !match->emit_dist2);
				dist3_hist.add(match->dist3_code);
			} else {
				if (match->emit_dist1) {
					CAT_DEBUG_ENFORCE(!match->emit_dist3 && match->emit_dist2);
					dist1_hist.add(match->dist1_code);
				}

				if (match->emit_dist2) {
					CAT_DEBUG_ENFORCE(!match->emit_dist3 && match->emit_dist1);
					dist2_hist.add(match->dist2_code);
				}
			}
		}

		// Update recent distances
		recent[recent_ii++] = match->distance;
		if (recent_ii >= LAST_COUNT) {
			recent_ii = 0;
		}
	}

	// Initialize encoders
	_lz_len_encoder.init(len_hist);
	_lz_sdist_encoder.init(sdist_hist);
	_lz_ldist_encoder.init(ldist_hist);
	_lz_dist1_encoder.init(dist1_hist);
	_lz_dist2_encoder.init(dist2_hist);
	_lz_dist3_encoder.init(dist3_hist);

	return true;
}

void RGBAMatchFinder::train(EntropyEncoder &ee) {
	// Get LZ match information
	LZMatch *match = pop();

	ee.add(match->escape_code);
}

int RGBAMatchFinder::writeTables(ImageWriter &writer) {
	int bits = 0;

	bits += _lz_len_encoder.writeTable(writer);
	bits += _lz_sdist_encoder.writeTable(writer);
	bits += _lz_ldist_encoder.writeTable(writer);
	bits += _lz_dist1_encoder.writeTable(writer);
	bits += _lz_dist2_encoder.writeTable(writer);
	bits += _lz_dist3_encoder.writeTable(writer);

	return bits;
}

int RGBAMatchFinder::write(EntropyEncoder &ee, ImageWriter &writer) {
	// Get LZ match information
	LZMatch *match = pop();

	int ee_bits = ee.write(match->escape_code, writer);

	int len_bits = 0, dist_bits = 0, dist1_bits = 0, dist2_bits = 0, dist3_bits = 0;

	if (match->emit_len) {
		len_bits += _lz_len_encoder.writeSymbol(match->len_code, writer);
	}

	if (match->emit_sdist) {
		dist_bits += _lz_sdist_encoder.writeSymbol(match->sdist_code, writer);
	}

	if (match->emit_ldist) {
		dist_bits += _lz_ldist_encoder.writeSymbol(match->ldist_code, writer);
	}

	if (match->emit_dist1) {
		dist1_bits += _lz_dist1_encoder.writeSymbol(match->dist1_code, writer);
	}

	if (match->emit_dist2) {
		dist2_bits += _lz_dist2_encoder.writeSymbol(match->dist2_code, writer);
	}

	if (match->emit_dist3) {
		dist3_bits += _lz_dist3_encoder.writeSymbol(match->dist3_code, writer);
	}

	int extra_bits = match->extra_bits;
	if (match->extra_bits > 0) {
		writer.writeBits(match->extra, match->extra_bits);
	}

	int bits = ee_bits + len_bits + dist_bits + dist1_bits + dist2_bits + dist3_bits + extra_bits;

#ifdef CAT_DUMP_LZ
	if (match->length < 5) {
		CAT_WARN("EMIT") << "ee=" << ee_bits << " len=" << len_bits << " dist=" << dist_bits << " dist1=" << dist1_bits << " dist2=" << dist2_bits << " dist3=" << dist3_bits << " extra=" << extra_bits << " : sum=" << bits;
	}
#endif

	return bits;
}


//// MonoMatchFinder

bool MonoMatchFinder::findMatches(const u8 *mono, int xsize, int ysize, MonoMatchFinder::MaskDelegate image_mask, const u8 mask_color) {
	// Setup mask
	const bool using_mask = image_mask.IsValid();

	// Allocate and zero the table and chain
	const int pixels = xsize * ysize;
	SmartArray<u32> table, chain;
	table.resizeZero(HASH_SIZE);
	chain.resizeZero(pixels);

	// Clear mask
	_xsize = xsize;
	const int mask_size = (xsize * ysize + 31) / 32;
	_mask.resizeZero(mask_size);

	// For each pixel, stopping just before the last pixel:
	const u8 *mono_now = mono;
	u16 x = 0, y = 0;
	for (int ii = 0, iiend = pixels - MIN_MATCH; ii <= iiend;) {
		const u32 hash = HashPixels(mono_now);
		u16 best_length = MIN_MATCH - 1;
		u32 best_distance = 0;

		// If not masked,
		if (!using_mask || !image_mask(x, y)) {
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
				for (; match_len < MAX_MATCH && mono_node[match_len] == mono_now[match_len]; ++match_len);

				// Future matches will be farther away (more expensive in distance)
				// so they should be at least as long as previous matches to be considered
				if (match_len > best_length) {
					if (using_mask) {
						int fix_len = 0;
						for (int jj = 0; jj < match_len; ++jj) {
							if (mono_now[jj] == mask_color) {
								int off = ii + jj, sy = off / xsize, sx = off % xsize;
								if (!image_mask(sx, sy)) {
									fix_len = jj + 1;
								}
							} else {
								fix_len = jj + 1;
							}
						}
						match_len = fix_len;
					}

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
				//CAT_WARN("MONOTEST") << offset << " : " << best_distance << ", " << best_length;

				mask(offset);

				// Insert matched pixels
				for (int jj = 1; jj < best_length; ++jj) {
					mask(ii);
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

bool MonoMatchFinder::init(const u8 * CAT_RESTRICT mono, int xsize, int ysize, MonoMatchFinder::MaskDelegate mask) {
	_xsize = xsize;

	if (!findMatches(mono, xsize, ysize, mask, 0)) {
		return false;
	}

	reset();
/*
	// Collect LZ distance symbol statistics
	FreqHistogram lz_dist_hist;
	lz_dist_hist.init(ImageRGBAReader::LZ_DIST_SYMS);

	// While not at the end of the match list,
	while (peekOffset() != LZMatchFinder::GUARD_OFFSET) {
		LZMatch *match = pop();

		// Record symbol instance
		lz_dist_hist.add(match->dist_code);
	}

	// Initialize the LZ distance encoder
	_lz_dist_encoder.init(lz_dist_hist);
*/
	return true;
}

int MonoMatchFinder::writeTables(ImageWriter &writer) {
	return _lz_dist_encoder.writeTable(writer);
}

int MonoMatchFinder::write(int num_syms, EntropyEncoder &ee, ImageWriter &writer) {
	int bits = 0;

	// Get LZ match information
	//LZMatch *match = pop();
/*
	// Write length code
	bits += ee.write(num_syms + match->len_code, writer);

	// Write distance code
	bits += _lz_dist_encoder.writeSymbol(match->dist_code, writer);
*/
	return bits;
}

