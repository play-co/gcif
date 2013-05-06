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
#include "Filters.hpp"
using namespace cat;

#ifdef CAT_COLLECT_STATS
#include "../encoder/Log.hpp"
#include "../encoder/Clock.hpp"

static cat::Clock *m_clock = 0;
#endif // CAT_COLLECT_STATS

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
	if (_tile_row_filters) {
		delete []_tile_row_filters;
		_tile_row_filters = 0;
	}
	if (_filter_decoder) {
		delete _filter_decoder;
		_filter_decoder = 0;
	}
}

int MonoReader::readTables(ImageReader &reader) {
	// Read tile size
	{
	}

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

int MonoReader::readPixels(ImageReader &reader) {
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

int MonoReader::read(const Parameters &params, ImageReader &reader) {
	_params = params;

	// Calculate bits to represent tile bits field
	u32 range = (_params.max_bits - _params.min_bits);
	int bits_bc = 0;
	if (range > 0) {
		bits_bc = BSR32(range) + 1;
	}
	_tile_bits_field_bc = bits_bc;

	return GCIF_RE_OK;
}

#ifdef CAT_COLLECT_STATS

bool MonoReader::dumpStats() {
	CAT_INANE("stats") << "(Mono Decode)     Initialization : " << Stats.initUsec << " usec (" << Stats.initUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(Mono Decode) Read Filter Tables : " << Stats.readFilterTablesUsec << " usec (" << Stats.readFilterTablesUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(Mono Decode)  Read Chaos Tables : " << Stats.readChaosTablesUsec << " usec (" << Stats.readChaosTablesUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(Mono Decode)      Decode Pixels : " << Stats.readPixelsUsec << " usec (" << Stats.readPixelsUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(Mono Decode)            Overall : " << Stats.overallUsec << " usec";

	CAT_INANE("stats") << "(Mono Decode)         Throughput : " << (_width * _height * 4) / Stats.overallUsec << " MBPS (output bytes/time)";
	CAT_INANE("stats") << "(Mono Decode)   Image Dimensions : " << _width << " x " << _height << " pixels";

	return true;
}

#endif


