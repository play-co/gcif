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

	// Just need to remember the last row of filters
	_filters = new u8[_width >> FILTER_ZONE_SIZE_SHIFT];

	return RE_OK;
}

int ImageCMReader::readFilterTables(ImageReader &reader) {
	// Read spatial filter codelens
	u8 sf_codelens[SF_COUNT];

	// For each table entry,
	for (int ii = 0; ii < SF_COUNT; ++ii) {
		// Read codelen
		u32 len = reader.readBits(4);

		// If codelen is extended,
		if (len >= 15) {
			// Can only be extended once
			len += reader.readBit();
		}

		sf_codelens[ii] = len;
	}

	// Initialize huffman decoder
	if (reader.eof() || !_sf.init(SF_COUNT, sf_codelens, 8)) {
		return RE_CM_CODES;
	}

	// Read color filter codelens
	u8 cf_codelens[CF_COUNT];

	// For each table entry,
	for (int ii = 0; ii < CF_COUNT; ++ii) {
		// Read codelen
		u32 len = reader.readBits(4);

		// If codelen is extended,
		if (len >= 15) {
			// Can only be extended once
			len += reader.readBit();
		}

		cf_codelens[ii] = len;
	}

	// Initialize huffman decoder
	if (reader.eof() || !_cf.init(CF_COUNT, cf_codelens, 8)) {
		return RE_CM_CODES;
	}

	return RE_OK;
}

int ImageCMReader::readChaosTables(ImageReader &reader) {
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

	// For each scanline,
	for (int y = 0; y < _height; ++y) {
		// If LZ triggered,
		if (y == trigger_y) {
			_lz->triggerY();

			trigger_x = _lz->getTriggerX();
			trigger_y = _lz->getTriggerY();
		}

		u8 left_rgb[3] = {0};
		u8 *last_chaos_read = last_chaos + 3;

		u8 sf = 0, cf = 0;

		// For each pixel,
		for (int x = 0; x < width; ++x) {
			// If LZ triggered,
			if (x == trigger_x) {
				// Trigger LZ
				int skip = _lz->triggerX(now);
				trigger_x = _lz->getTriggerX();
				trigger_y = _lz->getTriggerY();

				// If we are on a filter info scanline,
				if ((y & FILTER_ZONE_SIZE_MASK) == 0) {
					// For each pixel we skipped,
					for (int ii = x, iiend = x + skip; ii < iiend; ++ii) {
						// If a filter would be there,
						if ((ii & FILTER_ZONE_SIZE_MASK) == 0) {
							// Read SF and CF for this zone
							sf = reader.nextHuffmanSymbol(&_sf);
							cf = reader.nextHuffmanSymbol(&_cf);
							_filters[ii >> FILTER_ZONE_SIZE_SHIFT] = (sf << 4) | cf;
						}
					}
				}

				x += skip - 1; // Offset from end of for loop increment
				continue;
			}

			// If it is time to read the filter,
			if ((x & FILTER_ZONE_SIZE_MASK) == 0) {
				// If we are on a filter info scanline,
				if ((y & FILTER_ZONE_SIZE_MASK) == 0) {
					// Read SF and CF for this zone
					sf = reader.nextHuffmanSymbol(&_sf);
					cf = reader.nextHuffmanSymbol(&_cf);
					_filters[x >> FILTER_ZONE_SIZE_SHIFT] = (sf << 4) | cf;
				} else {
					// Read filter from previous scanline
					u8 filter = _filters[x >> FILTER_ZONE_SIZE_SHIFT];
					sf = filter >> 4;
					cf = filter & 15;
				}
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

				left_rgb[0] = chaosScore(now[0]);
				left_rgb[1] = chaosScore(now[1]);
				left_rgb[2] = chaosScore(now[2]);

				// Reverse color filter
				u8 rgb[3];
				convertYUVtoRGB(cf, now, rgb);

				// Reverse spatial filter
				const u8 *pred = spatialFilterPixel(now, sf, x, y, _width);

				// Write decoded RGB value
				now[0] = rgb[0] + pred[0];
				now[1] = rgb[1] + pred[1];
				now[2] = rgb[2] + pred[2];
				now[3] = 255;
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

int ImageCMReader::read(ImageReader &reader, ImageMaskReader &maskReader, ImageLZReader &lzReader) {
	int err;

	_mask = &maskReader;
	_lz = &lzReader;

	// Initialize
	if ((err = init(reader.getImageInfo()))) {
		return err;
	}

	// Read filter selection tables
	if ((err = readFilterTables(reader))) {
		return err;
	}

	// Read Huffman tables for each RGB channel and chaos level
	if ((err = readChaosTables(reader))) {
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

