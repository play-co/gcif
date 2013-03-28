#ifndef IMAGE_FILTER_HPP
#define IMAGE_FILTER_HPP

#include "Platform.hpp"
#include "ImageWriter.hpp"

#include <vector>

namespace cat {


/*
 * Filter inputs:
 *
 * C B D
 * A ?
 */

enum SpatialFilters {
	SF_Z,			// 0
	SF_A,			// A
	SF_B,			// B
	SF_C,			// C
	SF_D,			// D
	SF_A_BC,		// A + (B - C)/2
	SF_B_AC,		// B + (A - C)/2
	SF_AB,			// (A + B)/2
	SF_ABCD,		// (A + B + C + D + 1)/4
	SF_AD,			// (A + D)/2
	SF_ABC_CLAMP,	// A + B - C clamped to [0, 255]
	SF_PAETH,		// Paeth filter
	SF_ABC_PAETH,	// If A <= C <= B, A + B - C, else Paeth filter

	SF_COUNT
};

enum ColorFilters {
	CF_NOOP,
	CF_GB_RG,	// g-=b, r-=g
	CF_GB_RB,	// g-=b, r-=b
	CF_GR_BR,	// g-=r, b-=r
	CF_GR_BG,	// g-=r, b-=g
	CF_BG_RG,	// b-=g, r-=g

	CF_COUNT
};



//// RGB Filters

class FilterDesc {
	int w, h;
	int *matrix;

public:
	FilterDesc() {
		w = 0; h = 0;
		matrix = 0;
	}
	virtual ~FilterDesc() {
		if (matrix) {
			delete []matrix;
		}
	}

	bool init(int width, int height) {
		if (width < 8 || height < 8) {
			return false;
		}
		if ((width & 3) | (height & 3)) {
			return false;
		}
		w = width >> 3;
		h = height >> 3;

		if (matrix) {
			delete []matrix;
		}
		matrix = new int[w * h];

		return true;
	}

	void setFilter(int x, int y, int cfsf) {
		x >>= 3;
		y >>= 3;
		matrix[x + y*w] = cfsf;
	}

	int getFilter(int x, int y) {
		x >>= 3;
		y >>= 3;
		return matrix[x + y*w];
	}
};

class ImageFilter {
	static CAT_INLINE int score(u8 p) {
		if (p < 128) {
			return p;
		} else {
			return 256 - p;
		}
	}

public:
	ImageFilter() {
	}
	virtual ~ImageFilter() {
	}

	// Precondition: Image has dimensions that are multiples of 8x8
	bool filterImage(u8 *pixels, int width, int height, FilterDesc &filters) {
		if (!filters.init(width, height)) {
			return false;
		}

		for (int y = 0; y < height; y += 8) {
			for (int x = 0; x < width; x += 8) {
				int predErrors[SF_COUNT*CF_COUNT] = {0};

				for (int yy = 0; yy < 8; ++yy) {
					for (int xx = 0; xx < 8; ++xx) {

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
							int pabc = a + b - c;
							int pa = abs(pabc - a);
							int pb = abs(pabc - b);
							int pc = abs(pabc - c);
							int paeth;
							if (pa <= pb && pa <= pc) {
								paeth = a;
							} else if (pb <= pc) {
								paeth = b;
							} else {
								paeth = c;
							}
							sfPred[SF_PAETH + plane*SF_COUNT] = (u8)paeth;

							// Modified Paeth
							int abc_paeth = paeth;
							if (a <= c && c <= b) {
								abc_paeth = pabc;
							}
							sfPred[SF_ABC_PAETH + plane*SF_COUNT] = (u8)abc_paeth;
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
							predErrors[ii + SF_COUNT*CF_NOOP] += score(r) + score(g) + score(b);
							predErrors[ii + SF_COUNT*CF_GB_RG] += score(r - g) + score(g - b) + score(b);
							predErrors[ii + SF_COUNT*CF_GB_RB] += score(r - b) + score(g - b) + score(b);
							predErrors[ii + SF_COUNT*CF_GR_BR] += score(r)     + score(g - r) + score(b - r);
							predErrors[ii + SF_COUNT*CF_GR_BG] += score(r)     + score(g - r) + score(b - g);
							predErrors[ii + SF_COUNT*CF_BG_RG] += score(r - g) + score(g)     + score(b - g);
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

				filters.setFilter(x, y, bestSF);
			}
		}

		return true;
	}
};


class RGBCodec {
public:
	RGBCodec() {
	}
	virtual ~RGBCodec() {
	}

	bool encodeImage(u8 *pixels, int width, int height, FilterDesc &filter) {
		int pel[3*4]; // {RGB(A), RGB(B), RGB(C), RGB(D)}

		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {

				int f = filter.getFilter(x, y);

				int cf = f / SF_COUNT;
				int sf = f % SF_COUNT;

				switch (sf) {
					case SF_Z:			// 0
						break;
					case SF_A:			// A
						break;
					case SF_B:			// B
						break;
					case SF_C:			// C
						break;
					case SF_D:			// D
						break;
					case SF_A_BC:		// A + (B - C)/2
						break;
					case SF_B_AC:		// B + (A - C)/2
						break;
					case SF_AB:			// (A + B)/2
						break;
					case SF_ABCD:		// (A + B + C + D + 1)/4
						break;
					case SF_AD:			// (A + D)/2
						break;
					case SF_ABC_CLAMP:	// A + B - C clamped to [0, 255]
						break;
					case SF_PAETH:		// Paeth filter
						break;
					case SF_ABC_PAETH:	// If A <= C <= B, A + B - C, else Paeth filter
						break;
				}
			}
		}
	}
};


} // namespace cat

#endif // IMAGE_FILTER_HPP

