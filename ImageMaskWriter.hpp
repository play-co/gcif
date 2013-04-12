#ifndef IMAGE_MASK_WRITER_HPP
#define IMAGE_MASK_WRITER_HPP

#include "Platform.hpp"
#include "ImageWriter.hpp"
#include "Filters.hpp"

#include <vector>

/*
 * Game Closure Fully-Transparent Alpha Mask Compression
 *
 * Encodes pixels with fully-transparent alpha as a monochrome bitmap.
 *
 * It first performs bitwise filtering to reduce the data down to a few pixels.
 * Then the distance between those pixels is recorded, compressed with LZ4HC,
 * and then Huffman encoded.
 */

namespace cat {


//// ImageMaskWriter

class ImageMaskWriter {
	u32 _value;

	u32 *_mask;
	u32 *_filtered;
	int _size, _stride, _width, _height;

	void clear();

	void applyFilter();
	void performRLE(std::vector<u8> &rle);
	void performLZ(const std::vector<u8> &rle, std::vector<u8> &lz);
	void writeHuffmanTable(u8 codelens[256], ImageWriter &writer);
	void writeEncodedLZ(const std::vector<u8> &lz, u16 codes[256], u8 codelens[256], ImageWriter &writer);

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		u32 pivot;
		u32 table_bits;
		u32 data_bits;

		int rleBytes, lzBytes;

		double filterUsec, rleUsec, lzUsec, histogramUsec;
		double generateTableUsec, tableEncodeUsec, dataEncodeUsec;
		double overallUsec;

		int compressedDataBits;
		int originalDataBytes, compressedDataBytes;

		double compressionRatio;
	} Stats;
#endif // CAT_COLLECT_STATS

public:
	ImageMaskWriter() {
		_mask = 0;
		_filtered = 0;
	}
	virtual ~ImageMaskWriter() {
		clear();
	}

	int initFromRGBA(const u8 *rgba, int width, int height);

	void write(ImageWriter &writer);

	CAT_INLINE bool hasRGB(int x, int y) {
#ifdef LOWRES_MASK
		const int maskX = x >> FILTER_ZONE_SIZE_SHIFT;
		const int maskY = y >> FILTER_ZONE_SIZE_SHIFT;
#else
		const int maskX = x;
		const int maskY = y;
#endif
		const u32 word = _mask[(maskX >> 5) + maskY * _stride];
		return (word << (maskX & 31)) >> 31;
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

#endif // IMAGE_MASK_WRITER_HPP

