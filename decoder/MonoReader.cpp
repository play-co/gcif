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

#include "MonoReader.hpp"
#include "Enforcer.hpp"
#include "BitMath.hpp"
using namespace cat;


#ifdef CAT_DESYNCH_CHECKS
#define DESYNC_TABLE() writer.writeBits(1234567);
#define DESYNC(x, y) writer.writeBits(x ^ 12345, 16); writer.writeBits(y ^ 54321, 16);
#define DESYNC_FILTER(x, y) writer.writeBits(x ^ 31337, 16); writer.writeBits(y ^ 31415, 16);
#else
#define DESYNC_TABLE()
#define DESYNC(x, y)
#define DESYNC_FILTER(x, y)
#endif


//// MonoReader

void MonoReader::cleanup() {
}


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

#include "ImageCMReaderPal.hpp"
#include "Enforcer.hpp"
using namespace cat;

#ifdef CAT_COLLECT_STATS
#include "../encoder/Log.hpp"
#include "../encoder/Clock.hpp"

static cat::Clock *m_clock = 0;
#endif // CAT_COLLECT_STATS

#ifdef CAT_DESYNCH_CHECKS
#define DESYNC(x, y) \
	CAT_ENFORCE(reader.readBits(16) == (x ^ 12345)); \
	CAT_ENFORCE(reader.readBits(16) == (y ^ 54321));
#define DESYNC_FILTER(x, y) \
	CAT_ENFORCE(reader.readBits(16) == (x ^ 31337)); \
	CAT_ENFORCE(reader.readBits(16) == (y ^ 31415));
#else
#define DESYNC(x, y)
#define DESYNC_FILTER(x, y)
#endif


//// ImageCMReaderPal

void ImageCMReaderPal::clear() {
	if (_chaos) {
		delete []_chaos;
		_chaos = 0;
	}
	if (_filters) {
		delete []_filters;
		_filters = 0;
	}
	if (_pdata) {
		delete []_pdata;
		_pdata = 0;
	}
	// Do not free RGBA data here
}

int ImageCMReaderPal::init(GCIFImage *image) {
	_width = image->width;
	_height = image->height;

	// Always allocate new RGBA data
	_rgba = new u8[_width * _height * 4];

	// Fill in image pointer
	image->rgba = _rgba;

	// Just need to remember the last row of filters
	const int filter_count = (_width + PALETTE_ZONE_SIZE_MASK_W) >> PALETTE_ZONE_SIZE_SHIFT_W;
	_filters_bytes = filter_count * sizeof(FilterSelection);

	if (!_filters || _filters_bytes > _filters_alloc) {
		if (_filters) {
			delete []_filters;
		}
		_filters = new FilterSelection[filter_count];
		_filters_alloc = _filters_bytes;
	}

	// Allocate pdata bytes
	const int pdata_bytes = _width * _height;
	if (!_pdata || pdata_bytes > _pdata_alloc) {
		if (_pdata) {
			delete []_pdata;
		}
		_pdata = new u8[pdata_bytes];
		_pdata_alloc = pdata_bytes;
	}

	// And last row of chaos data
	_chaos_size = _width + 1;

	if (!_chaos || _chaos_alloc < _chaos_size) {
		if (_chaos) {
			delete []_chaos;
		}
		_chaos = new u8[_chaos_size];
		_chaos_alloc = _chaos_size;
	}

	return GCIF_RE_OK;
}

int ImageCMReaderPal::readFilterTables(ImageReader &reader) {
	// Read in count of custom spatial filters
	int rep_count = reader.readBits(5);
	if (rep_count > SF_COUNT) {
		CAT_DEBUG_EXCEPTION();
		return GCIF_RE_CM_CODES;
	}

	_pf_set.init();

	// Read in the preset index for each custom filter
	for (int ii = 0; ii < rep_count; ++ii) {
		int def = reader.readBits(5);

		if (def >= SF_COUNT) {
			CAT_DEBUG_EXCEPTION();
			return GCIF_RE_CM_CODES;
		}

		int cust = reader.readBits(7);
		if (cust >= SpatialFilterSet::TAPPED_COUNT) {
			CAT_DEBUG_EXCEPTION();
			return GCIF_RE_CM_CODES;
		}

		_pf_set.replace(def, cust);
	}

	// Initialize huffman decoder
	if (reader.eof() || !_pf.init(SF_COUNT, reader, 8)) {
		CAT_DEBUG_EXCEPTION();
		return GCIF_RE_CM_CODES;
	}

	return GCIF_RE_OK;
}

int ImageCMReaderPal::readChaosTables(ImageReader &reader) {
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
			return GCIF_RE_CM_CODES;
	}

	// For each chaos level,
	for (int jj = 0; jj < _chaos_levels; ++jj) {
		// Read the decoder tables
		if (!_decoder[jj].init(reader)) {
			CAT_DEBUG_EXCEPTION();
			return GCIF_RE_CM_CODES;
		}
	}

	return GCIF_RE_OK;
}

int ImageCMReaderPal::readPixels(ImageReader &reader) {
	const int width = _width;
	const u16 PAL_SIZE = static_cast<u16>( _pal->getPaletteSize() );
	const u32 MASK_COLOR = _mask->getColor();
	static u8 MASK_PALETTE = _pal->getMaskPalette();

	// Get initial triggers
	u16 trigger_x_lz = _lz->getTriggerX();

	// Start from upper-left of image
	u32 *rgba = reinterpret_cast<u32 *>( _rgba );
	u8 *p = _pdata;

	u8 *lastStart = _chaos + 1;
	CAT_CLR(_chaos, _chaos_size);

	const u8 *CHAOS_TABLE = _chaos_table;

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

		// Read mask scanline
		const u32 *mask_next = _mask->nextScanline();
		int mask_left = 0;
		u32 mask;

		// Restart for scanline
		u8 *last = lastStart;
		int lz_skip = 0;

		// For each pixel,
		for (int x = 0; x < width; ++x) {
			DESYNC(x, y);

			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerXPal(p, rgba);
				trigger_x_lz = _lz->getTriggerX();
			}

			// Next mask word
			if (mask_left-- <= 0) {
				mask = *mask_next++;
				mask_left = 31;
			}

			u8 code = 0;

			if (lz_skip > 0) {
				--lz_skip;
			} else if ((s32)mask < 0) {
				*rgba = MASK_COLOR;
				*p = MASK_PALETTE;
			} else {
				// Read filter for this zone
				FilterSelection *filter = &_filters[x >> PALETTE_ZONE_SIZE_SHIFT_W];
				if (!filter->ready()) {
					filter->sf = _pf_set.get(_pf.next(reader));
					DESYNC_FILTER(x, y);
				}

				// Calculate chaos
				const u32 chaos = CHAOS_TABLE[last[-1]];

				// Read filtered pixel
				code = (u8)_decoder[chaos].next(reader);
				DESYNC(x, y);

				// Reverse spatial filter
				const u32 pred = filter->sf.safe(p, x, y, width);
				u8 index = (code + pred) % PAL_SIZE;

				// Reverse palette to RGBA
				*rgba = _pal->getColor(index);
				*p = index;

				// Convert to score
				code = CHAOS_SCORE[code];
			}

			// Next pixel
			++rgba;
			++p;
			mask <<= 1;

			// Record chaos
			*last++ = code;
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
		if ((y & PALETTE_ZONE_SIZE_MASK_H) == 0) {
			CAT_CLR(_filters, _filters_bytes);
		}

		// Read mask scanline
		const u32 *mask_next = _mask->nextScanline();
		int mask_left = 0;
		u32 mask;

		// Restart for scanline
		u8 *last = lastStart;
		int lz_skip = 0;

		// Unroll x = 0 pixel
		{
			const int x = 0;
			DESYNC(x, y);

			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerXPal(p, rgba);
				trigger_x_lz = _lz->getTriggerX();
			}

			// Next mask word
			mask = *mask_next++;
			mask_left = 31;

			u8 code = 0;

			if (lz_skip > 0) {
				--lz_skip;
			} else if ((s32)mask < 0) {
				*rgba = MASK_COLOR; 
				*p = MASK_PALETTE;
			} else {
				// Read filter for this zone
				FilterSelection *filter = &_filters[x >> PALETTE_ZONE_SIZE_SHIFT_W];
				if (!filter->ready()) {
					filter->sf = _pf_set.get(_pf.next(reader));
					DESYNC_FILTER(x, y);
				}

				// Calculate chaos
				const u32 chaos = CHAOS_TABLE[last[0]];

				// Read filtered pixel
				code = (u8)_decoder[chaos].next(reader);
				DESYNC(x, y);

				// Reverse spatial filter
				const u32 pred = filter->sf.safe(p, x, y, width);
				u8 index = (code + pred) % PAL_SIZE;

				// Reverse palette to RGBA
				*rgba = _pal->getColor(index);
				*p = index;

				// Convert to score
				code = CHAOS_SCORE[code];
			}

			// Next pixel
			++rgba;
			++p;
			mask <<= 1;

			// Record chaos
			*last++ = code;
		}


		//// BIG INNER LOOP START ////


		// For each pixel,
		for (int x = 1, xend = width - 1; x < xend; ++x) {
			DESYNC(x, y);

			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerXPal(p, rgba);
				trigger_x_lz = _lz->getTriggerX();
			}

			// Next mask word
			if (mask_left-- <= 0) {
				mask = *mask_next++;
				mask_left = 31;
			}

			u8 code = 0;

			if (lz_skip > 0) {
				--lz_skip;
			} else if ((s32)mask < 0) {
				*rgba = MASK_COLOR; 
				*p = MASK_PALETTE;
			} else {
				// Read filter for this zone
				FilterSelection *filter = &_filters[x >> PALETTE_ZONE_SIZE_SHIFT_W];
				if (!filter->ready()) {
					filter->sf = _pf_set.get(_pf.next(reader));
					DESYNC_FILTER(x, y);
				}

				// Calculate chaos
				const u32 chaos = CHAOS_TABLE[last[-1] + (u16)last[0]];

				// Read filtered pixel
				code = (u8)_decoder[chaos].next(reader);
				DESYNC(x, y);

				// Reverse spatial filter
				const u32 pred = filter->sf.safe(p, x, y, width);
				u8 index = (code + pred) % PAL_SIZE;

				// Reverse palette to RGBA
				*rgba = _pal->getColor(index);
				*p = index;

				// Convert to score
				code = CHAOS_SCORE[code];
			}

			// Next pixel
			++rgba;
			++p;
			mask <<= 1;

			// Record chaos
			*last++ = code;
		}

		
		//// BIG INNER LOOP END ////


		// For x = width-1,
		{
			const int x = width - 1;
			DESYNC(x, y);

			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerXPal(p, rgba);
				trigger_x_lz = _lz->getTriggerX();
			}

			// Next mask word
			if (mask_left <= 0) {
				mask = *mask_next;
			}

			u8 code = 0;

			if (lz_skip > 0) {
				--lz_skip;
			} else if ((s32)mask < 0) {
				*rgba = MASK_COLOR; 
				*p = MASK_PALETTE;
			} else {
				// Read filter for this zone
				FilterSelection *filter = &_filters[x >> PALETTE_ZONE_SIZE_SHIFT_W];
				if (!filter->ready()) {
					filter->sf = _pf_set.get(_pf.next(reader));
					DESYNC_FILTER(x, y);
				}

				// Calculate chaos
				const u32 chaos = CHAOS_TABLE[last[-1] + (u16)last[0]];

				// Read filtered pixel
				code = (u8)_decoder[chaos].next(reader);
				DESYNC(x, y);

				// Reverse spatial filter
				const u32 pred = filter->sf.safe(p, x, y, width);
				u8 index = (code + pred) % PAL_SIZE;

				// Reverse palette to RGBA
				*rgba = _pal->getColor(index);
				*p = index;

				// Convert to score
				code = CHAOS_SCORE[code];
			}

			// Next pixel
			++rgba;
			++p;

			// Record chaos
			*last = code;
		} // next x
	} // next y

	return GCIF_RE_OK;
}

int ImageCMReaderPal::read(ImageReader &reader, ImageMaskReader &maskReader, ImageLZReader &lzReader, ImagePaletteReader &palReader, GCIFImage *image) {
#ifdef CAT_COLLECT_STATS
	m_clock = Clock::ref();

	double t0 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	int err;

	_mask = &maskReader;
	_lz = &lzReader;
	_pal = &palReader;

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
	return GCIF_RE_OK;
}

#ifdef CAT_COLLECT_STATS

bool ImageCMReaderPal::dumpStats() {
	CAT_INANE("stats") << "(CM Decode)     Initialization : " << Stats.initUsec << " usec (" << Stats.initUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(CM Decode) Read Filter Tables : " << Stats.readFilterTablesUsec << " usec (" << Stats.readFilterTablesUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(CM Decode)  Read Chaos Tables : " << Stats.readChaosTablesUsec << " usec (" << Stats.readChaosTablesUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(CM Decode)      Decode Pixels : " << Stats.readPixelsUsec << " usec (" << Stats.readPixelsUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(CM Decode)            Overall : " << Stats.overallUsec << " usec";

	CAT_INANE("stats") << "(CM Decode)         Throughput : " << (_width * _height * 4) / Stats.overallUsec << " MBPS (output bytes/time)";
	CAT_INANE("stats") << "(CM Decode)   Image Dimensions : " << _width << " x " << _height << " pixels";

	return true;
}

#endif


