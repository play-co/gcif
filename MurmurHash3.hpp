#ifndef MURMUR_HASH_3_HPP
#define MURMUR_HASH_3_HPP

#include "Platform.hpp"

namespace cat {


/*
 * MurmurHash3 - 32 bit version
 * https://code.google.com/p/smhasher/source/browse/trunk/MurmurHash3.cpp
 * by Austin Appleby
 *
 * Chosen because it works quickly on whole words at a time, just like GCIF
 */
class MurmurHash3 {
	static const u32 c1 = 0xcc9e2d51;
	static const u32 c2 = 0x1b873593;

	u32 h1;

	static CAT_INLINE u32 fmix(u32 h) {
		h ^= h >> 16;
		h *= 0x85ebca6b;
		h ^= h >> 13;
		h *= 0xc2b2ae35;
		h ^= h >> 16;
		return h;
	}

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
		return fmix(h1 ^ len);
	}

	static CAT_INLINE u32 hash(void *data, int bytes) {
		MurmurHash3 h;
		h.init(123456789);
		h.hashBytes(reinterpret_cast<u8*>( data ), bytes);
		return h.final(bytes);
	}
};


} // namespace cat

#endif // MURMUR_HASH_3_HPP

