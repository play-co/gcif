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

#ifndef LZ_MATCH_FINDER_HPP
#define LZ_MATCH_FINDER_HPP

#include "../decoder/Platform.hpp"
#include "../decoder/SmartArray.hpp"
#include "ImageMaskWriter.hpp"
#include "EntropyEncoder.hpp"
#include "../decoder/ImageRGBAReader.hpp"
#include "../decoder/MonoReader.hpp"

#include <vector>

/*
 * LZ Match Finder
 */

namespace cat {


//// LZMatchFinder

class LZMatchFinder {
public:
	static const u32 GUARD_OFFSET = 0xffffffff;

	// Match list, with guard at end
	struct LZMatch {
		// Initial found match
		u32 offset, distance;
		u16 length;

		// Encoding
		u16 escape_code;		// Which escape code to emit in Y-channel
		bool emit_len;			// Emit length code?
		u16 len_code;			// Code for length
		bool emit_ldist;		// Emit long distance code?
		u16 ldist_code;			// Code for distance
		bool emit_sdist;		// Emit short distance code?
		u16 sdist_code;			// Code for distance
		bool emit_dist1;		// Emit extended distance code 1?
		u16 dist1_code;			// Extended distance codes
		bool emit_dist2;		// Emit extended distance code 2?
		u16 dist2_code;			// Extended distance codes
		bool emit_dist3;		// Emit extended distance code 3?
		u16 dist3_code;			// Extended distance codes
		u16 extra, extra_bits;

		CAT_INLINE LZMatch(u32 offset, u32 distance, u16 length) {
			this->offset = offset;
			this->distance = distance;
			this->length = length;
		}
	};

protected:
	std::vector<LZMatch> _matches;
	LZMatch *_next_match;

	SmartArray<u32> _mask;
	int _xsize;

	CAT_INLINE void mask(u32 off) {
		_mask[off >> 5] |= 1 << (off & 31);
	}

public:
	CAT_INLINE bool masked(u16 x, u16 y) {
		const int off = x + y * _xsize;
		return ( _mask[off >> 5] & (1 << (off & 31)) ) != 0;
	}

	CAT_INLINE int size() {
		return static_cast<int>( _matches.size() );
	}

	CAT_INLINE void reset() {
		_next_match = &_matches[0];
	}

	// Once the guard offset is hit, pops should be avoided
	CAT_INLINE u32 peekOffset() {
		return _next_match->offset;
	}

	CAT_INLINE LZMatch *pop() {
		CAT_DEBUG_ENFORCE(peekOffset() != GUARD_OFFSET);

		return _next_match++;
	}
};


//// RGBAMatchFinder

class RGBAMatchFinder : public LZMatchFinder {
protected:
	HuffmanEncoder _lz_len_encoder;
	HuffmanEncoder _lz_sdist_encoder;
	HuffmanEncoder _lz_ldist_encoder;
	HuffmanEncoder _lz_dist1_encoder; // For long literal distances 
	HuffmanEncoder _lz_dist2_encoder;
	HuffmanEncoder _lz_dist3_encoder;

	bool findMatches(const u32 * CAT_RESTRICT rgba, const u8 * CAT_RESTRICT residuals, int xsize, int ysize, ImageMaskWriter *mask);

public:
	// Not worth matching less than MIN_MATCH
	static const int MIN_MATCH = 2; // pixels
	static const int MAX_MATCH = 256; // pixels
	static const int WIN_SIZE = 1024 * 1024; // pixels

	static const int LAST_COUNT = 4; // Keep track of recently emitted distances

	static const int HASH_BITS = 18;
	static const int HASH_SIZE = 1 << HASH_BITS;
	static const u64 HASH_MULT = 0xc6a4a7935bd1e995ULL;

	// Returns hash for MIN_MATCH pixels
	static CAT_INLINE u32 HashPixels(const u32 * CAT_RESTRICT rgba) {
		return (u32)( ( ((u64)rgba[0] << 32) | rgba[1] ) * HASH_MULT >> (64 - HASH_BITS) );
	}

	bool init(const u32 * CAT_RESTRICT rgba, const u8 * CAT_RESTRICT residuals, int xsize, int ysize, ImageMaskWriter *mask);

	void train(EntropyEncoder &ee);

	int writeTables(ImageWriter &writer);
	int write(EntropyEncoder &ee, ImageWriter &writer);
};


//// MonoMatchFinder

class MonoMatchFinder : public LZMatchFinder {
public:
	// bool IsMasked(u16 x, u16 y)
	typedef Delegate2<bool, u16, u16> MaskDelegate;

	// Not worth matching less than MIN_MATCH
	static const int MIN_MATCH = 6; // pixels
	static const int MAX_MATCH = 256; // pixels
	static const int WIN_SIZE = 512 * 512; // pixels

protected:
	HuffmanEncoder _lz_dist_encoder;

	bool findMatches(const u8 * CAT_RESTRICT mono, int xsize, int ysize, MonoMatchFinder::MaskDelegate mask, const u8 mask_color);

	/*
	 * Encoding cost in bits for monochrome data:
	 */
	static const int DIST_PREFIX_COST = 7; // bits
	static const int LEN_PREFIX_COST = 5; // bits
	static const int SAVED_PIXEL_BITS = 1; // bits

	static const int HASH_BITS = 18;
	static const int HASH_SIZE = 1 << HASH_BITS;
	static const u64 HASH_MULT = 0xc6a4a7935bd1e995ULL;

	// Returns hash for MIN_MATCH pixels
	static CAT_INLINE u32 HashPixels(const u8 * CAT_RESTRICT mono) {
		const u32 word0 = *reinterpret_cast<const u32 *>( mono );

		u16 word1 = mono[4];
		word1 |= static_cast<u16>( mono[5] ) << 8;

		return (u32)( ( ((u64)word1 << 32) | word0 ) * HASH_MULT >> (64 - HASH_BITS) );
	}

public:
	bool init(const u8 * CAT_RESTRICT mono, int xsize, int ysize, MonoMatchFinder::MaskDelegate mask);

	int writeTables(ImageWriter &writer);
	int write(int num_syms, EntropyEncoder &ee, ImageWriter &writer);
};


} // namespace cat

#endif // LZ_MATCH_FINDER_HPP

