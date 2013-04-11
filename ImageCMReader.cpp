#include "ImageCMReader.hpp"
using namespace cat;


//// ImageCMReader

void ImageCMReader::clear() {
}

int ImageCMReader::init(const ImageInfo *info) {
	return RE_OK;
}

int ImageCMReader::readTables(ImageReader &reader) {
	return RE_OK;
}

int ImageCMReader::read(ImageReader &reader) {
	return RE_OK;
}

#ifdef CAT_COLLECT_STATS

bool ImageCMReader::dumpStats() {
}

#endif

