/*
	Copyright (c) 2013 Game Closure.  All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	* Redistributions of source code must retain the above copyright notice,
	  this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	* Neither the name of GCIF nor the names of its contributors may be used
	  to endorse or promote products derived from this software without
	  specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef FILTERS_HPP
#define FILTERS_HPP

#include "Platform.hpp"

namespace cat {


//// Spatial Filters

/*
 * Spatial filters from BCIF, supplemented with CBloom's and our contributions.
 *
 * Filter inputs:
 *         E
 * F C B D
 *   A ?
 *
 * In addition to the static filters defined here (which are fast to evaluate),
 * there are a number of linear tapped filters based on A,B,C,D.  Usually a few
 * of these are preferable to the defaults.  And the encoder transmits which
 * ones are overwritten in the image file so the decoder stays in synch.
 *
 * I found through testing that a small list of about 80 tapped filters are
 * ever preferable to one of the default filters, out of all 6544 combinations,
 * so only those are evaluated and sent.
 */

enum SpatialFilters {
	// Simple filters
	SF_A,				// A
	SF_B,				// B
	SF_C,				// C
	SF_D,				// D
	SF_Z,				// 0

	// Dual average filters (round down)
	SF_AVG_AB,			// (A + B) / 2
	SF_AVG_AC,			// (A + C) / 2
	SF_AVG_AD,			// (A + D) / 2
	SF_AVG_BC,			// (B + C) / 2
	SF_AVG_BD,			// (B + D) / 2
	SF_AVG_CD,			// (C + D) / 2

	// Dual average filters (round up)
	SF_AVG_AB1,			// (A + B + 1) / 2
	SF_AVG_AC1,			// (A + C + 1) / 2
	SF_AVG_AD1,			// (A + D + 1) / 2
	SF_AVG_BC1,			// (B + C + 1) / 2
	SF_AVG_BD1,			// (B + D + 1) / 2
	SF_AVG_CD1,			// (C + D + 1) / 2

	// Triple average filters (round down)
	SF_AVG_ABC,			// (A + B + C) / 3
	SF_AVG_ACD,			// (A + C + D) / 3
	SF_AVG_ABD,			// (A + B + D) / 3
	SF_AVG_BCD,			// (B + C + D) / 3

	// Quad average filters (round down)
	SF_AVG_ABCD,		// (A + B + C + D) / 4

	// Quad average filters (round up)
	SF_AVG_ABCD1,		// (A + B + C + D + 2) / 4

	// ABCD Complex filters
	SF_CLAMP_GRAD,		// ClampedGradPredictor (CBloom #12)
	SF_SKEW_GRAD,		// Gradient skewed towards average (CBloom #5)
	SF_ABC_CLAMP,		// A + B - C clamped to [0, 255] (BCIF)
	SF_PAETH,			// Paeth (PNG)
	SF_ABC_PAETH,		// If A <= C <= B, A + B - C, else Paeth filter (BCIF)
	SF_PLO,				// Offset PL (BCIF)
	SF_SELECT,			// Select (WebP)

	// EF Complex filters
	SF_SELECT_F,		// Pick A or C based on which is closer to F (New)
	SF_ED_GRAD,			// Predict gradient continues from E to D to current (New)

	SF_BASIC_COUNT
};

static const int DIV2_TAPPED_COUNT = 80;
static const int SF_COUNT = SF_BASIC_COUNT + DIV2_TAPPED_COUNT;

/*
 * RGBA filter
 *
 * p: Pointer to current RGBA pixel
 * temp: Temp workspace if we need it (3 bytes)
 * x, y: Pixel location
 * width: Pixels in width of p buffer
 *
 * Returns filter prediction in pred.
 */
typedef const u8 *(*RGBAFilterFunc)(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int width);

struct RGBAFilterFuncs {
	// Safe for any input that is actually on the image
	RGBAFilterFunc safe;

	// Assumes that x>0, y>0, x<width-1
	RGBAFilterFunc unsafe;
};

extern const RGBAFilterFuncs RGBA_FILTERS[SF_COUNT];

/*
 * Monochrome filter
 *
 * p: Pointer to current monochrome pixel
 * num_syms: Number of symbols
 * x, y: Pixel location
 * width: Pixels in width of p buffer
 *
 * Returns filter prediction.
 */
typedef u8 (*MonoFilterFunc)(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int width);

struct MonoFilterFuncs {
	// Safe for any input that is actually on the image
	MonoFilterFunc safe;

	// Assumes that x>0, y>0, x<width-1
	MonoFilterFunc unsafe;
};

extern const MonoFilterFuncs MONO_FILTERS[SF_COUNT];


//// Color Filters

/*
 * Color filters taken directly from this paper by Tilo Strutz
 * "ADAPTIVE SELECTION OF COLOUR TRANSFORMATIONS FOR REVERSIBLE IMAGE COMPRESSION" (2012)
 * http://www.eurasip.org/Proceedings/Eusipco/Eusipco2012/Conference/papers/1569551007.pdf
 *
 * YUV899 kills compression performance too much so we are using aliased but
 * reversible YUV888 transforms based on the ones from the paper where possible.
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
	CF_NONE,	// No modification

	CF_COUNT
};

typedef void (*RGB2YUVFilterFunction)(const u8 * CAT_RESTRICT rgb_in, u8 * CAT_RESTRICT yuv_out);
typedef void (*YUV2RGBFilterFunction)(const u8 * CAT_RESTRICT yuv_in, u8 * CAT_RESTRICT rgb_out);

extern const RGB2YUVFilterFunction RGB2YUV_FILTERS[];
extern const YUV2RGBFilterFunction YUV2RGB_FILTERS[];

const char *GetColorFilterString(int cf);


} // namespace cat

#endif // FILTERS_HPP

