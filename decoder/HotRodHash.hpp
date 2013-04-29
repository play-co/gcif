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

#ifndef HOTROD_HASH_HPP
#define HOTROD_HASH_HPP

#include "Platform.hpp"
#include "Enforcer.hpp"

namespace cat {


/*
 * Based on MurmurHash3 - 32 bit version
 * https://code.google.com/p/smhasher/source/browse/trunk/MurmurHash3.cpp
 * by Austin Appleby
 *
 * I took out the final mixing function because it doesn't make sense
 * from a file validation perspective.
 */

class FileValidationHash {
	static const u32 c1 = 0xcc9e2d51;
	static const u32 c2 = 0x1b873593;

	u32 h1;

public:
	CAT_INLINE void init(u32 seed) {
		h1 = seed;
	}

	CAT_INLINE void hashWord(u32 k1) {
		k1 *= c1;
		k1 = CAT_ROL32(k1, 15);
		k1 *= c2;

		h1 ^= k1;
		h1 = CAT_ROL32(h1, 13); 
		h1 = h1 * 5 + 0xe6546b64;
	}

	CAT_INLINE void hashWords(u32 *keys, int wc) {
		for (int ii = 0; ii < wc; ++ii) {
			u32 k1 = keys[ii];

			k1 *= c1;
			k1 = CAT_ROL32(k1, 15);
			k1 *= c2;

			h1 ^= k1;
			h1 = CAT_ROL32(h1, 13); 
			h1 = h1 * 5 + 0xe6546b64;
		}
	}

	void hashBytes(u8 *bytes, int bc);

	CAT_INLINE u32 final(u32 len) {
		return h1 ^ len;
	}

	static CAT_INLINE u32 hash(void *data, int bytes) {
		FileValidationHash h;
		h.init(123456789);
		h.hashBytes(reinterpret_cast<u8*>( data ), bytes);
		return h.final(bytes);
	}
};


/*
 * HotRodHash - Based on MurmurHash3
 *
 * This hash isn't going to pass any tests.  It's designed to allow for
 * fast operation on ARM platforms.  This is a hot-rodded version of the
 * MurmurHash3 algorithm to be used for fast file validation only.
 */
class HotRodHash {
	static const u32 c1 = 0xcc9e2d51;

	u32 h1;

public:
	CAT_INLINE void init(u32 seed) {
		h1 = seed;
	}

	CAT_INLINE void hashWord(u32 k1) {
		h1 ^= k1;
		h1 = CAT_ROL32(h1, 13); 
		h1 = h1 * 5 + 0xe6546b64;
	}

	CAT_INLINE void hashWords(u32 *keys, int wc) {
		CAT_DEBUG_ENFORCE(keys != 0);

		for (int ii = 0; ii < wc; ++ii) {
			hashWord(keys[ii]);
		}
	}

	void hashBytes(u8 *bytes, int bc);

	CAT_INLINE u32 final(u32 len) {
		return h1 ^ len;
	}

	static CAT_INLINE u32 hash(void *data, int bytes) {
		CAT_DEBUG_ENFORCE(data != 0 && bytes >= 0);

		HotRodHash h;
		h.init(123456789);
		h.hashBytes(reinterpret_cast<u8*>( data ), bytes);
		return h.final(bytes);
	}
};


} // namespace cat

#endif // HOTROD_HASH_HPP

