#ifndef FILTERS_HPP
#define FILTERS_HPP

#include "Platform.hpp"

namespace cat {


//#define TEST_COLOR_FILTERS /* Verify color filters are reversible unit test */
//#define LOWRES_MASK /* Use zone sized granularity for fully-transparent alpha mask */

static const int FILTER_ZONE_SIZE = 4; // 4x4 blocks
static const int FILTER_ZONE_SIZE_SHIFT = 2; // FZS = 2^^n
static const int FILTER_ZONE_SIZE_MASK = FILTER_ZONE_SIZE - 1;


//// Spatial Filters

/*
 * Spatial filters from BCIF
 *
 * Filter inputs:
 *         E
 * F C B D
 *   A ?
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
	SF_ABCD,		// (A + B + C + D + 1)/4

	SF_PICK_LEFT,	// Pick A or C based on which is closer to F
	SF_PRED_UR,		// Predict gradient continues from E to D to current

	SF_AD,			// (A + D)/2

	SF_COUNT,

	// Disabled filters:
	SF_A_BC,		// A + (B - C)/2
	SF_B_AC,		// B + (A - C)/2
};


const u8 *spatialFilterPixel(u8 *p, int sf, int x, int y, int width);


//// Color Filters

/*
 * Color filters taken directly from this paper by Tilo Strutz
 * "ADAPTIVE SELECTION OF COLOUR TRANSFORMATIONS FOR REVERSIBLE IMAGE COMPRESSION" (2012)
 * http://www.eurasip.org/Proceedings/Eusipco/Eusipco2012/Conference/papers/1569551007.pdf
 *
 * YUV899 kills compression performance too much so we are using aliased but
 * reversible YUV888 transforms based on the ones from the paper where possible
 */

enum ColorFilters {
	// In order of preference (based on entropy scores from test images):
	CF_GB_RG,	// from BCIF
	CF_GR_BG,	// from BCIF
	CF_YUVr,	// YUVr from JPEG2000
	CF_D9,		// from the Strutz paper
	CF_D12,		// from the Strutz paper
	CF_D8,		// from the Strutz paper
	CF_E2_R,	// Derived from E2 and YCgCo-R
	CF_BG_RG,	// from BCIF (recommendation from LOCO-I paper)
	CF_GR_BR,	// from BCIF
	CF_D18,		// from the Strutz paper
	CF_B_GR_R,	// A decent default filter
	CF_D11,		// from the Strutz paper
	CF_D14,		// from the Strutz paper
	CF_D10,		// from the Strutz paper
	CF_YCgCo_R,	// Malvar's YCgCo-R
	CF_GB_RB,	// from BCIF
	CF_COUNT,

	// Disabled filters:
	// These do not appear to be reversible under YUV888
	CF_C7,		// from the Strutz paper
	CF_E2,		// from the Strutz paper
	CF_E1,		// from the Strutz paper
	CF_E4,		// from the Strutz paper
	CF_E5,		// from the Strutz paper
	CF_E8,		// from the Strutz paper
	CF_E11,		// from the Strutz paper
	CF_A3,		// from the Strutz paper
	CF_F1,		// from the Strutz paper
	CF_F2,		// from the Strutz paper
};


void convertRGBtoYUV(int cf, const u8 rgb[3], u8 out[3]);
void convertYUVtoRGB(int cf, const u8 yuv[3], u8 out[3]);

const char *GetColorFilterString(int cf);

#if TEST_COLOR_FILTERS
void testColorFilters();
#endif


} // namespace cat

#endif // FILTERS_HPP

