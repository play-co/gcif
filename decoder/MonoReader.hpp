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

#ifndef MONO_READER_HPP
#define MONO_READER_HPP

#include "Platform.hpp"
#include "ImageReader.hpp"
#include "Filters.hpp"
#include "ChaosMetric.hpp"
#include "EntropyDecoder.hpp"
#include "GCIFReader.h"
#include "Delegates.hpp"
#include "LZReader.hpp"

#include <vector>

/*
 * Game Closure Fractal Monochrome (GC-FM) Image Decompression
 *
 * Recursively decompresses filter decision tiles to recover the original
 * filter set to decompress the original image data at the top level.
 */

namespace cat {


//// MonoReader 

class MonoReader {
public:
	static const int MAX_FILTERS = 32;
	static const int MAX_CHAOS_LEVELS = 16;
	static const int MAX_PALETTE = 15;
	static const int MAX_SYMS = 256;
	static const int ZRLE_SYMS = 16;
	static const int HUFF_LUT_BITS = 7;

	enum RowFilters {
		RF_NOOP,		// Pass-through filter
		RF_PREV,		// Predict same as previously emitted spatial filter
		/*
		 * Note: This filter is a little weird because it is not necessarily
		 * the "tile to the left" that it is predicting.  Masking can cause the
		 * previous to actually be to the right..  The important thing here is
		 * that the encoder has verified that this new representation reduces
		 * the entropy of the filter data and has chosen it over just sending
		 * the filter data unmodified.
		 *
		 * Initialized to 0 at the start of each tile row.
		 *
		 * This design decision was made in favor of low decoder complexity.
		 */

		RF_COUNT
	};

	// bool IsMasked(u16 x, u16 y)
	typedef Delegate2<bool, u16, u16> MaskDelegate;

	struct Parameters {
		u8 * CAT_RESTRICT data;			// Output data
		u16 xsize, ysize;				// Data dimensions
		u16 min_bits, max_bits;			// Tile size bit range to try
		u16 num_syms;					// Number of symbols in data [0..num_syms-1]
	};

protected:
	Parameters _params;

	SmartArray<u8> _tiles;
	u16 _tile_xsize, _tile_ysize;
	u16 _tile_bits_x, _tile_bits_y;
	u16 _tile_mask_x, _tile_mask_y;
	u16 _tiles_x, _tiles_y;

	u8 _palette[MAX_PALETTE];
	MonoFilterFuncs _sf[MAX_FILTERS];
	int _filter_count;

	MonoReader * CAT_RESTRICT _filter_decoder;
	SmartArray<MonoFilterFuncs> _filter_row;

	bool _use_row_filters, _one_row_filter;
	u8 _row_filter, _prev_filter;
	EntropyDecoder _row_filter_decoder;

	MonoChaos _chaos;
	EntropyDecoder _decoder[MAX_CHAOS_LEVELS];

	// Decoder state
	u8 *_current_row;
	u16 _current_y;
	u8 *_current_tile;

	// LZ state
	bool _lz_enabled;
	LZReader _lz;

	void cleanup();

public:
	CAT_INLINE MonoReader() {
		_filter_decoder = 0;
	}
	CAT_INLINE virtual ~MonoReader() {
		cleanup();
	}

	int readTables(const Parameters & CAT_RESTRICT params, ImageReader & CAT_RESTRICT reader);

	int readRowHeader(u16 y, ImageReader & CAT_RESTRICT reader);

	CAT_INLINE void setupUnordered() {
		// Set entire matrix to zero to prepare for unordered filter-based reading
		CAT_CLR(_params.data, _params.xsize * _params.ysize);
	}

	CAT_INLINE void zero(u16 x) {
		_chaos.zero(x);
	}

	CAT_INLINE void zeroRegion(u16 x, u16 len) {
		_chaos.zeroRegion(x, len);
	}

	CAT_INLINE u8 *currentRow() {
		return _current_row;
	}

	u8 read(u16 x, ImageReader & CAT_RESTRICT reader);

	// Faster top-level version, when spatial filters can be unsafe
	u8 read_unsafe(u16 x, ImageReader & CAT_RESTRICT reader);
};


} // namespace cat

#endif // MONO_READER_HPP

