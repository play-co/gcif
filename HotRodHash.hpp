#ifndef HOTROD_HASH_HPP
#define HOTROD_HASH_HPP

#include "Platform.hpp"

namespace cat {


/*
 * Based on MurmurHash3 - 32 bit version
 * https://code.google.com/p/smhasher/source/browse/trunk/MurmurHash3.cpp
 * by Austin Appleby
 *
 * This hash isn't going to pass any tests.  It's designed to allow for
 * fast operation on ARM platforms.  This is a hotrodded version of the
 * MurmurHash3 algorithm to be used for file validation only.
 */
class HotRodHash {
	static const u32 c1 = 0xcc9e2d51;

	u32 h1;

public:
	CAT_INLINE void init(u32 seed) {
		h1 = seed;
	}

	CAT_INLINE void hashWord(u32 k1) {
		h1 += k1 * c1;
		h1 ^= CAT_ROL32(k1, 15);

		h1 = CAT_ROL32(h1, 13); 
		h1 = h1 * 5 + 0xe6546b64;
	}

	CAT_INLINE void hashWords(u32 *keys, int wc) {
		for (int ii = 0; ii < wc; ++ii) {
			hashWord(keys[ii]);
		}
	}

	void hashBytes(u8 *bytes, int bc);

	CAT_INLINE u32 final(u32 len) {
		return h1 ^ len;
	}

	static CAT_INLINE u32 hash(void *data, int bytes) {
		HotRodHash h;
		h.init(123456789);
		h.hashBytes(reinterpret_cast<u8*>( data ), bytes);
		return h.final(bytes);
	}
};


} // namespace cat

#endif // HOTROD_HASH_HPP

