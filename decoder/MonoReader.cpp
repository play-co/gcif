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
	if (_filter_decoder) {
		delete _filter_decoder;
		_filter_decoder = 0;
	}
}

int MonoReader::readTables(const Parameters &params, ImageReader &reader) {
	_params = params;

	// Calculate bits to represent tile bits field
	u32 range = (_params.max_bits - _params.min_bits);
	int bits_bc = 0;
	if (range > 0) {
		bits_bc = BSR32(range) + 1;

		int tile_bits_field_bc = bits_bc;

		// Read tile bits
		_tile_bits_x = reader.readBits(tile_bits_field_bc) + _params.min_bits;

		// Validate input
		if (_tile_bits_x > _params.max_bits) {
			CAT_DEBUG_EXCEPTION();
			return GCIF_RE_BAD_MONO;
		}
	} else {
		_tile_bits_x = _params.min_bits;
	}

	DESYNC_TABLE();

	// Initialize tile sizes
	_tile_bits_y = _tile_bits_x;
	_tile_size_x = 1 << _tile_bits_x;
	_tile_size_y = 1 << _tile_bits_y;
	_tile_mask_x = _tile_size_x - 1;
	_tile_mask_y = _tile_size_y - 1;
	_tiles_x = (_params.size_x + _tile_size_x - 1) >> _tile_bits_x;
	_tiles_y = (_params.size_y + _tile_size_y - 1) >> _tile_bits_y;

	_tiles.resize(_tiles_x * _tiles_y);
	_tiles.fill_ff(); // READ_TILE

	// Read palette
	_sympal_filter_count = reader.readBits(4) + 1;
	for (int ii = 0; ii < _sympal_filter_count; ++ii) {
		_palette[ii] = reader.readBits(8);
	}

	CAT_OBJCLR(_palette);

	DESYNC_TABLE();

	// Initialize fixed filters
	for (int ii = 0; ii < SF_FIXED; ++ii) {
		_sf[ii] = MONO_FILTERS[ii];
	}

	// Read normal filters
	_normal_filter_count = reader.readBits(5) + SF_FIXED;
	for (int ii = SF_FIXED; ii < _normal_filter_count; ++ii) {
		u8 sf = reader.readBits(7);

		if (sf >= SF_COUNT) {
			CAT_DEBUG_EXCEPTION();
			return GCIF_RE_BAD_MONO;
		}

		_sf[ii] = MONO_FILTERS[sf];
	}
	_filter_count = _normal_filter_count + _sympal_filter_count;

	DESYNC_TABLE();

	// Read chaos levels
	int chaos_levels = reader.readBits(5) + 1;
	_chaos.init(chaos_levels, _params.size_x);

	DESYNC_TABLE();

	// For each chaos level,
	for (int ii = 0; ii < chaos_levels; ++ii) {
		if (!_decoder[ii].init(reader)) {
			CAT_DEBUG_EXCEPTION();
			return GCIF_RE_BAD_MONO;
		}
	}

	DESYNC_TABLE();

	// If recursively encoded,
	if (reader.readBit()) {
		// Create a recursive decoder if needed
		if (!_filter_decoder) {
			_filter_decoder = new MonoReader;
		}

		Parameters params;
		params.data = _tiles.get();
		params.size_x = _tiles_x;
		params.size_y = _tiles_y;
		params.min_bits = _params.min_bits;
		params.max_bits = _params.max_bits;
		params.num_syms = MAX_FILTERS;

		int err;
		if ((err = _filter_decoder->readTables(params, reader))) {
			return err;
		}
	} else {
		// Ensure that old filter decoder is erased
		if (_filter_decoder) {
			delete _filter_decoder;
			_filter_decoder = 0;
		}

		if (!_row_filter_decoder.init(reader)) {
			return GCIF_RE_BAD_MONO;
		}
	}

	DESYNC_TABLE();

	CAT_DEBUG_ENFORCE(!reader.eof());

	_current_data = _params.data;
	_current_tile = _tiles.get();

	return GCIF_RE_OK;
}

int MonoReader::readRowHeader(u16 y, ImageReader &reader) {
	_chaos.startRow();

	// If at the start of a tile row,
	if ((y & _tile_mask_y) == 0) {
		// If using recursive decoder,
		if (_filter_decoder) {
			// Read its header too
			_filter_decoder->readRowHeader(y >> _tile_bits_y, reader);
		} else {
			// Read row filter
			_row_filter = reader.readBit();
		}
	}

	DESYNC_FILTER(0, y);

	CAT_DEBUG_ENFORCE(!reader.eof());

	_last_seen_tile_x = -1;

	return GCIF_RE_OK;
}

void MonoReader::masked(u16 x, u16 y, ImageReader &reader) {
	_chaos.zero();

	// Skip this one
	_current_data += _params.data_step;
}

u8 MonoReader::read(u16 x, u16 y, ImageReader &reader) {
	CAT_DEBUG_ENFORCE(x < _size_x && y < _size_y);

	// Check cached filter
	const u16 tx = x >> _tile_bits_x;

	// If now in a new filter tile,
	u8 f;
	if (tx == _last_seen_tile_x) {
		f = _current_filter;
	} else {
		_last_seen_tile_x = tx;
		const u16 ty = y >> _tile_bits_y;

		// If filter is recursively compressed,
		if (_filter_decoder) {
			f = _filter_decoder->read(tx, ty, reader);
		} else {
			// Read filter residual directly
			f = _row_filter_decoder.next(reader);

			// Defilter the filter value
			if (_row_filter == RF_PREV) {
				f += _current_filter;
				if (f >= _filter_count) {
					f -= _filter_count;
				}
			}

			*_current_tile++ = f;
		}

		_current_filter = f;
	}

	// If the filter is a palette symbol,
	u16 value;
	if (f >= _normal_filter_count) {
		f -= _normal_filter_count;

		CAT_DEBUG_ENFORCE(f < _sympal_filter_count);

		// Set up so that this is always within array bounds at least
		value = _palette[f];

		_chaos.zero();
	} else {
		// Get chaos bin
		int chaos = _chaos.get();

		// Read residual from bitstream
		u16 residual = _decoder[chaos].next(reader);

		// Store for next chaos lookup
		const u16 num_syms = _params.num_syms;
		_chaos.store(residual, num_syms);

		// Read filter result
		u16 pred = _sf[f].safe(_current_data, num_syms - 1, x, y, _params.size_x);

		// Defilter value
		value = residual + pred;
		if (value >= num_syms) {
			value -= num_syms;
		}
	}

	DESYNC(x, y);

	CAT_DEBUG_ENFORCE(!reader.eof());

	*_current_data = (u8)value;
	_current_data += _params.data_step;
	return value;
}

