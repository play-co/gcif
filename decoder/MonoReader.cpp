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
#endif

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


//// MonoReader

void MonoReader::cleanup() {
	if (_filter_decoder) {
		delete _filter_decoder;
		_filter_decoder = 0;
	}
}

int MonoReader::readTables(const Parameters & CAT_RESTRICT params, ImageReader & CAT_RESTRICT reader) {
	// Store parameters
	_params = params;

	// Check if LZ is enabled for this monochrome image
	_lz_enabled = (reader.readBit() == 1);

	const int num_syms = _params.num_syms + (_lz_enabled ? LZReader::ESCAPE_SYMS : 0);

	// If decoder is disabled,
	if (reader.readBit() == 0) {
		_use_row_filters = true;

		// If one row filter,
		if (reader.readBit()) {
			_one_row_filter = true;
			_row_filter = reader.readBit();
		} else {
			_one_row_filter = false;
		}

		DESYNC_TABLE();

		if CAT_UNLIKELY(!_row_filter_decoder.init(num_syms, ZRLE_SYMS, HUFF_LUT_BITS, reader)) {
			return GCIF_RE_BAD_MONO;
		}

		DESYNC_TABLE();

		// NOTE: Chaos is not actually used, but it avoids an if-statement in zero()
		_chaos.init(0, _params.xsize);
	} else {
		// Enable decoder
		_use_row_filters = false;

		// Calculate bits to represent tile bits field
		u32 range = _params.max_bits - _params.min_bits;
		int bits_bc = 0;
		if (range > 0) {
			bits_bc = BSR32(range) + 1;

			int tile_bits_field_bc = bits_bc;

			// Read tile bits
			_tile_bits_x = reader.readBits(tile_bits_field_bc) + _params.min_bits;

			// Validate input
			if CAT_UNLIKELY(_tile_bits_x > _params.max_bits) {
				CAT_DEBUG_EXCEPTION();
				return GCIF_RE_BAD_MONO;
			}
		} else {
			_tile_bits_x = _params.min_bits;
		}

		DESYNC_TABLE();

		// Initialize tile sizes
		_tile_bits_y = _tile_bits_x;
		_tile_xsize = 1 << _tile_bits_x;
		_tile_ysize = 1 << _tile_bits_y;
		_tile_mask_x = _tile_xsize - 1;
		_tile_mask_y = _tile_ysize - 1;
		_tiles_x = (_params.xsize + _tile_xsize - 1) >> _tile_bits_x;
		_tiles_y = (_params.ysize + _tile_ysize - 1) >> _tile_bits_y;

		_filter_row.resize(_tiles_x);

		_tiles.resizeZero(_tiles_x * _tiles_y);

		// Read palette
		CAT_DEBUG_ENFORCE(MAX_PALETTE == 15);

		// Clear palette so bounds checking does not need to be performed
		CAT_OBJCLR(_palette);

		const int sympal_filter_count = reader.readBits(4);
		for (int ii = 0; ii < sympal_filter_count; ++ii) {
			_palette[ii] = reader.readBits(8);
		}

		DESYNC_TABLE();

		CAT_DEBUG_ENFORCE(MAX_FILTERS == 32);

		// Read normal filters
		_filter_count = reader.readBits(5) + 1;
		for (int ii = 0, iiend = _filter_count; ii < iiend; ++ii) {
			u8 sf = reader.readBits(7);

#ifdef CAT_DUMP_FILTERS
			CAT_WARN("Mono") << "Filter " << ii << " = " << (int)sf;
#endif

			// If it is a palette filter,
			if (sf >= SF_COUNT) {
				u8 pf = sf - SF_COUNT;

				CAT_DEBUG_ENFORCE(pf < MAX_PALETTE);

				// Set filter function sentinel
				MonoFilterFuncs pal_funcs;
				pal_funcs.safe = pal_funcs.unsafe = (MonoFilterFunc)(pf + 1);
				_sf[ii] = pal_funcs;
			} else {
				_sf[ii] = MONO_FILTERS[sf];
			}
		}

		DESYNC_TABLE();

		CAT_DEBUG_ENFORCE(MAX_CHAOS_LEVELS == 16);

		// Read chaos levels
		int chaos_levels = reader.readBits(4) + 1;
		_chaos.init(chaos_levels, _params.xsize);
		_chaos.start();

		DESYNC_TABLE();

		// For each chaos level,
		for (int ii = 0; ii < chaos_levels; ++ii) {
			if CAT_UNLIKELY(!_decoder[ii].init(num_syms, ZRLE_SYMS, HUFF_LUT_BITS, reader)) {
				CAT_DEBUG_EXCEPTION();
				return GCIF_RE_BAD_MONO;
			}
		}

		DESYNC_TABLE();

		// Create a recursive decoder if needed
		if (!_filter_decoder) {
			_filter_decoder = new MonoReader;
		}

		Parameters sub_params;
		sub_params.data = _tiles.get();
		sub_params.xsize = _tiles_x;
		sub_params.ysize = _tiles_y;
		sub_params.min_bits = _params.min_bits;
		sub_params.max_bits = _params.max_bits;
		sub_params.num_syms = _filter_count;

		int err;
		if CAT_UNLIKELY((err = _filter_decoder->readTables(sub_params, reader))) {
			return err;
		}

		// Get safe read delegate
		_filter_decoder_read = _filter_decoder->getReadDelegate(true);

		_filter_decoder->setupUnordered();

		_current_tile = _tiles.get();
	}

	DESYNC_TABLE();

	// If LZ was enabled in header,
	if (_lz_enabled) {
		// Initialize LZ subsystem
		if CAT_UNLIKELY(!_lz.init(_params.xsize, _params.ysize, reader)) {
			CAT_DEBUG_EXCEPTION();
			return GCIF_RE_BAD_MONO;
		}
	}

	DESYNC_TABLE();

	if CAT_UNLIKELY(reader.eof()) {
		CAT_DEBUG_EXCEPTION();
		return GCIF_RE_BAD_MONO;
	}

	_current_row = _params.data;

	return GCIF_RE_OK;
}

int MonoReader::readRowHeader(u16 y, ImageReader & CAT_RESTRICT reader) {
	// If using row filters instead of tiled filters,
	if (_use_row_filters) {
		CAT_DEBUG_ENFORCE(RF_COUNT == 2);

		// If different row filter on each row,
		if (!_one_row_filter) {
			// Read row filter
			_row_filter = reader.readBit();
		}

		// Reset previous to zero
		_prev_filter = 0;
	} else {
		// If at the start of a tile row,
		if ((y & _tile_mask_y) == 0) {
			if (y > 0) {
				// For each pixel in seen row,
				for (u16 tx = 0; tx < _tiles_x; ++tx) {
					if (!_filter_row[tx].safe) {
						_filter_decoder->zero(tx);
					}
				}

				// Next tile row
				_current_tile += _tiles_x;
			}

			// Read its header too
			_filter_decoder->readRowHeader(y >> _tile_bits_y, reader);

			// Clear filter function row
			_filter_row.fill_00();
		}
	}

	_current_y = y;

	// Reset next non-LZ pixel
	_lz_xend = 0;

	if (y > 0) {
		_current_row += _params.xsize;
	}

	DESYNC(0, y);

	CAT_DEBUG_ENFORCE(!reader.eof());

	return GCIF_RE_OK;
}

u8 MonoReader::read_lz_row_filter(u16 code, ImageReader & CAT_RESTRICT reader, u16 x, u8 * CAT_RESTRICT data) {
	// Decode LZ match
	u32 dist;
	int len = _lz.read(code, reader, dist);

	DESYNC(x, 0);

	// If LZ match starts before start of image,
	if CAT_UNLIKELY(data < _params.data + dist) {
		CAT_DEBUG_EXCEPTION();
		return 0;
	}

	// If LZ match exceeds raster width,
	if CAT_UNLIKELY(x + len > _params.xsize) {
		CAT_DEBUG_EXCEPTION();
		return 0;
	}

	// Simple memory move to get it where it needs to be
	const volatile u8 *src = data - dist;
	for (int ii = 0; ii < len; ++ii) {
		data[ii] = src[ii];
	}

	// Set LZ skip region
	_lz_xend = x + len;

	// After this LZ skip region, the last value will be the prev filter
	_prev_filter = data[len - 1];

	// Stop here
	return *data;
}

// Edge-safe row filter version
u8 MonoReader::read_row_filter(u16 x, ImageReader & CAT_RESTRICT reader) {
#ifdef CAT_DEBUG
	const u16 y = _current_y;
#endif

	u8 * CAT_RESTRICT data = _current_row + x;

	CAT_DEBUG_ENFORCE(x < _params.xsize && y < _params.ysize);

	// If LZ already copied this data byte,
	if (x < _lz_xend) {
		return *data;
	}

	const u16 num_syms = _params.num_syms;

	// Read filter residual directly
	u16 value = _row_filter_decoder.next(reader);

	// If value is an LZ escape code,
	if (value >= num_syms) {
		// Read LZ code
		return read_lz_row_filter(value - num_syms, reader, x, data);
	}

	DESYNC(x, y);

	// Defilter the filter value
	if (_row_filter == RF_PREV) {
		value += _prev_filter;
		if (value >= num_syms) {
			value -= num_syms;
		}
		_prev_filter = static_cast<u8>( value );
	}

	CAT_DEBUG_ENFORCE(!reader.eof());

	return ( *data = static_cast<u8>( value ) );
}

// Safe tiled version, for edges of image
u8 MonoReader::read_tile_safe(u16 x, ImageReader & CAT_RESTRICT reader) {
#ifdef CAT_DEBUG
	const u16 y = _current_y;
#endif

	u8 * CAT_RESTRICT data = _current_row + x;

	CAT_DEBUG_ENFORCE(x < _params.xsize && y < _params.ysize);

	u16 value;
	const u16 num_syms = _params.num_syms;

	// Check cached filter
	const u16 tx = x >> _tile_bits_x;

	// Choose safe/unsafe filter
	MonoFilterFunc filter = _filter_row[tx].safe;

	// If filter is not read yet,
	if (!filter) {
		const u8 f = _filter_decoder_read(tx, reader);

		DESYNC(x, y);

		// Read filter
		MonoFilterFuncs * CAT_RESTRICT funcs = &_sf[f];
		_filter_row[tx] = *funcs;
		filter = funcs->safe; // Choose here
	}

	// If the filter is a palette symbol,
#ifdef CAT_WORD_64
	const u64 pf = (u64)filter;
#else
	const u32 pf = (u32)filter;
#endif
	if (pf <= MAX_PALETTE+1) {
		value = _palette[pf - 1];
		_chaos.zero(x);
	} else {
		// Get chaos bin
		const int chaos = _chaos.get(x);

		// Read residual from bitstream
		const u16 residual = _decoder[chaos].next(reader);

		CAT_DEBUG_ENFORCE(residual < num_syms);

		DESYNC(x, y);

		// Store for next chaos lookup
		_chaos.store(x, static_cast<u8>( residual ), num_syms);

		// Calculate predicted value
		const u16 pred = filter(data, num_syms, x, _current_y, _params.xsize);

		CAT_DEBUG_ENFORCE(pred < num_syms);

		// Defilter using prediction
		value = residual + pred;
		if (value >= num_syms) {
			value -= num_syms;
		}
	}

	CAT_DEBUG_ENFORCE(!reader.eof());

	return ( *data = static_cast<u8>( value ) );
}

// Faster tiled version, when spatial filters can be unsafe
u8 MonoReader::read_tile_unsafe(u16 x, ImageReader & CAT_RESTRICT reader) {
#ifdef CAT_DEBUG
	const u16 y = _current_y;
#endif

	u8 * CAT_RESTRICT data = _current_row + x;

	CAT_DEBUG_ENFORCE(x < _params.xsize && y < _params.ysize);

	u16 value;
	const u16 num_syms = _params.num_syms;

	// Check cached filter
	const u16 tx = x >> _tile_bits_x;

	// Choose safe/unsafe filter
	MonoFilterFunc filter = _filter_row[tx].unsafe;

	// If filter is not read yet,
	if (!filter) {
		const u8 f = _filter_decoder_read(tx, reader);

		DESYNC(x, y);

		// Read filter
		MonoFilterFuncs * CAT_RESTRICT funcs = &_sf[f];
		_filter_row[tx] = *funcs;
		filter = funcs->unsafe; // Choose here
	}

	// If the filter is a palette symbol,
#ifdef CAT_WORD_64
	const u64 pf = (u64)filter;
#else
	const u32 pf = (u32)filter;
#endif
	if (pf <= MAX_PALETTE+1) {
		value = _palette[pf - 1];
		_chaos.zero(x);
	} else {
		// Get chaos bin
		const int chaos = _chaos.get(x);

		// Read residual from bitstream
		const u16 residual = _decoder[chaos].next(reader);

		DESYNC(x, y);

		CAT_DEBUG_ENFORCE(residual < num_syms);

		// Store for next chaos lookup
		_chaos.store(x, static_cast<u8>( residual ), num_syms);

		// Calculate predicted value
		const u16 pred = filter(data, num_syms, x, _current_y, _params.xsize);

		CAT_DEBUG_ENFORCE(pred < num_syms);

		// Defilter using prediction
		value = residual + pred;
		if (value >= num_syms) {
			value -= num_syms;
		}
	}

	CAT_DEBUG_ENFORCE(!reader.eof());

	return ( *data = static_cast<u8>( value ) );
}

u8 MonoReader::read_lz_tile(u16 code, ImageReader & CAT_RESTRICT reader, u16 x, u8 * CAT_RESTRICT data) {
	// Decode LZ match
	u32 dist;
	int len = _lz.read(code, reader, dist);

	DESYNC(x, 0);

	// If LZ match starts before start of image,
	if CAT_UNLIKELY(data < _params.data + dist) {
		CAT_DEBUG_EXCEPTION();
		return 0;
	}

	// If LZ match exceeds raster width,
	if CAT_UNLIKELY(x + len > _params.xsize) {
		CAT_DEBUG_EXCEPTION();
		return 0;
	}

	// Simple memory move to get it where it needs to be
	const volatile u8 *src = data - dist;
	for (int ii = 0; ii < len; ++ii) {
		data[ii] = src[ii];
	}

	// Set LZ skip region
	_lz_xend = x + len;

	// Clear zero region
	_chaos.zeroRegion(x, len);

	// Stop here
	return *data;
}

// Safe tiled version, for edges of image
u8 MonoReader::read_tile_safe_lz(u16 x, ImageReader & CAT_RESTRICT reader) {
#ifdef CAT_DEBUG
	const u16 y = _current_y;
#endif

	u8 * CAT_RESTRICT data = _current_row + x;

	CAT_DEBUG_ENFORCE(x < _params.xsize && y < _params.ysize);

	// If LZ already copied this data byte,
	if (x < _lz_xend) {
		return *data;
	}

	u16 value;
	const u16 num_syms = _params.num_syms;

	// Check cached filter
	const u16 tx = x >> _tile_bits_x;

	// Choose safe/unsafe filter
	MonoFilterFunc filter = _filter_row[tx].safe;

	u16 residual;
	bool readResidual = false;

	// If filter is not read yet,
	if (!filter) {
		// Get chaos bin
		const int chaos = _chaos.get(x);

		// Always read residual first (may be LZ)
		residual = _decoder[chaos].next(reader);
		readResidual = true;

		// If residual is LZ escape code,
		if (residual >= num_syms) {
			// Read LZ match
			return read_lz_tile(residual - num_syms, reader, x, data);
		}

		DESYNC(x, y);

		const u8 f = _filter_decoder_read(tx, reader);

		DESYNC(x, y);

		// Read filter
		MonoFilterFuncs * CAT_RESTRICT funcs = &_sf[f];
		_filter_row[tx] = *funcs;
		filter = funcs->safe; // Choose here
	}

	// If the filter is a palette symbol,
#ifdef CAT_WORD_64
	const u64 pf = (u64)filter;
#else
	const u32 pf = (u32)filter;
#endif
	if (pf <= MAX_PALETTE+1) {
		value = _palette[pf - 1];
		_chaos.zero(x);
	} else {
		// If did not read residual above,
		if (!readResidual) {
			// Get chaos bin
			const int chaos = _chaos.get(x);

			// Read residual from bitstream
			residual = _decoder[chaos].next(reader);
		}

		// If residual is LZ escape code,
		if (residual >= num_syms) {
			// Read LZ match
			return read_lz_tile(residual - num_syms, reader, x, data);
		}

#ifdef CAT_DESYNCH_CHECKS
		if (!readResidual) {
			DESYNC(x, y);
		}
#endif

		// Store for next chaos lookup
		_chaos.store(x, static_cast<u8>( residual ), num_syms);

		// Calculate predicted value
		const u16 pred = filter(data, num_syms, x, _current_y, _params.xsize);

		CAT_DEBUG_ENFORCE(pred < num_syms);

		// Defilter using prediction
		value = residual + pred;
		if (value >= num_syms) {
			value -= num_syms;
		}
	}

	CAT_DEBUG_ENFORCE(!reader.eof());

	return ( *data = static_cast<u8>( value ) );
}

// Faster tiled version, when spatial filters can be unsafe
u8 MonoReader::read_tile_unsafe_lz(u16 x, ImageReader & CAT_RESTRICT reader) {
#ifdef CAT_DEBUG
	const u16 y = _current_y;
#endif

	u8 * CAT_RESTRICT data = _current_row + x;

	CAT_DEBUG_ENFORCE(x < _params.xsize && y < _params.ysize);

	// If LZ already copied this data byte,
	if (x < _lz_xend) {
		return *data;
	}

	u16 value;
	const u16 num_syms = _params.num_syms;

	// Check cached filter
	const u16 tx = x >> _tile_bits_x;

	// Choose safe/unsafe filter
	MonoFilterFunc filter = _filter_row[tx].unsafe;

	u16 residual;
	bool readResidual = false;

	// If filter is not read yet,
	if (!filter) {
		// Get chaos bin
		const int chaos = _chaos.get(x);

		// Always read residual first (may be LZ)
		residual = _decoder[chaos].next(reader);
		readResidual = true;

		DESYNC(x, y);

		// If residual is LZ escape code,
		if (residual >= num_syms) {
			// Read LZ match
			return read_lz_tile(residual - num_syms, reader, x, data);
		}

		const u8 f = _filter_decoder_read(tx, reader);

		DESYNC(x, y);

		// Read filter
		MonoFilterFuncs * CAT_RESTRICT funcs = &_sf[f];
		_filter_row[tx] = *funcs;
		filter = funcs->unsafe; // Choose here

		DESYNC(x, y);
	}

	// If the filter is a palette symbol,
#ifdef CAT_WORD_64
	const u64 pf = (u64)filter;
#else
	const u32 pf = (u32)filter;
#endif
	if (pf <= MAX_PALETTE+1) {
		value = _palette[pf - 1];
		_chaos.zero(x);
	} else {
		// If did not read residual above,
		if (!readResidual) {
			// Get chaos bin
			const int chaos = _chaos.get(x);

			// Read residual from bitstream
			residual = _decoder[chaos].next(reader);

			DESYNC(x, y);
		}

		// If residual is LZ escape code,
		if (residual >= num_syms) {
			// Read LZ match
			return read_lz_tile(residual - num_syms, reader, x, data);
		}

		// Store for next chaos lookup
		_chaos.store(x, static_cast<u8>( residual ), num_syms);

		// Calculate predicted value
		const u16 pred = filter(data, num_syms, x, _current_y, _params.xsize);

		CAT_DEBUG_ENFORCE(pred < num_syms);

		// Defilter using prediction
		value = residual + pred;
		if (value >= num_syms) {
			value -= num_syms;
		}
	}

	CAT_DEBUG_ENFORCE(!reader.eof());

	return ( *data = static_cast<u8>( value ) );
}

