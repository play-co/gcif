#ifndef IMAGE_MASK_READER_HPP
#define IMAGE_MASK_READER_HPP

#include "Platform.hpp"
#include "ImageReader.hpp"

#include <vector>

namespace cat {


//// ImageMaskReader

class ImageMaskReader {

	u32 *_mask;
	int _size;

	void clear();

public:
	ImageMaskReader() {
	}
	virtual ~ImageMaskReader() {
		clear();
	}

	void read(ImageReader &reader);

	CAT_INLINE bool hasRGB(int x, int y) {
		const u32 word = _mask[(x >> 5) + y * _stride];
		return (word >> (x & 31)) & 1;
	}
};


} // namespace cat

#endif // IMAGE_MASK_READER_HPP

