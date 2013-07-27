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
#include "SuffixArray3.hpp"

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
		u32 saved;

		// Encoding
		u16 escape_code;		// Which escape code to emit in Y-channel
		bool emit_len;			// Emit length code?
		u16 len_code;			// Code for length
		bool emit_ldist;		// Emit long distance code?
		u16 ldist_code;			// Code for distance
		bool emit_sdist;		// Emit short distance code?
		u16 sdist_code;			// Code for distance
		u16 extra, extra_bits;	// Extra raw bits to send
		bool accepted;			// Accepted to be transmitted

		CAT_INLINE LZMatch(u32 offset, u32 distance, u16 length, u32 saved) {
			this->offset = offset;
			this->distance = distance;
			this->length = length;
			this->saved = saved;
			this->accepted = true;
		}
	};

protected:
	std::vector<LZMatch> _matches;
	LZMatch *_next_match;

	SmartArray<u32> _mask;
	int _xsize;

	CAT_INLINE void setMask(u32 off) {
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

		while (!_next_match->accepted && _next_match->offset != GUARD_OFFSET) {
			++_next_match;
		}
	}

	// Once the guard offset is hit, pops should be avoided
	CAT_INLINE u32 peekOffset() {
		return _next_match->offset;
	}

	CAT_INLINE LZMatch *pop() {
		CAT_DEBUG_ENFORCE(peekOffset() != GUARD_OFFSET);

		LZMatch *match = _next_match;

		while (_next_match->offset != GUARD_OFFSET) {
			++_next_match;
			if (_next_match->accepted) break;
		}

		return match;
	}
};


//// RGBAMatchFinder

class RGBAMatchFinder : public LZMatchFinder {
	static const int HASH_BITS = 18;
	static const int HASH_SIZE = 1 << HASH_BITS;
	static const u64 HASH_MULT = 0xc6a4a7935bd1e995ULL;

	static const int ESCAPE_CODE_LOW_BOUND = 2; // bits

	// Returns hash for MIN_MATCH pixels
	static CAT_INLINE u32 HashPixels(const u32 * CAT_RESTRICT rgba) {
		return (u32)( ( ((u64)rgba[0] << 32) | rgba[1] ) * HASH_MULT >> (64 - HASH_BITS) );
	}

	bool findMatches(const u32 * CAT_RESTRICT rgba, const u8 * CAT_RESTRICT residuals, int xsize, int ysize, ImageMaskWriter *mask);

	// Encoders
	HuffmanEncoder _lz_len_encoder, _lz_sdist_encoder, _lz_ldist_encoder;

public:
	// Not worth matching fewer than MIN_MATCH
	static const int MIN_MATCH = 2; // pixels
	static const int MAX_MATCH = 256; // pixels
	static const int WIN_SIZE = 1024 * 1024; // pixels
	static const int LAST_COUNT = 4; // Keep track of recently emitted distances

	bool init(const u32 * CAT_RESTRICT rgba, const u8 * CAT_RESTRICT residuals, int xsize, int ysize, ImageMaskWriter *mask);

	void train(EntropyEncoder &ee);

	int writeTables(ImageWriter &writer);
	int write(EntropyEncoder &ee, ImageWriter &writer);
};


//// MonoMatchFinder

class MonoMatchFinder : public LZMatchFinder {
	static const int CHAIN_LIMIT = 32;

	static const int HASH_BITS = 18;
	static const int HASH_SIZE = 1 << HASH_BITS;
	//static const u64 HASH_MULT = 0xc6a4a7935bd1e995ULL;

	static const int ESCAPE_CODE_LOW_BOUND = 2; // bits

	// Returns hash for MIN_MATCH pixels
	static CAT_INLINE u32 HashPixels(const u8 * CAT_RESTRICT mono) {
		const u16 word0 = *reinterpret_cast<const u16 *>( mono );
		return word0;
	}

	// Encoders
	HuffmanEncoder _lz_len_encoder, _lz_sdist_encoder, _lz_ldist_encoder;

public:
	// bool IsMasked(u16 x, u16 y)
	typedef Delegate2<bool, u16, u16> MaskDelegate;

	// Not worth matching fewer than MIN_MATCH
	static const int MIN_MATCH = 2; // pixels
	static const int MAX_MATCH = 256; // pixels
	static const int WIN_SIZE = 1024 * 1024; // pixels
	static const int LAST_COUNT = 4; // Keep track of recently emitted distances

protected:
	int scoreMatch(int distance, const u32 *recent, const u8 *costs, int &match_len, int &bits_saved);

	bool findMatches(SuffixArray3_State *sa3state, const u8 * CAT_RESTRICT mono, const u8 * CAT_RESTRICT costs, int xsize, int ysize);

public:
	bool init(const u8 * CAT_RESTRICT mono, int num_syms, const u8 * CAT_RESTRICT costs, int xsize, int ysize);

	void train(EntropyEncoder &ee);

	int writeTables(ImageWriter &writer);
	int write(int num_syms, EntropyEncoder &ee, ImageWriter &writer);
};


} // namespace cat

#endif // LZ_MATCH_FINDER_HPP

