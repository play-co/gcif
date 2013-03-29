#include "ImageFilterWriter.hpp"
using namespace cat;


//// ImageFilterWriter

void ImageFilterWriter::clear() {
	if (_matrix) {
		delete []_matrix;
		_matrix = 0;
	}
}

bool ImageFilterWriter::init(int width, int height) {
	clear();

	if (width < 8 || height < 8) {
		return false;
	}

	if ((width & 7) | (height & 7)) {
		return false;
	}

	_w = width >> 3;
	_h = height >> 3;
	_matrix = new u16[_w * _h];

	return true;
}

int ImageFilterWriter::initFromRGBA(u8 *pixels, int width, int height) {
	if (!init(width, height)) {
		return WE_BAD_DIMS;
	}

	u16 *filterWriter = _matrix;

	static const int FSZ = 8;

	for (int y = height - FSZ; y >= 0; y -= FSZ) {
		for (int x = width - FSZ; x >= 0; x -= FSZ) {
			int predErrors[SF_COUNT*CF_COUNT] = {0};

			// Determine best filter combination to use

			// For each pixel in the 8x8 zone,
			for (int yy = 0; yy < FSZ; ++yy) {
				for (int xx = 0; xx < FSZ; ++xx) {
					int px = x + xx, py = y + yy;
					u8 *p = &pixels[(px + py * width) * 4];

					// Calculate spatial filter predictions
					u8 sfPred[SF_COUNT*3];
					for (int plane = 0; plane < 3; ++plane) {
						int a, c, b, d;

						// Grab ABCD
						if (px > 0) {
							if (py > 0) {
								a = p[plane - 4];
								c = p[plane - 4 - width];
								b = p[plane - width];
								d = p[plane + 4 - width];
							} else {
								a = p[plane - 4];
								c = 0;
								b = 0;
								d = 0;
							}
						} else {
							if (py > 0) {
								a = 0;
								c = 0;
								b = p[plane - width];
								d = p[plane + 4 - width];
							} else {
								a = 0;
								c = 0;
								b = 0;
								d = 0;
							}
						}

						sfPred[SF_Z + plane*SF_COUNT] = 0;
						sfPred[SF_A + plane*SF_COUNT] = a;
						sfPred[SF_B + plane*SF_COUNT] = b;
						sfPred[SF_C + plane*SF_COUNT] = c;
						sfPred[SF_D + plane*SF_COUNT] = d;
						sfPred[SF_A_BC + plane*SF_COUNT] = (u8)(a + (b - c) / 2);
						sfPred[SF_B_AC + plane*SF_COUNT] = (u8)(b + (a - c) / 2);
						sfPred[SF_AB + plane*SF_COUNT] = (u8)((a + b) / 2);
						sfPred[SF_ABCD + plane*SF_COUNT] = (u8)((a + b + c + d + 1) / 4);
						sfPred[SF_AD + plane*SF_COUNT] = (u8)((a + d) / 2);
						int abc = a + b - c;
						if (abc > 255) abc = 255;
						else if (abc < 0) abc = 0;
						sfPred[SF_ABC_CLAMP + plane*SF_COUNT] = (u8)abc;

						// Paeth filter
						sfPred[SF_PAETH + plane*SF_COUNT] = paeth(a, b, c);

						// Modified Paeth
						sfPred[SF_ABC_PAETH + plane*SF_COUNT] = abc_paeth(a, b, c);
					}

					// Calculate color filter predictions
					u8 xr = p[0], xg = p[1], xb = p[2];
					for (int ii = 0; ii < SF_COUNT; ++ii) {
						// Get predicted RGB
						u8 pr = sfPred[ii];
						u8 pg = sfPred[ii + SF_COUNT];
						u8 pb = sfPred[ii + SF_COUNT*2];

						// Apply spatial filter
						u8 r = xr - pr;
						u8 g = xg - pg;
						u8 b = xb - pb;

						// Calculate color filter error
						predErrors[ii + SF_COUNT*CF_NOOP] += score(r)*score(r) + score(g)*score(g) + score(b)*score(b);
						predErrors[ii + SF_COUNT*CF_GB_RG] += score(r-g)*score(r-g) + score(g-b)*score(g-b) + score(b)*score(b);
						predErrors[ii + SF_COUNT*CF_GB_RB] += score(r-b)*score(r-b) + score(g-b)*score(g-b) + score(b)*score(b);
						predErrors[ii + SF_COUNT*CF_GR_BR] += score(r)*score(r) + score(g-r)*score(g-r) + score(b-r)*score(b-r);
						predErrors[ii + SF_COUNT*CF_GR_BG] += score(r)*score(r) + score(g-r)*score(g-r) + score(b-g)*score(b-g);
						predErrors[ii + SF_COUNT*CF_BG_RG] += score(r-g)*score(r-g) + score(g)*score(g) + score(b-g)*score(b-g);
					}
				}
			}

			// Find lowest error filter
			int lowestSum = predErrors[0];
			int bestSF = 0;
			for (int ii = 1; ii < SF_COUNT*CF_COUNT; ++ii) {
				if (predErrors[ii] < lowestSum) {
					lowestSum = predErrors[ii];
					bestSF = ii;
				}
			}

			// Write it out
			u8 sf = bestSF % SF_COUNT;
			u8 cf = bestSF / SF_COUNT;
			*filterWriter++ = ((u16)sf << 8) | cf;

			for (int yy = FSZ-1; yy >= 0; --yy) {
				for (int xx = FSZ-1; xx >= 0; --xx) {
					int px = x + xx, py = y + yy;
					u8 *p = &pixels[(px + py * width) * 4];

					u8 fp[3];

					for (int plane = 0; plane < 3; ++plane) {
						int a, c, b, d;

						// Grab ABCD
						if (px > 0) {
							if (py > 0) {
								a = p[plane - 4];
								c = p[plane - 4 - width];
								b = p[plane - width];
								d = p[plane + 4 - width];
							} else {
								a = p[plane - 4];
								c = 0;
								b = 0;
								d = 0;
							}
						} else {
							if (py > 0) {
								a = 0;
								c = 0;
								b = p[plane - width];
								d = p[plane + 4 - width];
							} else {
								a = 0;
								c = 0;
								b = 0;
								d = 0;
							}
						}

						u8 pred;

						switch (sf) {
							default:
							case SF_Z:			// 0
								pred = 0;
								break;
							case SF_A:			// A
								pred = a;
								break;
							case SF_B:			// B
								pred = b;
								break;
							case SF_C:			// C
								pred = c;
								break;
							case SF_D:			// D
								pred = d;
								break;
							case SF_A_BC:		// A + (B - C)/2
								pred = (u8)(a + (b - c) / 2);
								break;
							case SF_B_AC:		// B + (A - C)/2
								pred = (u8)(b + (a - c) / 2);
								break;
							case SF_AB:			// (A + B)/2
								pred = (u8)((a + b) / 2);
								break;
							case SF_ABCD:		// (A + B + C + D + 1)/4
								pred = (u8)((a + b + c + d + 1) / 4);
								break;
							case SF_AD:			// (A + D)/2
								pred = (u8)((a + d) / 2);
								break;
							case SF_ABC_CLAMP:	// A + B - C clamped to [0, 255]
								{
									int abc = a + b - c;
									if (abc > 255) abc = 255;
									else if (abc < 0) abc = 0;
									pred = (u8)abc;
								}
								break;
							case SF_PAETH:		// Paeth filter
								{
									pred = paeth(a, b, c);
								}
								break;
							case SF_ABC_PAETH:	// If A <= C <= B, A + B - C, else Paeth filter
								{
									pred = abc_paeth(a, b, c);
								}
								break;
						}

						fp[plane] = p[plane] - pred;
					}

					switch (cf) {
						default:
						case CF_NOOP:
							// No changes necessary
							break;
						case CF_GB_RG:
							fp[0] -= fp[1];
							fp[1] -= fp[2];
							break;
						case CF_GB_RB:
							fp[0] -= fp[2];
							fp[1] -= fp[2];
							break;
						case CF_GR_BR:
							fp[1] -= fp[0];
							fp[2] -= fp[0];
							break;
						case CF_GR_BG:
							fp[2] -= fp[1];
							fp[1] -= fp[0];
							break;
						case CF_BG_RG:
							fp[0] -= fp[1];
							fp[2] -= fp[1];
							break;
					}

					p[0] = score(fp[0]);
					p[1] = score(fp[1]);
					p[2] = score(fp[2]);
				}
			}

			//pixels[(x + y * width) * 4] = 255;
		}
	}

	return WE_OK;
}

