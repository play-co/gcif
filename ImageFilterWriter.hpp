#ifndef IMAGE_FILTER_WRITER_HPP
#define IMAGE_FILTER_WRITER_HPP

#include "Platform.hpp"
#include "ImageWriter.hpp"
#include "ImageMaskWriter.hpp"

namespace cat {


static const int FILTER_ZONE_SIZE = 4;
static const int FILTER_RLE_SYMS = 8;
static const int FILTER_MATCH_FUZZ = 16;

/*
 * Spatial filters from BCIF
 *
 * Filter inputs:
 *	C B D
 *	A ?
 */

enum SpatialFilters {
	// In the order they are applied in the case of a tie:
	SF_Z,			// 0
	SF_D,			// D
	SF_C,			// C
	SF_B,			// B
	SF_A,			// A
	SF_AB,			// (A + B)/2
	SF_BD,			// (B + D)/2
	SF_ABC_CLAMP,	// A + B - C clamped to [0, 255]
	SF_PAETH,		// Paeth filter
	SF_ABC_PAETH,	// If A <= C <= B, A + B - C, else Paeth filter
	SF_PL,			// Use ABC to determine if increasing or decreasing
	SF_PLO,			// Offset PL
	SF_AD,			// (A + D)/2
	SF_ABCD,		// (A + B + C + D + 1)/4

	SF_COUNT,

	// Disabled filters:
	SF_A_BC,		// A + (B - C)/2
	SF_B_AC,		// B + (A - C)/2

	SF_TEST,		// Crazy random test filter
};


/*
 * Color filters taken directly from this paper by Tilo Strutz
 * "ADAPTIVE SELECTION OF COLOUR TRANSFORMATIONS FOR REVERSIBLE IMAGE COMPRESSION" (2012)
 * http://www.eurasip.org/Proceedings/Eusipco/Eusipco2012/Conference/papers/1569551007.pdf
 *
 * YUV899 kills compression performance too much so we are using aliased but
 * reversible YUV888 transforms based on the ones from the paper where possible
 */

enum ColorFilters {
	// In order of preference:
	CF_YUVr,	// YUVr from JPEG2000

	CF_YCgCo_R,	// Malvar's YCgCo-R

	CF_D8,		// from the Strutz paper
	CF_D9,		// from the Strutz paper
	CF_D14,		// from the Strutz paper

	CF_D10,		// from the Strutz paper
	CF_D11,		// from the Strutz paper
	CF_D12,		// from the Strutz paper
	CF_D18,		// from the Strutz paper

	CF_GB_RG,	// from BCIF
	CF_GB_RB,	// from BCIF
	CF_GR_BR,	// from BCIF
	CF_GR_BG,	// from BCIF
	CF_BG_RG,	// from BCIF (recommendation from LOCO-I paper)

	CF_RGB,		// Original RGB

	CF_COUNT,

	// Disabled filters:
	// These do not appear to be reversible under YUV888
	CF_E2,		// from the Strutz paper
	CF_C7,		// from the Strutz paper
	CF_E1,		// from the Strutz paper
	CF_E4,		// from the Strutz paper
	CF_A3,		// from the Strutz paper
	CF_E5,		// from the Strutz paper
	CF_E8,		// from the Strutz paper
	CF_E11,		// from the Strutz paper
	CF_F1,		// from the Strutz paper
	CF_F2,		// from the Strutz paper
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
		_matrix[(x / FILTER_ZONE_SIZE) + (y / FILTER_ZONE_SIZE) * _w] = filter;
	}

	CAT_INLINE u16 getFilter(int x, int y) {
		return _matrix[(x / FILTER_ZONE_SIZE) + (y / FILTER_ZONE_SIZE) * _w];
	}

	int initFromRGBA(u8 *rgba, int width, int height, ImageMaskWriter &mask);
};


} // namespace cat

#endif // IMAGE_FILTER_WRITER_HPP

