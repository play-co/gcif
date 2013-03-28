#include "ImageFilter.hpp"
using namespace cat;


//// ImageMask

bool ImageMask::initFromRGBA(u8 *rgba, int width, int height) {
	if (mask) {
		delete []mask;
	}

	stride = (width + 31) >> 5;

	// Assumes fully-transparent black for now
	// TODO: Also work with images that have a different most-common color
	repRGBA = getLE(0x00000000);

	const unsigned char *alpha = (const unsigned char*)&rgba[0] + 3;

	u32 *writer = mask;

	for (int y = 0; y < height; ++y) {
		for (int x = 0, len = width >> 5; x < len; ++x) {
			u32 bits = (alpha[0] == 0);
			bits = (bits << 1) | (alpha[4] == 0);
			bits = (bits << 1) | (alpha[8] == 0);
			bits = (bits << 1) | (alpha[12] == 0);
			bits = (bits << 1) | (alpha[16] == 0);
			bits = (bits << 1) | (alpha[20] == 0);
			bits = (bits << 1) | (alpha[24] == 0);
			bits = (bits << 1) | (alpha[28] == 0);
			bits = (bits << 1) | (alpha[32] == 0);
			bits = (bits << 1) | (alpha[36] == 0);
			bits = (bits << 1) | (alpha[40] == 0);
			bits = (bits << 1) | (alpha[44] == 0);
			bits = (bits << 1) | (alpha[48] == 0);
			bits = (bits << 1) | (alpha[52] == 0);
			bits = (bits << 1) | (alpha[56] == 0);
			bits = (bits << 1) | (alpha[60] == 0);
			bits = (bits << 1) | (alpha[64] == 0);
			bits = (bits << 1) | (alpha[68] == 0);
			bits = (bits << 1) | (alpha[72] == 0);
			bits = (bits << 1) | (alpha[76] == 0);
			bits = (bits << 1) | (alpha[80] == 0);
			bits = (bits << 1) | (alpha[84] == 0);
			bits = (bits << 1) | (alpha[88] == 0);
			bits = (bits << 1) | (alpha[92] == 0);
			bits = (bits << 1) | (alpha[96] == 0);
			bits = (bits << 1) | (alpha[100] == 0);
			bits = (bits << 1) | (alpha[104] == 0);
			bits = (bits << 1) | (alpha[108] == 0);
			bits = (bits << 1) | (alpha[112] == 0);
			bits = (bits << 1) | (alpha[116] == 0);
			bits = (bits << 1) | (alpha[120] == 0);
			bits = (bits << 1) | (alpha[124] == 0);

			*writer++ = bits;
			alpha += 128;
		}

		u32 ctr = width & 31;
		if (ctr) {
			u32 bits = 0;
			while (ctr--) {
				bits = (bits << 1) | (alpha[0] == 0);
				alpha += 4;
			}

			*writer++ = bits;
		}
	}
}

