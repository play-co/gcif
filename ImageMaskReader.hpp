#ifndef IMAGE_MASK_READER_HPP
#define IMAGE_MASK_READER_HPP

#include "Platform.hpp"
#include "ImageReader.hpp"

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

	bool init(const ImageInfo *info);

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		int pivot;

		double initUsec, readCodelensUsec, initHuffmanUsec;
		double lzUsec, rleUsec;
		double overallUsec;

		u32 originalDataBytes, compressedDataBytes;
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
		const u32 word = _mask[(x >> 5) + y * _stride];
		return (word >> (x & 31)) & 1;
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

