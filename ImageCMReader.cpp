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
	_chaos_size = (_width + RECENT_SYMS) * PLANES;
	_chaos = new u8[_chaos_size];

	return RE_OK;
}

int ImageCMReader::readFilterTables(ImageReader &reader) {
	// Initialize huffman decoder
	if (reader.eof() || !_sf.init(SF_COUNT, reader, 8)) {
		return RE_CM_CODES;
	}

	// Initialize huffman decoder
	if (reader.eof() || !_cf.init(CF_COUNT, reader, 8)) {
		return RE_CM_CODES;
	}

	return RE_OK;
}

int ImageCMReader::readChaosTables(ImageReader &reader) {
	// For each color plane,
	for (int ii = 0; ii < PLANES; ++ii) {
		// For each chaos level,
		for (int jj = 0; jj < CHAOS_LEVELS; ++jj) {
			CAT_WARN("TABLE") << ii << " : " << jj;
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

	// Get initial triggers
	u16 trigger_y_lz = _lz->getTriggerY();
	u16 trigger_x_lz = _lz->getTriggerX();

	// Start from upper-left of image
	u8 *p = _rgba;
	u8 *lastStart = _chaos + RECENT_SYMS * PLANES;
	CAT_CLR(_chaos, _chaos_size);

	// Unroll y = 0 scanline
	{
		const int y = 0;

		// If LZ triggered,
		if (y == trigger_y_lz) {
			_lz->triggerY();
			trigger_x_lz = _lz->getTriggerX();
			trigger_y_lz = _lz->getTriggerY();
		}

		// Restart for scanline
		u8 *last = lastStart;
		SpatialFilterFunction sf;
		YUV2RGBFilterFunction cf;
		int lz_skip = 0;

		// For each pixel,
		for (int x = 0; x < width; ++x) {
			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerX(p);
				trigger_x_lz = _lz->getTriggerX();
				trigger_y_lz = _lz->getTriggerY();
			}

			// If it is time to read the filter,
			if ((x & FILTER_ZONE_SIZE_MASK) == 0) {
				// If at least one pixel requires these filters,
				for (int ii = 0; ii < FILTER_ZONE_SIZE; ++ii) {
					for (int jj = 0; jj < FILTER_ZONE_SIZE; ++jj) {
						if (!_mask->hasRGB(x + jj, y + ii)) {
							// Read SF and CF for this zone
							FilterSelection *filter = &_filters[x >> FILTER_ZONE_SIZE_SHIFT];

							u8 sfi = _sf.next(reader);
							filter->sf = sf = SPATIAL_FILTERS[sfi];
							filter->sfu = UNSAFE_SPATIAL_FILTERS[sfi];
							u8 cfi = _cf.next(reader);
							filter->cf = cf = YUV2RGB_FILTERS[cfi];
							goto y0_had_filter;
						}
					}
				}
y0_had_filter:;
			}

			if (lz_skip > 0) {
				--lz_skip;

				for (int c = 0; c < PLANES; ++c) {
					last[c] = 0;
				}
			} else if (_mask->hasRGB(x, y)) {
				// Fully-transparent pixel
				u32 *zp = reinterpret_cast<u32 *>( p );
				*zp = 0;

				for (int c = 0; c < PLANES; ++c) {
					last[c] = 0;
				}
			} else {
				// Read YUV filtered pixel
				u8 yuv[3], a;

				u32 y = _decoder[0][CHAOS_TABLE[chaosScore(last[-4])]].next(reader);

				// If pixel is a recent palette entry,
				if (y >= 256) {
					u8 *src = p - (y - 256 - RECENT_AHEAD) * 4;
					last[0] = yuv[0] = src[0];
					last[1] = yuv[1] = src[1];
					last[2] = yuv[2] = src[2];
					last[3] = a = src[3];
				} else {
					last[0] = yuv[0] = y;
					last[1] = yuv[1] = _decoder[1][CHAOS_TABLE[chaosScore(last[-3])]].next(reader);
					last[2] = yuv[2] = _decoder[2][CHAOS_TABLE[chaosScore(last[-2])]].next(reader);
					last[3] = a = _decoder[3][CHAOS_TABLE[chaosScore(last[-1])]].next(reader);
				}

				// Reverse color filter
				u8 rgb[3];
				cf(yuv, rgb);

				// Reverse spatial filter
				const u8 *pred = sf(p, x, y, width);
				p[0] = rgb[0] + pred[0];
				p[1] = rgb[1] + pred[1];
				p[2] = rgb[2] + pred[2];
				if (x > 0) {
					p[3] = p[-1] - a;
				} else {
					p[3] = 255 - a;
				}
			}

			// Next pixel
			last += PLANES;
			p += 4;
		}
	}


	// For each scanline,
	for (int y = 1; y < _height; ++y) {
		// If LZ triggered,
		if (y == trigger_y_lz) {
			_lz->triggerY();
			trigger_x_lz = _lz->getTriggerX();
			trigger_y_lz = _lz->getTriggerY();
		}

		// Restart for scanline
		u8 *last = lastStart;
		SpatialFilterFunction sf;
		YUV2RGBFilterFunction cf;
		int lz_skip = 0;

		// Unroll x = 0 pixel
		{
			const int x = 0;

			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerX(p);
				trigger_x_lz = _lz->getTriggerX();
				trigger_y_lz = _lz->getTriggerY();
			}

			FilterSelection *filter = &_filters[x >> FILTER_ZONE_SIZE_SHIFT];

			// If we are on a filter info scanline,
			if ((y & FILTER_ZONE_SIZE_MASK) == 0) {
				// If at least one pixel requires these filters,
				for (int ii = 0; ii < FILTER_ZONE_SIZE; ++ii) {
					for (int jj = 0; jj < FILTER_ZONE_SIZE; ++jj) {
						if (!_mask->hasRGB(x + jj, y + ii)) {
							// Read SF and CF for this zone
							FilterSelection *filter = &_filters[x >> FILTER_ZONE_SIZE_SHIFT];

							u8 sfi = _sf.next(reader);
							filter->sf = sf = SPATIAL_FILTERS[sfi];
							filter->sfu = UNSAFE_SPATIAL_FILTERS[sfi];
							u8 cfi = _cf.next(reader);
							filter->cf = cf = YUV2RGB_FILTERS[cfi];
							goto x0_had_filter;
						}
					}
				}
x0_had_filter:;
			} else {
				// Read filter from previous scanline
				sf = filter->sf;
				cf = filter->cf;
			}

			if (lz_skip > 0) {
				--lz_skip;

				for (int c = 0; c < PLANES; ++c) {
					last[c] = 0;
				}
			} else if (_mask->hasRGB(x, y)) {
				// Fully-transparent pixel
				u32 *zp = reinterpret_cast<u32 *>( p );
				*zp = 0;

				for (int c = 0; c < PLANES; ++c) {
					last[c] = 0;
				}
			} else {
				// Read YUV filtered pixel
				u8 yuv[3], a;

				u32 y = _decoder[0][CHAOS_TABLE[chaosScore(last[0])]].next(reader);

				// If pixel is a recent palette entry,
				if (y >= 256) {
					u8 *src = p - (y - 256 - RECENT_AHEAD) * 4;
					last[0] = yuv[0] = src[0];
					last[1] = yuv[1] = src[1];
					last[2] = yuv[2] = src[2];
					last[3] = a = src[3];
				} else {
					last[0] = yuv[0] = y;
					last[1] = yuv[1] = _decoder[1][CHAOS_TABLE[chaosScore(last[1])]].next(reader);
					last[2] = yuv[2] = _decoder[2][CHAOS_TABLE[chaosScore(last[2])]].next(reader);
					last[3] = a = _decoder[3][CHAOS_TABLE[chaosScore(last[3])]].next(reader);
				}

				// Reverse color filter
				u8 rgb[3];
				cf(yuv, rgb);

				// Reverse spatial filter
				const u8 *pred = sf(p, x, y, width);
				p[0] = rgb[0] + pred[0];
				p[1] = rgb[1] + pred[1];
				p[2] = rgb[2] + pred[2];
				p[3] = 255 - a;
			}

			// Next pixel
			last += PLANES;
			p += 4;
		}

		// For each pixel,  (THIS IS THE BIG INNER LOOP)
		for (int x = 1, xend = width - 1; x < xend; ++x) {
			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerX(p);
				trigger_x_lz = _lz->getTriggerX();
				trigger_y_lz = _lz->getTriggerY();
			}

			// If it is time to read the filter,
			if ((x & FILTER_ZONE_SIZE_MASK) == 0) {
				FilterSelection *filter = &_filters[x >> FILTER_ZONE_SIZE_SHIFT];

				// If we are on a filter info scanline,
				if ((y & FILTER_ZONE_SIZE_MASK) == 0) {
					// If at least one pixel requires these filters,
					for (int ii = 0; ii < FILTER_ZONE_SIZE; ++ii) {
						for (int jj = 0; jj < FILTER_ZONE_SIZE; ++jj) {
							if (!_mask->hasRGB(x + jj, y + ii)) {
								// Read SF and CF for this zone
								FilterSelection *filter = &_filters[x >> FILTER_ZONE_SIZE_SHIFT];

								u8 sfi = _sf.next(reader);
								filter->sf = SPATIAL_FILTERS[sfi];
								filter->sfu = sf = UNSAFE_SPATIAL_FILTERS[sfi];
								u8 cfi = _cf.next(reader);
								filter->cf = cf = YUV2RGB_FILTERS[cfi];
								goto had_filter;
							}
						}
					}
had_filter:;
				} else {
					// Read filter from previous scanline
					sf = filter->sfu;
					cf = filter->cf;
				}
			}

			if (lz_skip > 0) {
				--lz_skip;

				for (int c = 0; c < PLANES; ++c) {
					last[c] = 0;
				}
			} else if (_mask->hasRGB(x, y)) {
				// Fully-transparent pixel
				u32 *zp = reinterpret_cast<u32 *>( p );
				*zp = 0;

				for (int c = 0; c < PLANES; ++c) {
					last[c] = 0;
				}
			} else {
				// Read YUV filtered pixel
				u8 yuv[3], a;

				u32 y = _decoder[0][CHAOS_TABLE[chaosScore(last[-4]) + chaosScore(last[0])]].next(reader);

				// If pixel is a recent palette entry,
				if (y >= 256) {
					u8 *src = p - (y - 256 - RECENT_AHEAD) * 4;
					last[0] = yuv[0] = src[0];
					last[1] = yuv[1] = src[1];
					last[2] = yuv[2] = src[2];
					last[3] = a = src[3];
				} else {
					last[0] = yuv[0] = y;
					last[1] = yuv[1] = _decoder[1][CHAOS_TABLE[chaosScore(last[-3]) + chaosScore(last[1])]].next(reader);
					last[2] = yuv[2] = _decoder[2][CHAOS_TABLE[chaosScore(last[-2]) + chaosScore(last[2])]].next(reader);
					last[3] = a = _decoder[3][CHAOS_TABLE[chaosScore(last[-1]) + chaosScore(last[3])]].next(reader);
				}

				// Reverse color filter
				u8 rgb[3];
				cf(yuv, rgb);

				// Reverse spatial filter
				const u8 *pred = sf(p, x, y, width);
				p[0] = rgb[0] + pred[0];
				p[1] = rgb[1] + pred[1];
				p[2] = rgb[2] + pred[2];
				p[3] = p[-1] - a;
			}

			// Next pixel
			last += PLANES;
			p += 4;
		}

		// For x = width-1,
		{
			const int x = width - 1;

			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerX(p);
				trigger_x_lz = _lz->getTriggerX();
				trigger_y_lz = _lz->getTriggerY();
			}

			if (lz_skip > 0) {
				--lz_skip;

				for (int c = 0; c < PLANES; ++c) {
					last[c] = 0;
				}
			} else if (_mask->hasRGB(x, y)) {
				// Fully-transparent pixel
				u32 *zp = reinterpret_cast<u32 *>( p );
				*zp = 0;

				for (int c = 0; c < PLANES; ++c) {
					last[c] = 0;
				}
			} else {
				// Read YUV filtered pixel
				u8 yuv[3], a;

				u32 y = _decoder[0][CHAOS_TABLE[chaosScore(last[-4]) + chaosScore(last[0])]].next(reader);

				// If pixel is a recent palette entry,
				if (y >= 256) {
					u8 *src = p - (y - 256 - RECENT_AHEAD) * 4;
					last[0] = yuv[0] = src[0];
					last[1] = yuv[1] = src[1];
					last[2] = yuv[2] = src[2];
					last[3] = a = src[3];
				} else {
					last[0] = yuv[0] = y;
					last[1] = yuv[1] = _decoder[1][CHAOS_TABLE[chaosScore(last[-3]) + chaosScore(last[1])]].next(reader);
					last[2] = yuv[2] = _decoder[2][CHAOS_TABLE[chaosScore(last[-2]) + chaosScore(last[2])]].next(reader);
					last[3] = a = _decoder[3][CHAOS_TABLE[chaosScore(last[-1]) + chaosScore(last[3])]].next(reader);
				}

				// Reverse color filter
				u8 rgb[3];
				cf(yuv, rgb);

				// Switch back to safe spatial filter
				FilterSelection *filter = &_filters[x >> FILTER_ZONE_SIZE_SHIFT];
				sf = filter->sf;

				// Reverse spatial filter
				const u8 *pred = sf(p, x, y, width);
				p[0] = rgb[0] + pred[0];
				p[1] = rgb[1] + pred[1];
				p[2] = rgb[2] + pred[2];
				p[3] = p[-1] - a;
			}

			// Next pixel
			last += PLANES;
			p += 4;
		}
	}

	return RE_OK;
}

int ImageCMReader::read(ImageReader &reader, ImageMaskReader &maskReader, ImageLZReader &lzReader, GCIFImage *image) {
#ifdef CAT_COLLECT_STATS
	m_clock = Clock::ref();

	double t0 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	int err;

	_mask = &maskReader;
	_lz = &lzReader;

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
	CAT_INANE("stats") << "(CM Decode)     Initialization : " << Stats.initUsec << " usec (" << Stats.initUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(CM Decode) Read Filter Tables : " << Stats.readFilterTablesUsec << " usec (" << Stats.readFilterTablesUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(CM Decode)  Read Chaos Tables : " << Stats.readChaosTablesUsec << " usec (" << Stats.readChaosTablesUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(CM Decode)           Read RGB : " << Stats.readRGBUsec << " usec (" << Stats.readRGBUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(CM Decode)            Overall : " << Stats.overallUsec << " usec";

	CAT_INANE("stats") << "(CM Decode)         Throughput : " << (_width * _height * 4) / Stats.overallUsec << " MBPS (output bytes/time)";

	return true;
}

#endif

