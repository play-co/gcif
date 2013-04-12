#ifndef IMAGE_MASK_READER_HPP
#define IMAGE_MASK_READER_HPP

#include "Platform.hpp"
#include "ImageReader.hpp"
#include "Filters.hpp"

/*
 * Game Closure Fully-Transparent Alpha Mask Decompression
 */

namespace cat {


//// ImageMaskReader

class ImageMaskReader {
	u32 *_mask;
	int _width;
	int _height;
	int _stride;

	u8 *_lz;

	int _sum, _lastSum;
	int _rowLeft;
	bool _rowStarted;
	u32 *_row;

	int _bitOffset;
	bool _bitOn;
	int _writeRow;

	bool decodeRLE(u8 *rle, int len);
	bool decodeLZ(HuffmanDecoder &decoder, ImageReader &reader);
	bool readHuffmanCodelens(u8 codelens[256], ImageReader &reader);
	void clear();

	int init(const ImageHeader *header);

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		double initUsec, readCodelensUsec, initHuffmanUsec;
		double lzUsec, rleUsec;
		double overallUsec;

		u32 originalDataBytes;
	} Stats;
#endif // CAT_COLLECT_STATS

public:
	ImageMaskReader() {
		_mask = 0;
		_lz = 0;
	}
	virtual ~ImageMaskReader() {
		clear();
	}

	int read(ImageReader &reader);

	CAT_INLINE bool hasRGB(int x, int y) {
#ifdef LOWRES_MASK
		const int maskX = x >> FILTER_ZONE_SIZE_SHIFT;
		const int maskY = y >> FILTER_ZONE_SIZE_SHIFT;
#else
		const int maskX = x;
		const int maskY = y;
#endif
		const u32 word = _mask[(maskX >> 5) + maskY * _stride];
		return (word >> (maskX & 31)) & 1;
	}

#ifdef CAT_COLLECT_STATS
	bool dumpStats();
#else
	CAT_INLINE bool dumpStats() {
		// Not implemented
		return false;
	}
#endif
};


} // namespace cat

#endif // IMAGE_MASK_READER_HPP

