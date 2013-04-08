#ifndef IMAGE_FILTER_WRITER_HPP
#define IMAGE_FILTER_WRITER_HPP

#include "Platform.hpp"
#include "ImageWriter.hpp"
#include "ImageMaskWriter.hpp"

namespace cat {


static const int FILTER_ZONE_SIZE = 4;
static const int FILTER_RLE_SYMS = 8;
static const int FILTER_MATCH_FUZZ = 20;

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


//#define CAT_FILTER_LZ


//// ImageFilterWriter

class ImageFilterWriter {
	int _w, _h;
	u16 *_matrix;
	u8 *_chaos;

	std::vector<u8> _lz;

	u8 *_lz_mask;

	void clear();

	u8 *_rgba;
	int _width;
	int _height;
	ImageMaskWriter *_mask;

#ifdef CAT_FILTER_LZ
	CAT_INLINE bool hasR(int x, int y) {
		return _lz_mask[(x + y * _width) * 3];
	}

	CAT_INLINE bool hasG(int x, int y) {
		return _lz_mask[(x + y * _width) * 3 + 1];
	}

	CAT_INLINE bool hasB(int x, int y) {
		return _lz_mask[(x + y * _width) * 3 + 2];
	}

	CAT_INLINE bool hasPixel(int x, int y) {
		return hasR(x, y) || hasG(x, y) || hasB(x, y);
	}
#endif

	bool init(int width, int height);
#ifdef CAT_FILTER_LZ
	void makeLZmask();
#endif
	void decideFilters();
	void applyFilters();
	void chaosEncode();

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
#ifdef CAT_FILTER_LZ
		u32 lz_lit_len_ov, lz_token_ov, lz_offset_ov, lz_match_len_ov, lz_overall_ov;
		u32 lz_huff_bits;
#endif

		int rleBytes, lzBytes;

		double filterUsec, rleUsec, lzUsec, histogramUsec;
		double generateTableUsec, tableEncodeUsec, dataEncodeUsec;
		double overallUsec;

		int originalDataBytes, compressedDataBytes;

		double compressionRatio;
	} Stats;
#endif // CAT_COLLECT_STATS

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
