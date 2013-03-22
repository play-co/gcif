#include "MurmurHash3.hpp"
using namespace cat;

void MurmurHash3::hashBytes(u8 *bytes, int bc) {
	int wc = bc >> 2;
	u32 *keys = reinterpret_cast<u32*>( bytes );
	for (int ii = 0; ii < wc; ++ii) {
		u32 k1 = keys[ii];

		k1 *= c1;
		k1 = CAT_ROL32(k1, 15);
		k1 *= c2;

		h1 ^= k1;
		h1 = CAT_ROL32(h1, 13); 
		h1 = h1 * 5 + 0xe6546b64;
	}

	bytes += wc << 2;
	u32 k1 = 0;
	switch (bc & 3) {
		case 3: k1 ^= bytes[2] << 16;
		case 2: k1 ^= bytes[1] << 8;
		case 1: k1 ^= bytes[0];
				k1 *= c1;
				k1 = CAT_ROL32(k1, 15);
				k1 *= c2;
				h1 ^= k1;
	}
}

