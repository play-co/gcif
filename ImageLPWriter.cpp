#include "ImageLPWriter.hpp"
#include "GCIFWriter.hpp"
using namespace cat;


//// ImageLPWriter

void ImageLPWriter::clear() {
}

bool ImageLPWriter::checkMatch(u16 x, u16 y, u16 w, u16 h) {
}

bool ImageLPWriter::expandMatch(u16 &x, u16 &y, u16 &w, u16 &h) {
}

void ImageLPWriter::add(u16 x, u16 y, u16 w, u16 h) {
}

int ImageLPWriter::match() {
	return WE_OK;
}

int ImageLPWriter::initFromRGBA(const u8 *rgba, int width, int height, ImageMaskWriter &mask, ImageLZWriter &lz) {
	clear();

	if (width < ZONE || height < ZONE) {
		return WE_BAD_DIMS;
	}

	_rgba = rgba;
	_width = width;
	_height = height;

	_mask = &mask;
	_lz = &lz;

	const int visited_size = (width * height + 31) / 32;
	_visited = new u32[visited_size];
	memset(_visited, 0, visited_size * sizeof(u32));

	return match();
}

void ImageLPWriter::write(ImageWriter &writer) {
}

#ifdef CAT_COLLECT_STATS

bool ImageLPWriter::dumpStats() {
	return true;
}

#endif // CAT_COLLECT_STATS

