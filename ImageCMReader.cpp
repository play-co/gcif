/*
	Copyright (c) 2013 Game Closure.  All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	* Redistributions of source code must retain the above copyright notice,
	  this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	* Neither the name of GCIF nor the names of its contributors may be used
	  to endorse or promote products derived from this software without
	  specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#include "ImageCMReader.hpp"
using namespace cat;

#ifdef CAT_COLLECT_STATS
#include "Log.hpp"
#include "Clock.hpp"

static cat::Clock *m_clock = 0;
#endif // CAT_COLLECT_STATS

#ifdef CAT_DESYNCH_CHECKS
#define DESYNC(x, y) \
	CAT_ENFORCE(reader.readBits(16) == (x ^ 12345)) << x << ", " << y; \
	CAT_ENFORCE(reader.readBits(16) == (y ^ 54321)) << x << ", " << y;
#define DESYNC_FILTER(x, y) \
	CAT_ENFORCE(reader.readBits(16) == (x ^ 31337)) << x << ", " << y; \
	CAT_ENFORCE(reader.readBits(16) == (y ^ 31415)) << x << ", " << y;
#else
#define DESYNC(x, y)
#define DESYNC_FILTER(x, y)
#endif


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
		CAT_DEBUG_EXCEPTION();
		return RE_BAD_DIMS;
	}
	if (_width % FILTER_ZONE_SIZE || _height % FILTER_ZONE_SIZE) {
		CAT_DEBUG_EXCEPTION();
		return RE_BAD_DIMS;
	}

	_rgba = new u8[_width * _height * 4];

	// Fill in image pointer
	image->rgba = _rgba;

	// Just need to remember the last row of filters
	_filters_bytes = (_width >> FILTER_ZONE_SIZE_SHIFT) * sizeof(FilterSelection);
	_filters = new FilterSelection[_width >> FILTER_ZONE_SIZE_SHIFT];

	// And last row of chaos data
	_chaos_size = (_width + 1) * COLOR_PLANES;
	_chaos = new u8[_chaos_size];

	return RE_OK;
}

int ImageCMReader::readFilterTables(ImageReader &reader) {
	// Read in count of custom spatial filters
	u32 rep_count = reader.readBits(5);
	if (rep_count > SF_COUNT) {
		CAT_DEBUG_EXCEPTION();
		return RE_CM_CODES;
	}

	// Read in the preset index for each custom filter
	for (int ii = 0; ii < rep_count; ++ii) {
		u32 def = reader.readBits(5);

		if (def >= SF_COUNT) {
			CAT_DEBUG_EXCEPTION();
			return RE_CM_CODES;
		}

		u32 cust = reader.readBits(7);
		if (cust >= SpatialFilterSet::TAPPED_COUNT) {
			CAT_DEBUG_EXCEPTION();
			return RE_CM_CODES;
		}

		_sf_set.replace(def, cust);
	}

	// Initialize huffman decoder
	if (reader.eof() || !_cf.init(CF_COUNT, reader, 8)) {
		CAT_DEBUG_EXCEPTION();
		return RE_CM_CODES;
	}

	// Initialize huffman decoder
	if (reader.eof() || !_sf.init(SF_COUNT, reader, 8)) {
		CAT_DEBUG_EXCEPTION();
		return RE_CM_CODES;
	}

	return RE_OK;
}

int ImageCMReader::readChaosTables(ImageReader &reader) {
	_chaos_levels = reader.readBits(3) + 1;

	switch (_chaos_levels) {
		case 1:
			_chaos_table = CHAOS_TABLE_1;
			break;
		case 8:
			_chaos_table = CHAOS_TABLE_8;
			break;
		default:
			CAT_DEBUG_EXCEPTION();
			return RE_CM_CODES;
	}

	// For each chaos level,
	for (int jj = 0; jj < _chaos_levels; ++jj) {
		// Read the decoder tables
		if (!_y_decoder[jj].init(reader)) {
			CAT_DEBUG_EXCEPTION();
			return RE_CM_CODES;
		}
		if (!_u_decoder[jj].init(reader)) {
			CAT_DEBUG_EXCEPTION();
			return RE_CM_CODES;
		}
		if (!_v_decoder[jj].init(reader)) {
			CAT_DEBUG_EXCEPTION();
			return RE_CM_CODES;
		}
		if (!_a_decoder[jj].init(reader)) {
			CAT_DEBUG_EXCEPTION();
			return RE_CM_CODES;
		}
	}

	return RE_OK;
}

int ImageCMReader::readPixels(ImageReader &reader) {
	const int width = _width;

	CAT_DEBUG_ENFORCE(MASK_COUNT == 2); // Unrolled in here
	CAT_DEBUG_ENFORCE(!_masks[0].enabled() || _masks[0].getColor() == 0);
	CAT_DEBUG_ENFORCE(!_masks[1].enabled() || _masks[1].getColor() != 0);

	// Get initial triggers
	u16 trigger_x_lz = _lz->getTriggerX();

	// Start from upper-left of image
	u8 *p = _rgba;
	u8 *lastStart = _chaos + COLOR_PLANES;
	CAT_CLR(_chaos, _chaos_size);

	const u8 *CHAOS_TABLE = _chaos_table;
	u8 FPT[3];

	// Unroll y = 0 scanline
	{
		const int y = 0;

		// If LZ triggered,
		if (y == _lz->getTriggerY()) {
			_lz->triggerY();
			trigger_x_lz = _lz->getTriggerX();
		}

		// Clear filters data
		CAT_CLR(_filters, _filters_bytes);

		for (int ii = 0; ii < MASK_COUNT; ++ii) {
			_masks[ii].nextScanline();
		}

		// Restart for scanline
		u8 *last = lastStart;
		int lz_skip = 0, lz_lines_left = 0;

		// For each pixel,
		for (int x = 0; x < width; ++x) {
			DESYNC(x, y);

			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerX(p, lz_lines_left);
				trigger_x_lz = _lz->getTriggerX();
			}

			if (lz_skip > 0) {
				--lz_skip;
				last[0] = 0;
				last[1] = 0;
				last[2] = 0;
				last[3] = 0;
			} else if (_masks[0].masked(x)) {
				// Fully-transparent pixel
				*reinterpret_cast<u32 *>( p ) = 0;
				last[0] = 0;
				last[1] = 0;
				last[2] = 0;
				last[3] = 0;
			} else if (_masks[1].masked(x)) {
				// Fully-transparent pixel
				*reinterpret_cast<u32 *>( p ) = _masks[1].getColor();
				last[0] = 0;
				last[1] = 0;
				last[2] = 0;
				last[3] = 0;
			} else {
				// Read SF and CF for this zone
				FilterSelection *filter = &_filters[x >> FILTER_ZONE_SIZE_SHIFT];
				if (!filter->ready()) {
					filter->cf = YUV2RGB_FILTERS[_cf.next(reader)];
					DESYNC_FILTER(x, y);
					filter->sf = _sf_set.get(_sf.next(reader));
					DESYNC_FILTER(x, y);
				}

				// Calculate YUV chaos
				const u32 chaos_y = CHAOS_TABLE[last[-4]];
				const u32 chaos_u = CHAOS_TABLE[last[-3]];
				const u32 chaos_v = CHAOS_TABLE[last[-2]];

				// Read YUV filtered pixel
				u8 YUV[3];
				YUV[0] = (u8)_y_decoder[chaos_y].next(reader);
				DESYNC(x, y);
				YUV[1] = (u8)_u_decoder[chaos_u].next(reader);
				DESYNC(x, y);
				YUV[2] = (u8)_v_decoder[chaos_v].next(reader);
				DESYNC(x, y);

				// Calculate alpha chaos
				const u32 chaos_a = CHAOS_TABLE[last[-1]];

				// Reverse color filter
				filter->cf(YUV, p);

				// Reverse spatial filter
				const u8 *pred = FPT;
				filter->sf.safe(p, &pred, x, y, width);
				p[0] += pred[0];
				p[1] += pred[1];
				p[2] += pred[2];

				// Read alpha pixel
				u8 A = (u8)_a_decoder[chaos_a].next(reader);
				DESYNC(x, y);
				if (x > 0) {
					p[3] = p[-1] - A;
				} else {
					p[3] = 255 - A;
				}

				// Convert last to score
				last[0] = chaosScore(YUV[0]);
				last[1] = chaosScore(YUV[1]);
				last[2] = chaosScore(YUV[2]);
				last[3] = chaosScore(A);
			}

			// Next pixel
			last += COLOR_PLANES;
			p += 4;
		}
	}


	// For each scanline,
	for (int y = 1; y < _height; ++y) {
		// If LZ triggered,
		if (y == _lz->getTriggerY()) {
			_lz->triggerY();
			trigger_x_lz = _lz->getTriggerX();
		}

		// If it is time to clear the filters data,
		if ((y & FILTER_ZONE_SIZE_MASK) == 0) {
			CAT_CLR(_filters, _filters_bytes);
		}

		for (int ii = 0; ii < MASK_COUNT; ++ii) {
			_masks[ii].nextScanline();
		}

		// Restart for scanline
		u8 *last = lastStart;
		int lz_skip = 0, lz_lines_left = 0;

		// Unroll x = 0 pixel
		{
			const int x = 0;
			DESYNC(x, y);

			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerX(p, lz_lines_left);
				trigger_x_lz = _lz->getTriggerX();
			}

			if (lz_skip > 0) {
				--lz_skip;
				last[0] = 0;
				last[1] = 0;
				last[2] = 0;
				last[3] = 0;
			} else if (_masks[0].masked(x)) {
				// Fully-transparent pixel
				*reinterpret_cast<u32 *>( p ) = 0;
				last[0] = 0;
				last[1] = 0;
				last[2] = 0;
				last[3] = 0;
			} else if (_masks[1].masked(x)) {
				// Fully-transparent pixel
				*reinterpret_cast<u32 *>( p ) = _masks[1].getColor();
				last[0] = 0;
				last[1] = 0;
				last[2] = 0;
				last[3] = 0;
			} else {
				// Read SF and CF for this zone
				FilterSelection *filter = &_filters[x >> FILTER_ZONE_SIZE_SHIFT];
				if (!filter->ready()) {
					filter->cf = YUV2RGB_FILTERS[_cf.next(reader)];
					DESYNC_FILTER(x, y);
					filter->sf = _sf_set.get(_sf.next(reader));
					DESYNC_FILTER(x, y);
				}

				// Calculate YUV chaos
				const u32 chaos_y = CHAOS_TABLE[last[0]];
				const u32 chaos_u = CHAOS_TABLE[last[1]];
				const u32 chaos_v = CHAOS_TABLE[last[2]];

				// Read YUV filtered pixel
				u8 YUV[3];
				YUV[0] = (u8)_y_decoder[chaos_y].next(reader);;
				DESYNC(x, y);
				YUV[1] = (u8)_u_decoder[chaos_u].next(reader);
				DESYNC(x, y);
				YUV[2] = (u8)_v_decoder[chaos_v].next(reader);
				DESYNC(x, y);

				// Calculate alpha chaos
				const u32 chaos_a = CHAOS_TABLE[last[3]];

				// Reverse color filter
				filter->cf(YUV, p);

				// Reverse spatial filter
				const u8 *pred = FPT;
				filter->sf.safe(p, &pred, x, y, width);
				p[0] += pred[0];
				p[1] += pred[1];
				p[2] += pred[2];

				// Read alpha pixel
				u8 A = (u8)_a_decoder[chaos_a].next(reader);
				DESYNC(x, y);
				p[3] = 255 - A;

				// Convert last to score
				last[0] = chaosScore(YUV[0]);
				last[1] = chaosScore(YUV[1]);
				last[2] = chaosScore(YUV[2]);
				last[3] = chaosScore(A);
			}

			// Next pixel
			last += COLOR_PLANES;
			p += 4;
		}


		//// BIG INNER LOOP START ////


		// For each pixel,
		for (int x = 1, xend = width - 1; x < xend; ++x) {
			DESYNC(x, y);

			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerX(p, lz_lines_left);
				trigger_x_lz = _lz->getTriggerX();
			}

			if (lz_skip > 0) {
				--lz_skip;
				last[0] = 0;
				last[1] = 0;
				last[2] = 0;
				last[3] = 0;
			} else if (_masks[0].masked(x)) {
				*reinterpret_cast<u32 *>( p ) = 0;
				last[0] = 0;
				last[1] = 0;
				last[2] = 0;
				last[3] = 0;
			} else if (_masks[1].masked(x)) {
				// Fully-transparent pixel
				*reinterpret_cast<u32 *>( p ) = _masks[1].getColor();
				last[0] = 0;
				last[1] = 0;
				last[2] = 0;
				last[3] = 0;
			} else {
				// Read SF and CF for this zone
				FilterSelection *filter = &_filters[x >> FILTER_ZONE_SIZE_SHIFT];
				if (!filter->ready()) {
					filter->cf = YUV2RGB_FILTERS[_cf.next(reader)];
					DESYNC_FILTER(x, y);
					filter->sf = _sf_set.get(_sf.next(reader));
					DESYNC_FILTER(x, y);
				}

				// Calculate YUV chaos
				const u32 chaos_y = CHAOS_TABLE[last[-4] + (u16)last[0]];
				const u32 chaos_u = CHAOS_TABLE[last[-3] + (u16)last[1]];
				const u32 chaos_v = CHAOS_TABLE[last[-2] + (u16)last[2]];

				// Read YUV filtered pixel
				u8 YUV[3];
				YUV[0] = (u8)_y_decoder[chaos_y].next(reader);
				DESYNC(x, y);
				YUV[1] = (u8)_u_decoder[chaos_u].next(reader);
				DESYNC(x, y);
				YUV[2] = (u8)_v_decoder[chaos_v].next(reader);
				DESYNC(x, y);

				// Calculate alpha chaos
				const u32 chaos_a = CHAOS_TABLE[last[-1] + (u16)last[3]];

				// Reverse color filter
				filter->cf(YUV, p);

				// Reverse spatial filter
				const u8 *pred = FPT;
				filter->sf.unsafe(p, &pred, x, y, width);
				p[0] += pred[0];
				p[1] += pred[1];
				p[2] += pred[2];

				// Read alpha pixel
				u32 A = (u8)_a_decoder[chaos_a].next(reader);
				p[3] = p[-1] - A;
				DESYNC(x, y);

				// Convert last to score
				last[0] = chaosScore(YUV[0]);
				last[1] = chaosScore(YUV[1]);
				last[2] = chaosScore(YUV[2]);
				last[3] = chaosScore(A);
			}

			// Next pixel
			last += COLOR_PLANES;
			p += 4;
		}

		
		//// BIG INNER LOOP END ////


		// For x = width-1,
		{
			const int x = width - 1;
			DESYNC(x, y);

			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerX(p, lz_lines_left);
				trigger_x_lz = _lz->getTriggerX();
			}

			if (lz_skip > 0) {
				--lz_skip;
				last[0] = 0;
				last[1] = 0;
				last[2] = 0;
				last[3] = 0;
			} else if (_masks[0].masked(x)) {
				*reinterpret_cast<u32 *>( p ) = 0;
				last[0] = 0;
				last[1] = 0;
				last[2] = 0;
				last[3] = 0;
			} else if (_masks[1].masked(x)) {
				// Fully-transparent pixel
				*reinterpret_cast<u32 *>( p ) = _masks[1].getColor();
				last[0] = 0;
				last[1] = 0;
				last[2] = 0;
				last[3] = 0;
			} else {
				// Read SF and CF for this zone
				FilterSelection *filter = &_filters[x >> FILTER_ZONE_SIZE_SHIFT];
				if (!filter->ready()) {
					filter->cf = YUV2RGB_FILTERS[_cf.next(reader)];
					DESYNC_FILTER(x, y);
					filter->sf = _sf_set.get(_sf.next(reader));
					DESYNC_FILTER(x, y);
				}

				// Calculate YUV chaos
				const u32 chaos_y = CHAOS_TABLE[last[-4] + (u16)last[0]];
				const u32 chaos_u = CHAOS_TABLE[last[-3] + (u16)last[1]];
				const u32 chaos_v = CHAOS_TABLE[last[-2] + (u16)last[2]];

				// Read YUV filtered pixel
				u8 YUV[3];
				YUV[0] = (u8)_y_decoder[chaos_y].next(reader);
				DESYNC(x, y);
				YUV[1] = (u8)_u_decoder[chaos_u].next(reader);
				DESYNC(x, y);
				YUV[2] = (u8)_v_decoder[chaos_v].next(reader);
				DESYNC(x, y);

				// Calculate alpha chaos
				const u32 chaos_a = CHAOS_TABLE[last[-1] + (u16)last[3]];

				// Reverse color filter
				filter->cf(YUV, p);

				// Reverse (safe) spatial filter
				const u8 *pred = FPT;
				filter->sf.safe(p, &pred, x, y, width);
				p[0] += pred[0];
				p[1] += pred[1];
				p[2] += pred[2];

				// Read alpha pixel
				u8 A = (u8)_a_decoder[chaos_a].next(reader);
				DESYNC(x, y);
				p[3] = p[-1] - A;

				// Convert last to score
				last[0] = chaosScore(YUV[0]);
				last[1] = chaosScore(YUV[1]);
				last[2] = chaosScore(YUV[2]);
				last[3] = chaosScore(A);
			}

			// Next pixel
			last += COLOR_PLANES;
			p += 4;
		}
	}

	return RE_OK;
}

int ImageCMReader::read(ImageReader &reader, ImageMaskReader *maskReaders, ImageLZReader &lzReader, GCIFImage *image) {
#ifdef CAT_COLLECT_STATS
	m_clock = Clock::ref();

	double t0 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	int err;

	_masks = maskReaders;
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
	if ((err = readPixels(reader))) {
		return err;
	}

	// Pass image data reference back to caller
	_rgba = 0;


#ifdef CAT_COLLECT_STATS
	double t4 = m_clock->usec();

	Stats.initUsec = t1 - t0;
	Stats.readFilterTablesUsec = t2 - t1;
	Stats.readChaosTablesUsec = t3 - t2;
	Stats.readPixelsUsec = t4 - t3;
	Stats.overallUsec = t4 - t0;
#endif	
	return RE_OK;
}

#ifdef CAT_COLLECT_STATS

bool ImageCMReader::dumpStats() {
	CAT_INANE("stats") << "(CM Decode)     Initialization : " << Stats.initUsec << " usec (" << Stats.initUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(CM Decode) Read Filter Tables : " << Stats.readFilterTablesUsec << " usec (" << Stats.readFilterTablesUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(CM Decode)  Read Chaos Tables : " << Stats.readChaosTablesUsec << " usec (" << Stats.readChaosTablesUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(CM Decode)      Decode Pixels : " << Stats.readPixelsUsec << " usec (" << Stats.readPixelsUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(CM Decode)            Overall : " << Stats.overallUsec << " usec";

	CAT_INANE("stats") << "(CM Decode)         Throughput : " << (_width * _height * 4) / Stats.overallUsec << " MBPS (output bytes/time)";

	return true;
}

#endif

