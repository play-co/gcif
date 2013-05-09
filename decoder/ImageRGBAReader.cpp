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

#include "ImageRGBAReader.hpp"
#include "Enforcer.hpp"
using namespace cat;

#ifdef CAT_COLLECT_STATS
#include "../encoder/Log.hpp"
#include "../encoder/Clock.hpp"

static cat::Clock *m_clock = 0;
#endif // CAT_COLLECT_STATS

#ifdef CAT_DESYNCH_CHECKS
#define DESYNC_TABLE() \
	CAT_ENFORCE(reader.readWord() == 1234567);
#define DESYNC(x, y) \
	CAT_ENFORCE(reader.readBits(16) == (x ^ 12345)); \
	CAT_ENFORCE(reader.readBits(16) == (y ^ 54321));
#else
#define DESYNC_TABLE()
#define DESYNC(x, y)
#endif


//// ImageRGBAReader

int ImageRGBAReader::readFilterTables(ImageReader & CAT_RESTRICT reader) {
	int err;

	// Read tile bits
	_tile_bits_x = reader.readBits(3) + 1;
	_tile_bits_y = _tile_bits_x;
	_tile_size_x = 1 << _tile_bits_x;
	_tile_size_y = 1 << _tile_bits_y;
	_tile_mask_x = _tile_size_x - 1;
	_tile_mask_y = _tile_size_y - 1;
	_tiles_x = (_size_x + _tile_mask_x) >> _tile_bits_x;
	_tiles_y = (_size_y + _tile_mask_y) >> _tile_bits_y;
	_filters.resize(_tiles_x);

	const int tile_count = _tiles_x * _tiles_y;
	_sf_tiles.resize(tile_count);
	_cf_tiles.resize(tile_count);

	DESYNC_TABLE();

	// Read filter choices
	_sf_count = reader.readBits(5) + SF_FIXED;
	for (int ii = SF_FIXED; ii < _sf_count; ++ii) {
		u8 sf = reader.readBits(7);

		if (sf >= SF_COUNT) {
			CAT_DEBUG_EXCEPTION();
			return GCIF_RE_BAD_RGBA;
		}

		_sf[ii] = RGBA_FILTERS[sf];
	}

	DESYNC_TABLE();

	// Read SF decoder
	{
		MonoReader::Parameters params;
		params.data = _sf_tiles.get();
		params.data_step = 1;
		params.size_x = _tiles_x;
		params.size_y = _tiles_y;
		params.num_syms = _sf_count;

		if ((err = _sf_decoder.readTables(params, reader))) {
			return err;
		}
	}

	DESYNC_TABLE();

	// Read CF decoder
	{
		MonoReader::Parameters params;
		params.data = _cf_tiles.get();
		params.data_step = 1;
		params.size_x = _tiles_x;
		params.size_y = _tiles_y;
		params.num_syms = CF_COUNT;

		if ((err = _cf_decoder.readTables(params, reader))) {
			return err;
		}
	}

	DESYNC_TABLE();

	return GCIF_RE_OK;
}

int ImageRGBAReader::readRGBATables(ImageReader & CAT_RESTRICT reader) {
	int err;

	// Read alpha decoder
	{
		MonoReader::Parameters params;
		params.data = _rgba + 3;
		params.data_step = 4;
		params.size_x = _size_x;
		params.size_y = _size_y;
		params.num_syms = 256;

		if ((err = _a_decoder.readTables(params, reader))) {
			return err;
		}
	}

	DESYNC_TABLE();

	// Read chaos levels
	const int chaos_levels = reader.readBits(4) + 1;

	_chaos.init(chaos_levels, _size_x);

	// For each chaos level,
	for (int jj = 0; jj < chaos_levels; ++jj) {
		// Read the decoder tables
		if (!_y_decoder[jj].init(reader)) {
			CAT_DEBUG_EXCEPTION();
			return GCIF_RE_BAD_RGBA;
		}
		if (!_u_decoder[jj].init(reader)) {
			CAT_DEBUG_EXCEPTION();
			return GCIF_RE_BAD_RGBA;
		}
		if (!_v_decoder[jj].init(reader)) {
			CAT_DEBUG_EXCEPTION();
			return GCIF_RE_BAD_RGBA;
		}
	}

	return GCIF_RE_OK;
}

int ImageRGBAReader::readPixels(ImageReader & CAT_RESTRICT reader) {
	const int size_x = _size_x;
	const u32 MASK_COLOR = _mask->getColor();

	// Get initial triggers
	u16 trigger_x_lz = _lz->getTriggerX();

	// Start from upper-left of image
	u8 * CAT_RESTRICT p = _rgba;

	_chaos.start();

	// Unroll y = 0 scanline
	{
		const int y = 0;

		// If LZ triggered,
		if (y == _lz->getTriggerY()) {
			_lz->triggerY();
			trigger_x_lz = _lz->getTriggerX();
		}

		// Clear filters data
		_filters.fill_00();

		// Read row headers
		_sf_decoder.readRowHeader(y, reader);
		_cf_decoder.readRowHeader(y, reader);

		_a_decoder.readRowHeader(y, reader);

		// Read mask scanline
		const u32 * CAT_RESTRICT mask_next = _mask->nextScanline();
		int mask_left = 0;
		u32 mask;

		// Restart for scanline
		int lz_skip = 0;

		// For each pixel,
		for (int x = 0; x < size_x; ++x) {
			DESYNC(x, y);

			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerX(p);
				trigger_x_lz = _lz->getTriggerX();
			}

			// Next mask word
			if (mask_left-- <= 0) {
				mask = *mask_next++;
				mask_left = 31;
			}

			if (lz_skip > 0) {
				--lz_skip;
				_chaos.zero(x);
				_a_decoder.maskedSkip();
			} else if ((s32)mask < 0) {
				*reinterpret_cast<u32 *>( p ) = MASK_COLOR;
				_chaos.zero(x);
				_a_decoder.maskedSkip();
			} else {
				readSafe(x, y, p, reader);

				DESYNC(x, y);
			}

			// Next pixel
			mask <<= 1;
			p += 4;
		}
	}


	// For each scanline,
	for (int y = 1; y < _size_y; ++y) {
		// If LZ triggered,
		if (y == _lz->getTriggerY()) {
			_lz->triggerY();
			trigger_x_lz = _lz->getTriggerX();
		}

		// If it is time to clear the filters data,
		if ((y & _tile_mask_y) == 0) {
			// Clear filters data
			_filters.fill_00();

			// Read row headers
			_sf_decoder.readRowHeader(y, reader);
			_cf_decoder.readRowHeader(y, reader);
		}

		_a_decoder.readRowHeader(y, reader);

		// Read mask scanline
		const u32 * CAT_RESTRICT mask_next = _mask->nextScanline();
		int mask_left = 0;
		u32 mask;

		// Restart for scanline
		int lz_skip = 0;

		// Unroll x = 0 pixel
		{
			const int x = 0;
			DESYNC(x, y);

			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerX(p);
				trigger_x_lz = _lz->getTriggerX();
			}

			// Next mask word
			mask = *mask_next++;
			mask_left = 31;

			// If pixel was copied with LZ subsystem,
			if (lz_skip > 0) {
				--lz_skip;
				_chaos.zero(x);
				_a_decoder.maskedSkip();
			} else if ((s32)mask < 0) {
				*reinterpret_cast<u32 *>( p ) = MASK_COLOR;
				_chaos.zero(x);
				_a_decoder.maskedSkip();
			} else {
				readSafe(x, y, p, reader);

				DESYNC(x, y);
			}

			// Next pixel
			mask <<= 1;
			p += 4;
		}


		//// BIG INNER LOOP START ////


		// For each pixel,
		for (int x = 1, xend = size_x - 1; x < xend; ++x) {
			DESYNC(x, y);

			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerX(p);
				trigger_x_lz = _lz->getTriggerX();
			}

			// Next mask word
			if (mask_left-- <= 0) {
				mask = *mask_next++;
				mask_left = 31;
			}

			// If pixel was copied with LZ subsystem,
			if (lz_skip > 0) {
				--lz_skip;
				_chaos.zero(x);
				_a_decoder.maskedSkip();
			} else if ((s32)mask < 0) {
				*reinterpret_cast<u32 *>( p ) = MASK_COLOR;
				_chaos.zero(x);
				_a_decoder.maskedSkip();
			} else {
				// Note: Reading with unsafe spatial filter
				readUnsafe(x, y, p, reader);

				DESYNC(x, y);
			}

			// Next pixel
			p += 4;
			mask <<= 1;
		}

		
		//// BIG INNER LOOP END ////


		// For right image edge,
		{
			const int x = size_x - 1;
			DESYNC(x, y);

			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerX(p);
				trigger_x_lz = _lz->getTriggerX();
			}

			// Next mask word
			if (mask_left <= 0) {
				mask = *mask_next;
			}

			// If pixel was copied with LZ subsystem,
			if (lz_skip > 0) {
				--lz_skip;
				_chaos.zero(x);
				_a_decoder.maskedSkip();
			} else if ((s32)mask < 0) {
				*reinterpret_cast<u32 *>( p ) = MASK_COLOR;
				_chaos.zero(x);
				_a_decoder.maskedSkip();
			} else {
				readSafe(x, y, p, reader);

				DESYNC(x, y);
			}

			// Next pixel
			p += 4;
		}
	}

	return GCIF_RE_OK;
}

int ImageRGBAReader::read(ImageReader & CAT_RESTRICT reader, ImageMaskReader & CAT_RESTRICT maskReader, ImageLZReader & CAT_RESTRICT lzReader, GCIFImage * CAT_RESTRICT image) {
#ifdef CAT_COLLECT_STATS
	m_clock = Clock::ref();

	double t0 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	int err;

	_mask = &maskReader;
	_lz = &lzReader;

	_rgba = image->rgba;
	_size_x = image->size_x;
	_size_y = image->size_y;

	// Read filter selection tables
	if ((err = readFilterTables(reader))) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t1 = m_clock->usec();
#endif	

	// Read Huffman tables for each RGB channel and chaos level
	if ((err = readRGBATables(reader))) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t2 = m_clock->usec();
#endif	

	// Read RGB data and decompress it
	if ((err = readPixels(reader))) {
		return err;
	}

	// Pass image data reference back to caller
	_rgba = 0;


#ifdef CAT_COLLECT_STATS
	double t3 = m_clock->usec();

	Stats.readFilterTablesUsec = t1 - t0;
	Stats.readChaosTablesUsec = t2 - t1;
	Stats.readPixelsUsec = t3 - t2;
	Stats.overallUsec = t3 - t0;
#endif	
	return GCIF_RE_OK;
}

#ifdef CAT_COLLECT_STATS

bool ImageRGBAReader::dumpStats() {
	CAT_INANE("stats") << "(RGBA Decode)     Initialization : " << Stats.initUsec << " usec (" << Stats.initUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(RGBA Decode) Read Filter Tables : " << Stats.readFilterTablesUsec << " usec (" << Stats.readFilterTablesUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(RGBA Decode)   Read RGBA Tables : " << Stats.readChaosTablesUsec << " usec (" << Stats.readChaosTablesUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(RGBA Decode)      Decode Pixels : " << Stats.readPixelsUsec << " usec (" << Stats.readPixelsUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(RGBA Decode)            Overall : " << Stats.overallUsec << " usec";

	CAT_INANE("stats") << "(RGBA Decode)         Throughput : " << (_size_x * _size_y * 4) / Stats.overallUsec << " MBPS (output bytes/time)";
	CAT_INANE("stats") << "(RGBA Decode)   Image Dimensions : " << _size_x << " x " << _size_y << " pixels";

	return true;
}

#endif

