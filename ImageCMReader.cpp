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

int ImageCMReader::init(GCIFImage *image) {
	clear();

	_width = image->width;
	_height = image->height;

	// Validate input dimensions
	if (_width < FILTER_ZONE_SIZE || _height < FILTER_ZONE_SIZE) {
		return RE_BAD_DIMS;
	}
	if (_width % FILTER_ZONE_SIZE || _height % FILTER_ZONE_SIZE) {
		return RE_BAD_DIMS;
	}

	_rgba = new u8[_width * _height * 4];

	// Fill in image pointer
	image->rgba = _rgba;

	// Just need to remember the last row of filters
	_filters = new u8[_width >> FILTER_ZONE_SIZE_SHIFT];

	// And last row of chaos data
	_chaos = new u8[_width * 3];

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

	// Initialize previous chaos scanline to zero
	CAT_CLR(_chaos, width * 3);

	// Get initial triggers
	u16 trigger_y = _lz->getTriggerY();
	u16 trigger_x = _lz->getTriggerX();

	// Start from upper-left of image
	u8 *p = _rgba;

	// For each scanline,
	for (int y = 0; y < _height; ++y) {
		// If LZ triggered,
		if (y == trigger_y) {
			_lz->triggerY();
			trigger_x = _lz->getTriggerX();
			trigger_y = _lz->getTriggerY();
		}

		// Restart for scanline
		u8 left[3] = {0};
		u8 *last = _chaos;
		u8 sf = 0, cf = 0;

		// For each pixel,
		for (int x = 0; x < width; ++x) {
			// If LZ triggered,
			if (x == trigger_x) {
				// Trigger LZ
				int skip = _lz->triggerX(p);
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

				p += skip * 4;
				last += skip * 3;
				x += skip - 1; // -1 offset from for loop increment
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

			// If fully transparent,
			if (_mask->hasRGB(x, y)) {
				// Write empty pixel
				p[0] = 0;
				p[1] = 0;
				p[2] = 0;
				p[3] = 0;
			} else {
				// Read YUV filtered pixel
				u8 yuv[3];
				for (int c = 0; c < 3; ++c) {
					u8 chaos = CHAOS_TABLE[left[c] + (u16)last[c]];
					u8 value = reader.nextHuffmanSymbol(&_decoder[c][chaos]);
					left[c] = last[c] = yuv[c] = value;
				}

				// Reverse color filter
				u8 rgb[3];
				convertYUVtoRGB(cf, yuv, rgb);

				// Reverse spatial filter
				const u8 *pred = spatialFilterPixel(p, sf, x, y, width);
				p[0] = rgb[0] + pred[0];
				p[1] = rgb[1] + pred[1];
				p[2] = rgb[2] + pred[2];
				p[3] = 255;
			}

			// Next pixel
			last += 3;
			p += 4;
		}
	}

	return RE_OK;
}

int ImageCMReader::read(ImageReader &reader, ImageMaskReader &maskReader, ImageLZReader &lzReader, GCIFImage *image) {
	int err;

	_mask = &maskReader;
	_lz = &lzReader;

	// Initialize
	if ((err = init(image))) {
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

	// Pass image data reference back to caller
	_rgba = 0;

	return RE_OK;
}

#ifdef CAT_COLLECT_STATS

bool ImageCMReader::dumpStats() {
}

#endif

