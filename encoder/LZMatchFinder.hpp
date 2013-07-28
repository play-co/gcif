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

#include "../decoder/SmartArray.hpp"
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
	/*
	 * LZ Escape Bitstream Format
	 *
	 * RGBA:
	 * Attached to the Y channel of the YUV encoder output are a number of
	 * extra "escape codes" that cause the decoder to go into an LZ decoding
	 * mode for the next few bits.
	 *
	 * Monochrome:
	 * After the last palette symbol there are some additional escape codes
	 * that act the same as for the RGBA escapes.
	 *
	 *
	 * Nice features of this format:
	 *
	 * (1) Include "same as last 1-4 distance" code and use it as often as possible.
	 *
	 * (2) Use most common 2D distance matches for escape codes in addition to common
	 * lengths.  This results in LZ matches under 5 pixels that often take only 7-9 bits
	 * to represent overall.
	 *
	 * (3) Use only short literal lengths for escape codes.  This is great because only
	 * long matches will make it through to the default "complex LZ" escape code, where
	 * I do not care too much about being careful about bit representation.
	 *
	 * (4) Only match up to 256 and if I have to emit a match length, emit a literal
	 * Huffman code for 2-256.  Longer matches do not matter and this makes full use of
	 * the statistics.
	 *
	 * (5) To encode distances, an extended "local region" of pixels up to 16 to the
	 * left/right for the current and previous row, and -8 through +8 for the next 7
	 * rows are assigned symbols in a Huffman code, in addition to a bit count encoding
	 * that indicates how many bits the distance contains as well as some of the high bits.
	 *
	 *
	 * The extra (32) escape codes are:
	 *
	 * 256 = "Distance Same as 1st Last"
	 * 257 = "Distance Same as 2nd Last"
	 * 258 = "Distance Same as 3rd Last"
	 * 259 = "Distance Same as 4th Last"
	 * 260 = "Distance = 1"
	 * 261 = "Distance = 3"
	 * 262 = "Distance = 4"
	 * 263 = "Distance = 5"
	 * 264 = "Distance = 6"
	 * 265 = "Distance = (-2, -1)"
	 * 266 = "Distance = (-1, -1)"
	 * 267 = "Distance = (0, -1)"
	 * 268 = "Distance = (1, -1)"
	 * 269 = "Distance = (2, -1)"
	 * 270 = "Length = 2" + ShortDistance code follows
	 * 271 = "Length = 3" + ShortDistance code follows
	 * 272 = "Length = 4" + ShortDistance code follows
	 * 273 = "Length = 5" + ShortDistance code follows
	 * 274 = "Length = 6" + ShortDistance code follows
	 * 275 = "Length = 7" + ShortDistance code follows
	 * 276 = "Length = 8" + ShortDistance code follows
	 * 277 = "Length = 9" + ShortDistance code follows
	 * 278 = Length code + ShortDistance code follows
	 * 279 = "Length = 2" + LongDistance code follows
	 * 280 = "Length = 3" + LongDistance code follows
	 * 281 = "Length = 4" + LongDistance code follows
	 * 282 = "Length = 5" + LongDistance code follows
	 * 283 = "Length = 6" + LongDistance code follows
	 * 284 = "Length = 7" + LongDistance code follows
	 * 285 = "Length = 8" + LongDistance code follows
	 * 286 = "Length = 9" + LongDistance code follows
	 * 287 = Length code + LongDistance code follows
	 *
	 * For codes 256-269, the following bits are a Length code.
	 *
	 *
	 * Length Huffman code:
	 *
	 * 0 = "Length = 2"
	 * 1 = "Length = 3"
	 * ...
	 * 254 = "Length = 256"
	 *
	 *
	 * ShortDistance Huffman code:
	 *
	 * 0 = "Distance = 2"
	 * 1 = "Distance = 7"
	 * 2 = "Distance = 8"
	 * 3 = "Distance = 9"
	 * 4 = "Distance = 10"
	 * 5 = "Distance = 11"
	 * 6 = "Distance = 12"
	 * 7 = "Distance = 13"
	 * 8 = "Distance = 14"
	 * 9 = "Distance = 15"
	 * 10 = "Distance = 16"
	 *
	 * 11 = "Distance = (-16, -1)"
	 * 12 = "Distance = (-15, -1)"
	 * ...
	 * 24 = "Distance = (-3, -1)"
	 * 25 = "Distance = (3, -1)"
	 * ...
	 * 38 = "Distance = (16, -1)"
	 *
	 * 39 - 55 = "Distance (-8, -2) ... (8, -2)"
	 * 56 - 72 = "Distance (-8, -3) ... (8, -3)"
	 * 73 - 89 = "Distance (-8, -4) ... (8, -4)"
	 * 90 - 106 = "Distance (-8, -5) ... (8, -5)"
	 * 107 - 123 = "Distance (-8, -6) ... (8, -6)"
	 * 124 - 140 = "Distance (-8, -7) ... (8, -7)"
	 * 141 - 157 = "Distance (-8, -8) ... (8, -8)"
	 *
	 *
	 * LongDistance Huffman code:
	 *
	 * Let D = Distance - 17.
	 *
	 * Longer distances are encoded using 256 symbols to represent the high
	 * bits and the number of bits remaining at once.  Extra bits follow
	 * the distance code.
	 *
	 * Converting from D to (Code, EB):
	 * EB = Bits(D >> 5) + 1
	 * C0 = ((1 << (EB - 1)) - 1) << 5
	 * Code = ((D - C0) >> EB) + ((EB - 1) << 4)
	 *
	 * Converting from Code to (D0, EB):
	 * EB = (Code >> 4) + 1
	 * C0 = ((1 << (EB - 1)) - 1) << 5
	 * D0 = ((Code - ((EB - 1) << 4)) << EB) + C0;
	 * D = D0 + ExtraBits(EB)
	 */

	static const int LZ_LEN_SYMS = 255;
	static const int LZ_LDIST_SYMS = 256;
	static const int LZ_SDIST_SYMS = 158;

	struct Parameters {
		int num_syms;		// First escape symbol / number of symbols
		int xsize, ysize;	// Image dimensions
		int chain_limit;	// Maximum number of walks down a hash chain to try for local matches
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

	// Not worth matching fewer than MIN_MATCH
	static const int MIN_MATCH = 2; // pixels
	static const int MAX_MATCH = 256; // pixels
	static const int WIN_SIZE = 1024 * 1024; // pixels
	static const int LAST_COUNT = 4; // Keep track of recently emitted distances

protected:
	// Input parameters
	Parameters _params;
	int _pixels;

	// Match list
	std::vector<LZMatch> _matches;
	LZMatch *_head_match;

	// Encoders
	HuffmanEncoder _lz_len_encoder, _lz_sdist_encoder, _lz_ldist_encoder;

	// Bitmask
	SmartArray<u32> _mask;

	CAT_INLINE void setMask(u32 off) {
		_mask[off >> 5] |= 1 << (off & 31);
	}

	void init(Parameters &params);
	int scoreMatch(int distance, const u32 *recent, const u8 *costs, int &match_len, int &bits_saved);
	void rejectMatches();

public:
	CAT_INLINE bool masked(u16 x, u16 y) {
		const int off = x + y * _params.xsize;
		return ( _mask[off >> 5] & (1 << (off & 31)) ) != 0;
	}

	CAT_INLINE LZMatch *getHead() {
		return _head_match;
	}

	void train(EntropyEncoder &ee);

	int writeTables(ImageWriter &writer);
	int write(EntropyEncoder &ee, ImageWriter &writer);
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

	bool findMatches(SuffixArray3_State *sa3state, const u32 * CAT_RESTRICT rgba);

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

	bool findMatches(SuffixArray3_State *sa3state, const u8 * CAT_RESTRICT mono);

public:
	bool init(const u8 * CAT_RESTRICT mono, Parameters &params);
};


} // namespace cat

#endif // LZ_MATCH_FINDER_HPP

