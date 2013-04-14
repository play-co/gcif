#include "HotRodHash.hpp"
using namespace cat;

void HotRodHash::hashBytes(u8 *bytes, int bc) {
	int wc = bc >> 2;
	u32 *keys = reinterpret_cast<u32*>( bytes );
	hashWords(keys, wc);

	bytes += wc << 2;
	u32 k1 = 0;
	switch (bc & 3) {
		case 3: k1 ^= bytes[2] << 16;
		case 2: k1 ^= bytes[1] << 8;
		case 1: k1 ^= bytes[0];
				h1 += k1 * c1;
				h1 ^= CAT_ROL32(k1, 15);
	}
}

