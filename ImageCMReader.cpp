#include "ImageCMReader.hpp"
using namespace cat;

#ifdef CAT_COLLECT_STATS
#include "Log.hpp"
#include "Clock.hpp"

static cat::Clock *m_clock = 0;

#include <iostream>
using namespace std;

#endif // CAT_COLLECT_STATS


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
	_filters = new FilterSelection[_width >> FILTER_ZONE_SIZE_SHIFT];

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
	// For each color plane,
	for (int ii = 0; ii < 3; ++ii) {
		// For each chaos level,
		for (int jj = 0; jj < CHAOS_LEVELS; ++jj) {
			// Read the decoder table
			if (!_decoder[ii][jj].init(reader)) {
				return RE_CM_CODES;
			}
		}
	}

	return RE_OK;
}

int ImageCMReader::readRGB(ImageReader &reader) {
	const int width = _width;

	// Initialize previous chaos scanline to zero
	CAT_CLR(_chaos, width * 3);

	// Get initial triggers
	u16 trigger_y_lz = _lz->getTriggerY();
	u16 trigger_x_lz = _lz->getTriggerX();
	u16 trigger_y_lp = _lp->getTriggerY();
	u16 trigger_x_lp = _lp->getTriggerX();

	// Start from upper-left of image
	u8 *p = _rgba;

	// For each scanline,
	for (int y = 0; y < _height; ++y) {
		// If LZ triggered,
		if (y == trigger_y_lz) {
			_lz->triggerY();
			trigger_x_lz = _lz->getTriggerX();
			trigger_y_lz = _lz->getTriggerY();
		}
		// If LP triggered,
		if (y == trigger_y_lp) {
			_lp->triggerY();
			trigger_x_lp = _lp->getTriggerX();
			trigger_y_lp = _lp->getTriggerY();
		}

		// Restart for scanline
		u8 left[3] = {0};
		u8 *last = _chaos;
		SpatialFilterFunction sf;
		YUV2RGBFilterFunction cf;
		int lz_skip = 0, lp_skip = 0;

		// For each pixel,
		for (int x = 0; x < width; ++x) {
			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerX(p);
				trigger_x_lz = _lz->getTriggerX();
				trigger_y_lz = _lz->getTriggerY();
			}
			// If LP triggered,
			if (x == trigger_x_lp) {
				lp_skip = _lz->triggerX(p);
				trigger_x_lp = _lp->getTriggerX();
				trigger_y_lp = _lp->getTriggerY();
			}

			// If it is time to read the filter,
			if ((x & FILTER_ZONE_SIZE_MASK) == 0) {
				FilterSelection *filter = &_filters[x >> FILTER_ZONE_SIZE_SHIFT];

				// If we are on a filter info scanline,
				if ((y & FILTER_ZONE_SIZE_MASK) == 0) {
					// Read SF and CF for this zone
					filter->sf = sf = SPATIAL_FILTERS[reader.nextHuffmanSymbol(&_sf)];
					filter->cf = cf = YUV2RGB_FILTERS[reader.nextHuffmanSymbol(&_cf)];
				} else {
					// Read filter from previous scanline
					sf = filter->sf;
					cf = filter->cf;
				}
			}

			// If fully transparent,
			if (lz_skip > 0) {
				// Record a zero here
				for (int c = 0; c < 3; ++c) {
					left[c] = last[c] = 0;
				}
				--lz_skip;
			} else if (lp_skip > 0) {
				// Record a zero here
				for (int c = 0; c < 3; ++c) {
					left[c] = last[c] = 0;
				}
				--lp_skip;
			} else if (_mask->hasRGB(x, y)) {
				// Write empty pixel
				p[0] = 0;
				p[1] = 0;
				p[2] = 0;
				p[3] = 0;
				for (int c = 0; c < 3; ++c) {
					left[c] = last[c] = 0;
				}
			} else {
				// Read YUV filtered pixel
				u8 yuv[3];

				left[0] = last[0] = yuv[0] = _decoder[0][CHAOS_TABLE[left[0] + (u16)last[0]]].next(reader);
				left[1] = last[1] = yuv[1] = _decoder[1][CHAOS_TABLE[left[1] + (u16)last[1]]].next(reader);
				left[2] = last[2] = yuv[2] = _decoder[2][CHAOS_TABLE[left[2] + (u16)last[2]]].next(reader);

				// Reverse color filter
				u8 rgb[3];
				cf(yuv, rgb);

				// Reverse spatial filter
				const u8 *pred = sf(p, x, y, width);
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

int ImageCMReader::read(ImageReader &reader, ImageMaskReader &maskReader, ImageLZReader &lzReader, ImageLPReader &lpReader, GCIFImage *image) {
#ifdef CAT_COLLECT_STATS
	m_clock = Clock::ref();

	double t0 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	int err;

	_mask = &maskReader;
	_lz = &lzReader;
	_lp = &lpReader;

	// Initialize
	if ((err = init(image))) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t1 = m_clock->usec();
#endif	

	// Read filter selection tables
	if ((err = readFilterTables(reader))) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t2 = m_clock->usec();
#endif	

	// Read Huffman tables for each RGB channel and chaos level
	if ((err = readChaosTables(reader))) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t3 = m_clock->usec();
#endif	

	// Read RGB data and decompress it
	if ((err = readRGB(reader))) {
		return err;
	}

	// Pass image data reference back to caller
	_rgba = 0;


#ifdef CAT_COLLECT_STATS
	double t4 = m_clock->usec();

	Stats.initUsec = t1 - t0;
	Stats.readFilterTablesUsec = t2 - t1;
	Stats.readChaosTablesUsec = t3 - t2;
	Stats.readRGBUsec = t4 - t3;
	Stats.overallUsec = t4 - t0;
#endif	
	return RE_OK;
}

#ifdef CAT_COLLECT_STATS

bool ImageCMReader::dumpStats() {
	CAT_INFO("stats") << "(CM Decode)     Initialization : " << Stats.initUsec << " usec (" << Stats.initUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INFO("stats") << "(CM Decode) Read Filter Tables : " << Stats.readFilterTablesUsec << " usec (" << Stats.readFilterTablesUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INFO("stats") << "(CM Decode)  Read Chaos Tables : " << Stats.readChaosTablesUsec << " usec (" << Stats.readChaosTablesUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INFO("stats") << "(CM Decode)           Read RGB : " << Stats.readRGBUsec << " usec (" << Stats.readRGBUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INFO("stats") << "(CM Decode)            Overall : " << Stats.overallUsec << " usec";

	CAT_INFO("stats") << "(CM Decode)         Throughput : " << (_width * _height * 4) / Stats.overallUsec << " MBPS (output bytes/time)";

	return true;
}

#endif

