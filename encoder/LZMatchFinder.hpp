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
#include "../decoder/Delegates.hpp"

#include <vector>

/*
 * LZ Match Finder
 *
 * This LZ system is only designed for RGBA data at this time.
 */

namespace cat {


//// LZMatchFinder

class LZMatchFinder {
protected:
	static const u32 GUARD_OFFSET = 0xffffffff;

	// Hash Chain search structure
	SmartArray<u32> _table;
	SmartArray<u32> _chain;

	// Match list, with guard at end
	struct LZMatch {
		u32 offset;
		u32 distance;
		u16 length;

		CAT_INLINE LZMatch(u32 offset, u32 distance, u16 length) {
			this->offset = offset;
			this->distance = distance;
			this->length = length;
		}
	};

	std::vector<LZMatch> _matches;
	LZMatch *_next_match;

public:
	typedef Delegate2<bool, u16, u16> MaskDelegate;

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
		return _next_match++;
	}
};


//// RGBAMatchFinder

class RGBAMatchFinder : public LZMatchFinder {
public:
	// Not worth matching less than MIN_MATCH
	static const int MIN_MATCH = 2; // pixels
	static const int MAX_MATCH = 4096; // pixels
	static const int WIN_SIZE = 1024 * 1024; // pixels

	/*
	 * Encoding cost in bits for RGBA data:
	 *
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
	static const int DIST_PREFIX_COST = 7; // bits
	static const int LEN_PREFIX_COST = 5; // bits
	static const int SAVED_PIXEL_BITS = 9; // bits

	static const int HASH_BITS = 18;
	static const int HASH_SIZE = 1 << HASH_BITS;
	static const u64 HASH_MULT = 0xc6a4a7935bd1e995ULL;

	// Returns hash for MIN_MATCH pixels
	static CAT_INLINE u32 HashPixels(const u32 * CAT_RESTRICT rgba) {
		return (u32)( ( ((u64)rgba[0] << 32) | rgba[1] ) * HASH_MULT >> (64 - HASH_BITS) );
	}

	bool findMatches(const u32 * CAT_RESTRICT rgba, int xsize, int ysize, LZMatchFinder::MaskDelegate mask);
};


//// MonoMatchFinder

class MonoMatchFinder : public LZMatchFinder {
public:
	// Not worth matching less than MIN_MATCH
	static const int MIN_MATCH = 6; // pixels
	static const int MAX_MATCH = 4096; // pixels
	static const int WIN_SIZE = 1024 * 1024; // pixels

	/*
	 * Encoding cost in bits for monochrome data:
	 */
	static const int DIST_PREFIX_COST = 7; // bits
	static const int LEN_PREFIX_COST = 5; // bits
	static const int SAVED_PIXEL_BITS = 2; // bits

	static const int HASH_BITS = 18;
	static const int HASH_SIZE = 1 << HASH_BITS;
	static const u64 HASH_MULT = 0xc6a4a7935bd1e995ULL;

	// Returns hash for MIN_MATCH pixels
	static CAT_INLINE u32 HashPixels(const u8 * CAT_RESTRICT mono) {
		u16 word1 = mono[4];
		word1 |= static_cast<u16>( mono[5] ) << 8;

		const u32 * CAT_RESTRICT word0 = reinterpret_cast<const u32 *>( mono );

		return (u32)( ( ((u64)word1 << 32) | *word0 ) * HASH_MULT >> (64 - HASH_BITS) );
	}

	bool findMatches(const u8 * CAT_RESTRICT mono, int xsize, int ysize, LZMatchFinder::MaskDelegate mask);
};


} // namespace cat

#endif // LZ_MATCH_FINDER_HPP

