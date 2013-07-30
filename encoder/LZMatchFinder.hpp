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

#include "../decoder/LZReader.hpp"
#include "../decoder/SmartArray.hpp"
#include "EntropyEncoder.hpp"
#include "../decoder/ImageRGBAReader.hpp"
#include "../decoder/MonoReader.hpp"
#include "SuffixArray3.hpp"

#include <vector>

/*
 * LZ Match Finder
 *
 * See decoder/LZReader.hpp for a description of the file format.
 */

namespace cat {


//// LZMatchFinder

class LZMatchFinder {
public:
	static const int ESCAPE_SYMS = LZReader::ESCAPE_SYMS;
	static const int LEN_SYMS = LZReader::LEN_SYMS;
	static const int LDIST_SYMS = LZReader::LDIST_SYMS;
	static const int SDIST_SYMS = LZReader::SDIST_SYMS;
	static const int MIN_MATCH = LZReader::MIN_MATCH;
	static const int MAX_MATCH = LZReader::MAX_MATCH;
	static const int WIN_SIZE = LZReader::WIN_SIZE;
	static const int LAST_COUNT = LZReader::LAST_COUNT;

	struct Parameters {
		int num_syms;		// First escape symbol / number of symbols
		int xsize, ysize;	// Image dimensions
		int prematch_chain_limit;	// Maximum number of walks down a hash chain to try for local matches
		int inmatch_chain_limit;	// Limit while inside a found match
		const u8 * CAT_RESTRICT costs;	// Cost per pixel in bits
	};

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

		LZMatch *next;			// Next accepted match

		CAT_INLINE LZMatch(u32 offset, u32 distance, u16 length, u32 saved) {
			this->offset = offset;
			this->distance = distance;
			this->length = length;
			this->saved = saved;
			this->next = 0;
		}
	};

protected:
	// Input parameters
	Parameters _params;
	int _pixels;

	// Match list
	std::vector<LZMatch> _matches;
	LZMatch * CAT_RESTRICT _match_head;

	// Encoders
	HuffmanEncoder _lz_len_encoder, _lz_sdist_encoder, _lz_ldist_encoder;

	// Bitmask
	SmartArray<u32> _mask;

	CAT_INLINE void setMask(u32 off) {
		_mask[off >> 5] |= 1 << (off & 31);
	}

	void init(Parameters &params);
	int scoreMatch(int distance, const u32 * CAT_RESTRICT recent, const u8 * CAT_RESTRICT costs, int &match_len, int &bits_saved);
	void rejectMatches();

public:
	CAT_INLINE bool masked(u16 x, u16 y) {
		const int off = x + y * _params.xsize;
		return ( _mask[off >> 5] & (1 << (off & 31)) ) != 0;
	}

	CAT_INLINE LZMatch *getHead() {
		return _match_head;
	}

	void train(LZMatch * CAT_RESTRICT match, EntropyEncoder &ee);

	int writeTables(ImageWriter &writer);
	int write(LZMatch * CAT_RESTRICT match, EntropyEncoder &ee, ImageWriter &writer);
};


//// RGBAMatchFinder

class RGBAMatchFinder : public LZMatchFinder {
	static const int HASH_BITS = 18;
	static const int HASH_SIZE = 1 << HASH_BITS;
	static const u64 HASH_MULT = 0xc6a4a7935bd1e995ULL; // from WebP

	// Returns hash for MIN_MATCH pixels
	static CAT_INLINE u32 HashPixels(const u32 * CAT_RESTRICT rgba) {
		return (u32)( ( ((u64)rgba[0] << 32) | rgba[1] ) * HASH_MULT >> (64 - HASH_BITS) );
	}

	bool findMatches(SuffixArray3_State * CAT_RESTRICT sa3state, const u32 * CAT_RESTRICT rgba);

public:
	bool init(const u32 * CAT_RESTRICT rgba, Parameters &params);
};


//// MonoMatchFinder

class MonoMatchFinder : public LZMatchFinder {
protected:
	static const int HASH_BITS = 16;
	static const int HASH_SIZE = 1 << HASH_BITS;

	// Returns hash for MIN_MATCH pixels
	static CAT_INLINE u32 HashPixels(const u8 * CAT_RESTRICT mono) {
		// No need to actually hash in this case!
		const u16 word0 = *reinterpret_cast<const u16 *>( mono );
		return word0;
	}

	bool findMatches(SuffixArray3_State * CAT_RESTRICT sa3state, const u8 * CAT_RESTRICT mono);

public:
	bool init(const u8 * CAT_RESTRICT mono, Parameters &params);
};


} // namespace cat

#endif // LZ_MATCH_FINDER_HPP

