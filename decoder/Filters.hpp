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


static const int FILTER_ZONE_SIZE_SHIFT = 2; // Block size pow2
static const int FILTER_ZONE_SIZE = 1 << FILTER_ZONE_SIZE_SHIFT; // 4x4
static const int FILTER_ZONE_SIZE_MASK = FILTER_ZONE_SIZE - 1;


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
	// In the order they are applied in the case of a tie:
	// WARNING: Changing the order of these requires changes to SPATIAL_FILTERS
	SF_Z,			// 0
	SF_D,			// D
	SF_C,			// C
	SF_B,			// B
	SF_A,			// A
	SF_AB,			// (A + B)/2
	SF_BD,			// (B + D)/2
	SF_CLAMP_GRAD,	// CBloom: 12: ClampedGradPredictor
	SF_SKEW_GRAD,	// CBloom: 5: Gradient skewed towards average
	SF_PICK_LEFT,	// New: Pick A or C based on which is closer to F
	SF_PRED_UR,		// New: Predict gradient continues from E to D to current
	SF_ABC_CLAMP,	// A + B - C clamped to [0, 255]
	SF_PAETH,		// Paeth filter
	SF_ABC_PAETH,	// If A <= C <= B, A + B - C, else Paeth filter
	SF_PLO,			// Offset PL
	SF_ABCD,		// (A + B + C + D + 1)/4
	SF_AD,			// (A + D)/2

	SF_COUNT,

	// Disabled filters:
	SF_A_BC,		// A + (B - C)/2
	SF_B_AC,		// B + (A - C)/2
	SF_PL,			// Use ABC to determine if increasing or decreasing
};

typedef void (*SpatialFilterFunction)(const u8 *p, const u8 **pred, int x, int y, int width);


/*
 * Spatial Filter Set
 *
 * This class wraps the spatial filter arrays so that multiple threads can be
 * decoding images simultaneously, each with its own filter set.
 */
class SpatialFilterSet {
public:
	struct Functions {
		SpatialFilterFunction safe;

		// The unsafe version assumes x>0, y>0 and x<width-1 for speed
		SpatialFilterFunction unsafe;
	};

protected:
	Functions _filters[SF_COUNT];

public:
	/*
	 * Extended tapped linear filters
	 *
	 * The taps correspond to coefficients in the expression:
	 *
	 * Prediction = (t0*A + t1*B + t2*C + t3*D) / 2
	 *
	 * And the taps are selected from: {-4, -3, -2, -1, 0, 1, 2, 3, 4}.
	 *
	 * We ran simulations with a number of test images and chose the linear filters
	 * of this form that were consistently better than the default spatial filters.
	 * The selected linear filters are in the FILTER_TAPS[TAPPED_COUNT] array.
	 */
	static const int TAPPED_COUNT = 80;
	static const int FILTER_TAPS[TAPPED_COUNT][4];

public:
	CAT_INLINE SpatialFilterSet() {
		reset();
	}
	CAT_INLINE virtual ~SpatialFilterSet() {
	}

	// Set filters back to defaults
	void reset();

	// Choose a linear tapped filter from the FILTER_TAPS set over default
	void replace(int defaultIndex, int tappedIndex);

	CAT_INLINE Functions get(int index) {
		return _filters[index];
	}
};


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
};

typedef void (*RGB2YUVFilterFunction)(const u8 rgb[3], u8 yuv[3]);
typedef void (*YUV2RGBFilterFunction)(const u8 yuv[3], u8 out[3]);

extern RGB2YUVFilterFunction RGB2YUV_FILTERS[];
extern YUV2RGBFilterFunction YUV2RGB_FILTERS[];

const char *GetColorFilterString(int cf);


//// Chaos

extern const u8 CHAOS_TABLE_1[512];
extern const u8 CHAOS_TABLE_8[512];
extern const u8 CHAOS_SCORE[256];


} // namespace cat

#endif // FILTERS_HPP
