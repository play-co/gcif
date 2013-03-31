#ifndef IMAGE_MASK_WRITER_HPP
#define IMAGE_MASK_WRITER_HPP

#include "Platform.hpp"
#include "ImageWriter.hpp"

#include <vector>

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
	void collectFreqs(const std::vector<u8> &lz, u16 freqs[256]);
	void generateHuffmanCodes(u16 freqs[256], u16 codes[256], u8 codelens[256]);
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

	int initFromRGBA(u8 *rgba, int width, int height);

	void write(ImageWriter &writer);

	CAT_INLINE bool hasRGB(int x, int y) {
		const u32 word = _mask[(x >> 5) + y * _stride];
		return (word << (x & 31)) >> 31;
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

