#include "ImageCMReader.hpp"
using namespace cat;


//// ImageCMReader

void ImageCMReader::clear() {
}

int ImageCMReader::init(const ImageInfo *info) {
	return RE_OK;
}

int ImageCMReader::readFilters(ImageReader &reader) {
	return RE_OK;
}

int ImageCMReader::readTables(ImageReader &reader) {
	return RE_OK;
}

int ImageCMReader::readRGB(ImageReader &reader) {
	const int width = _width;

	u8 *last_chaos = _chaos;
	CAT_CLR(last_chaos, width * 3 + 3);

	u8 *last = _rgba;
	u8 *now = _rgba;

	for (int y = 0; y < _height; ++y) {
		u8 left_rgb[3] = {0};
		u8 *last_chaos_read = last_chaos + 3;

		for (int x = 0; x < width; ++x) {
			u16 chaos[3] = {
				left_rgb[0],
				left_rgb[1],
				left_rgb[2]
			};
			if (y > 0) {
				chaos[0] += chaosScore(last[0]);
				chaos[1] += chaosScore(last[1]);
				chaos[2] += chaosScore(last[2]);
				last += 4;
			}

			chaos[0] = CHAOS_TABLE[chaos[0]];
			chaos[1] = CHAOS_TABLE[chaos[1]];
			chaos[2] = CHAOS_TABLE[chaos[2]];
#ifdef FUZZY_CHAOS
			u16 isum = last_chaos_read[0] + last_chaos_read[-3];
			chaos[0] += (isum + chaos[0] + (isum >> 1)) >> 2;
			chaos[1] += (last_chaos_read[1] + last_chaos_read[-2] + chaos[1] + (chaos[0] >> 1)) >> 2;
			chaos[2] += (last_chaos_read[2] + last_chaos_read[-1] + chaos[2] + (chaos[1] >> 1)) >> 2;
#else
			//chaos[1] = (chaos[1] + chaos[0]) >> 1;
			//chaos[2] = (chaos[2] + chaos[1]) >> 1;
#endif
			left_rgb[0] = chaosScore(now[0]);
			left_rgb[1] = chaosScore(now[1]);
			left_rgb[2] = chaosScore(now[2]);

			if (!_lz->visited(x, y) && !_mask->hasRGB(x, y)) {
				for (int ii = 0; ii < 3; ++ii) {
					int bits = _encoder[ii][chaos[ii]].encode(now[ii], writer);
#ifdef CAT_COLLECT_STATS
					bitcount[ii] += bits;
#endif
				}
#ifdef CAT_COLLECT_STATS
				chaos_count++;
#endif
			}

			last_chaos_read[0] = chaos[0] >> 1;
			last_chaos_read[1] = chaos[1] >> 1;
			last_chaos_read[2] = chaos[2] >> 1;
			last_chaos_read += 3;

			now += 4;
		}
	}

	return RE_OK;
}

int ImageCMReader::read(ImageReader &reader) {
	int err;

	// Read filter selection
	if ((err = readFilters(reader))) {
		return err;
	}

	// Read Huffman tables for each RGB channel and chaos level
	if ((err = readTables(reader))) {
		return err;
	}

	// Read RGB data and decompress it
	if ((err = readRGB(reader))) {
		return err;
	}

	return RE_OK;
}

#ifdef CAT_COLLECT_STATS

bool ImageCMReader::dumpStats() {
}

#endif

