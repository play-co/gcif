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
#include "EntropyDecoder.hpp"
#include "GCIFReader.h"
#include "Delegates.hpp"

#include <vector>

/*
 * Monochrome Decompression
 */

namespace cat {


//// MonoReader 

class MonoReader {
public:
	static const int MAX_FILTERS = 32;
	static const int MAX_CHAOS_LEVELS = 16;
	static const int MAX_PALETTE = 16;
	static const int MAX_SYMS = 256;
	static const int ZRLE_SYMS = 16;

	// 2-bit row filters
	enum RowFilters {
		RF_NOOP,
		RF_A,	// Left
		RF_B,	// Up
		RF_C,	// Up-Left

		RF_COUNT
	};

	// bool IsMasked(u16 x, u16 y)
	typedef Delegate2<bool, u16, u16> MaskDelegate;

	struct Parameters {
		// Shared
		u16 size_x, size_y;				// Data dimensions
		u16 min_bits, max_bits;			// Tile size bit range to try
		MaskDelegate mask;				// Function to call to determine if an element is masked out
		u16 num_syms;					// Number of symbols in data [0..num_syms-1]

		// Decoder-only
	};

protected:
	Parameters _params;

	u8 _sympal_filters[MAX_PALETTE];
	struct FilterSelection {
		MonoFilterFuncs sf;

		CAT_INLINE bool ready() {
			return sf.safe != 0;
		}
	} _filters[MAX_FILTERS];
	int _normal_filter_count;
	int _sympal_filter_count;
	int _filter_count;

	MonoReader *_filter_decoder;
	u8 *_tile_row_filters;					// One for each tile row

	MonoChaos _chaos;						// Chaos bin lookup table
	EntropyDecoder<MAX_SYMS, ZRLE_SYMS> _encoder[MAX_CHAOS_LEVELS];

	void cleanup();

	int readTables(ImageReader &reader);
	int readPixels(ImageReader &reader);

public:
	CAT_INLINE MonoReader() {
		_tile_row_filters = 0;
		_filter_decoder = 0;
	}
	CAT_INLINE virtual ~MonoReader() {
		cleanup();
	}

	int read(Parameters &params, ImageReader &reader);
};


} // namespace cat

#endif // MONO_READER_HPP

