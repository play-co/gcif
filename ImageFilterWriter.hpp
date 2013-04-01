#ifndef IMAGE_FILTER_WRITER_HPP
#define IMAGE_FILTER_WRITER_HPP

#include "Platform.hpp"
#include "ImageWriter.hpp"
#include "ImageMaskWriter.hpp"

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
	SF_AB,			// (A + B)/2
	SF_AD,			// (A + D)/2
	SF_A_BC,		// A + (B - C)/2
	SF_B_AC,		// B + (A - C)/2
	SF_ABCD,		// (A + B + C + D + 1)/4
	SF_ABC_CLAMP,	// A + B - C clamped to [0, 255]
	SF_PAETH,		// Paeth filter
	SF_ABC_PAETH,	// If A <= C <= B, A + B - C, else Paeth filter

	SF_COUNT
};

enum ColorFilters {
	CF_GB_RG,	// g-=b, r-=g
	CF_BG_RG,	// b-=g, r-=g
	CF_GR_BG,	// g-=r, b-=g
	CF_NOOP,
	CF_GB_RB,	// g-=b, r-=b
	CF_GR_BR,	// g-=r, b-=r

	CF_COUNT
};



//// ImageFilterWriter

class ImageFilterWriter {
	int _w, _h;
	u16 *_matrix;
	u8 *_chaos;

	void clear();

	bool init(int width, int height);
	void decideFilters(u8 *rgba, int width, int height, ImageMaskWriter &mask);
	void applyFilters(u8 *rgba, int width, int height, ImageMaskWriter &mask);
	void chaosEncode(u8 *rgba, int width, int height, ImageMaskWriter &mask);

public:
	CAT_INLINE ImageFilterWriter() {
		_matrix = 0;
		_chaos = 0;
	}
	CAT_INLINE virtual ~ImageFilterWriter() {
		clear();
	}

	CAT_INLINE void setFilter(int x, int y, u16 filter) {
		_matrix[(x >> 3) + (y >> 3) * _w] = filter;
	}

	CAT_INLINE u16 getFilter(int x, int y) {
		return _matrix[(x >> 3) + (y >> 3) * _w];
	}

	static CAT_INLINE u8 paeth(int a, int b, int c) {
		// Paeth filter
		int pabc = a + b - c;
		int pa = abs(pabc - a);
		int pb = abs(pabc - b);
		int pc = abs(pabc - c);

		if (pa <= pb && pa <= pc) {
			return (u8)a;
		} else if (pb <= pc) {
			return (u8)b;
		} else {
			return (u8)c;
		}
	}

	static CAT_INLINE u8 abc_paeth(int a, int b, int c) {
		// Paeth filter with modifications from BCIF
		int pabc = a + b - c;
		if (a <= c && c <= b) {
			return (u8)pabc;
		}

		int pa = abs(pabc - a);
		int pb = abs(pabc - b);
		int pc = abs(pabc - c);

		if (pa <= pb && pa <= pc) {
			return (u8)a;
		} else if (pb <= pc) {
			return (u8)b;
		} else {
			return (u8)c;
		}
	}

	int initFromRGBA(u8 *rgba, int width, int height, ImageMaskWriter &mask);
};


} // namespace cat

#endif // IMAGE_FILTER_WRITER_HPP

