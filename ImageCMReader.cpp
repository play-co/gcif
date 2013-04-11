#include "ImageCMReader.hpp"
#include "Filters.hpp"
using namespace cat;


//// ImageCMReader

void ImageCMReader::clear() {
	if (_chaos) {
		delete []_chaos;
		_chaos = 0;
	}
	if (_rgba) {
		delete []_rgba;
		_rgba = 0;
	}
	if (_filters) {
		delete []_filters;
		_filters = 0;
	}
}

int ImageCMReader::init(const ImageInfo *info) {
	clear();

	_width = info->width;
	_height = info->height;

	// Validate input dimensions
	if (info->width < FILTER_ZONE_SIZE || info->height < FILTER_ZONE_SIZE) {
		return RE_BAD_DIMS;
	}
	if (info->width % FILTER_ZONE_SIZE || info->height % FILTER_ZONE_SIZE) {
		return RE_BAD_DIMS;
	}

	_rgba = new u8[_width * _height * 4];

	return RE_OK;
}

int ImageCMReader::readFilters(ImageReader &reader) {
	_filters = new u8[(_width >> FILTER_ZONE_SIZE_SHIFT) * (_height >> FILTER_ZONE_SIZE)];

	const int HUFF_TABLE_SIZE = 256;

	// For each table entry,
	for (int ii = 0; ii < HUFF_TABLE_SIZE; ++ii) {
		// Read codelen
		u32 len = reader.readBits(4);

		// If codelen is extended,
		if (len >= 15) {
			// Can only be extended once
			len += reader.readBit();
		}
	}

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

	u16 trigger_y = _lz->getTriggerY();
	u16 trigger_x = _lz->getTriggerX();

	for (int y = 0; y < _height; ++y) {
		// If LZ triggered,
		if (y == trigger_y) {
			_lz->triggerY();

			trigger_x = _lz->getTriggerX();
			trigger_y = _lz->getTriggerY();
		}

		u8 left_rgb[3] = {0};
		u8 *last_chaos_read = last_chaos + 3;

		for (int x = 0; x < width; ++x) {
			// If LZ triggered,
			if (x == trigger_x) {
				int skip = _lz->triggerX(now);

				trigger_x = _lz->getTriggerX();
				trigger_y = _lz->getTriggerY();

				x += skip - 1;
				continue;
			}

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

			// If not fully transparent,
			if (_mask->hasRGB(x, y)) {
				now[0] = 0;
				now[1] = 0;
				now[2] = 0;
				now[3] = 0;
			} else {
				for (int ii = 0; ii < 3; ++ii) {
					now[ii] = reader.nextHuffmanSymbol(&_decoder[ii][chaos[ii]]);
				}

				// Reverse color filter
				int cf = 0;
				u8 rgb[3];
				convertYUVtoRGB(cf, now, rgb);

				// Reverse spatial filter
				int sf = 0;
				const u8 *pred = spatialFilterPixel(now, sf, x, y, _width);

				// Write decoded RGB value
				now[0] = rgb[0] + pred[0];
				now[1] = rgb[1] + pred[1];
				now[2] = rgb[2] + pred[2];
				now[3] = 255;
			}

			left_rgb[0] = chaosScore(now[0]);
			left_rgb[1] = chaosScore(now[1]);
			left_rgb[2] = chaosScore(now[2]);

			last_chaos_read[0] = chaos[0] >> 1;
			last_chaos_read[1] = chaos[1] >> 1;
			last_chaos_read[2] = chaos[2] >> 1;
			last_chaos_read += 3;

			now += 4;
		}
	}

	return RE_OK;
}

int ImageCMReader::read(ImageReader &reader, ImageMaskReader &maskReader, ImageLZReader &lzReader) {
	int err;

	_mask = &maskReader;
	_lz = &lzReader;

	// Initialize
	if ((err = init(reader.getImageInfo()))) {
		return err;
	}

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

