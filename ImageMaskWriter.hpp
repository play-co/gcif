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
	int _size, _stride, _height;

	void clear();

	void applyFilter();
	void performRLE(std::vector<u8> &rle);
	void performLZ(const std::vector<u8> &rle, std::vector<u8> &lz);
	void collectFreqs(const std::vector<u8> &lz, u16 freqs[256]);
	void generateHuffmanCodes(u16 freqs[256], u16 codes[256], u8 codelens[256]);
	void writeHuffmanTable(u8 codelens[256], ImageWriter &writer);
	void writeEncodedLZ(const std::vector<u8> &lz, u16 codes[256], u8 codelens[256], ImageWriter &writer);

public:
	ImageMaskWriter() {
		_stride = 0;
		_mask = 0;
	}
	virtual ~ImageMaskWriter() {
		clear();
	}

	bool initFromRGBA(u8 *rgba, int width, int height);

	void write(ImageWriter &writer);

	CAT_INLINE bool hasRGB(int x, int y) {
		const u32 word = _mask[(x >> 5) + y * _stride];
		return (word >> (x & 31)) & 1;
	}
};


} // namespace cat

#endif // IMAGE_MASK_WRITER_HPP

