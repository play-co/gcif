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

#ifndef MONO_WRITER_HPP
#define MONO_WRITER_HPP

#include "../decoder/Platform.hpp"
#include "ImageWriter.hpp"
#include "../decoder/ImageMaskReader.hpp"
#include "../decoder/ImageLZReader.hpp"
#include "../decoder/Filters.hpp"
#include "EntropyEncoder.hpp"
#include "GCIFWriter.h"
#include "Delegates.hpp"

#include <vector>

/*
 * Monochrome Compression
 *
 * Used to compress any monochrome data that is generated during compression.
 *
 * Produces a subresolution filter matrix that is used to filter and reduce
 * entropy of the input matrix.  This matrix is recursively compressed.
 */

namespace cat {


//// MonoWriter 

class MonoWriter {
public:
	static const int MAX_AWARDS = 8;	// Maximum filters to award
	static const int MAX_FILTERS = 32;	// Maximum filters to use

	// bool IsMasked(u16 x, u16 y)
	typedef Delegate2<bool, u16, u16> MaskDelegate;

	// Parameters provided to process()
	struct Parameters {
		const GCIFKnobs *knobs;			// Global knobs
		const u8 *data;					// Input data
		u16 num_syms;					// Number of symbols in data [0..num_syms-1]
		u16 size_x, size_y;				// Data dimensions
		u16 max_filters;				// Maximum number of filters to use
		u16 min_bits, max_bits;			// Tile size bit range to try
		float sympal_thresh;			// Normalized coverage to add a symbol palette filter (1.0 = entire image)
		float filter_thresh;			// Normalized coverage to stop adding filters (1.0 = entire image)
		MaskDelegate mask;				// Function to call to determine if an element is masked out
		u32 AWARDS[MAX_AWARDS];			// Awards to give for top N filters
		int award_count;				// Number of awards to give out
	};

	// 2-bit row filters
	enum RowFilters {
		RF_NOOP,
		RF_A,	// Left
		RF_B,	// Up
		RF_C,	// Up-Left

		RF_COUNT
	};

	struct _Stats {
		int basic_overhead_bits;
		int encoder_overhead_bits;
		int filter_overhead_bits;
		int data_bits;
	} Stats;

protected:
	static const int MAX_SYMS = 256;
	static const int ZRLE_SYMS = 16;
	static const int MAX_CHAOS_LEVELS = 16;
	static const int MAX_PASSES = 4;
	static const int MAX_ROW_PASSES = 4;
	static const int RECURSE_THRESH_COUNT = 128;
	static const int MAX_PALETTE = 16;

	static const u8 MASK_TILE = 255;
	static const u8 TODO_TILE = 0;

	static const u8 UNUSED_SYMPAL = 255;

	// Parameters
	Parameters _params;						// Input parameters

	// Generated filter tiles
	u8 *_tiles;								// Filter tiles
	u32 _tiles_count;						// Number of tiles
	int _tiles_x, _tiles_y;					// Tiles in x,y
	u16 _tile_bits_x, _tile_bits_y;			// Number of bits in size
	u16 _tile_size_x, _tile_size_y;			// Size of tile

	// Residuals
	u8 *_residuals;							// Residual data after applying filters
	u32 _residual_entropy;					// Calculated entropy of residuals

	// Filter choices
	int _filter_indices[MAX_FILTERS];		// First MF_FIXED are always the same
	MonoFilterFuncs _filters[MAX_FILTERS];	// Chosen filters
	int _normal_filter_count;				// Number of normal filters
	int _filter_count;						// Totla filters chosen

	// Palette filters
	u8 _sympal[MAX_PALETTE];				// Palette filter values
	u8 _sympal_filter_map[MAX_PALETTE];		// Filter index for this palette entry
	int _sympal_filter_count;				// Number of palette filters

	// Filter encoder
	MonoWriter *_filter_encoder;			// Child instance
	u8 *_tile_row_filters;					// One for each tile row
	u32 _row_filter_entropy;				// Calculated entropy from using row filters
	u8 *_tile_seen;							// Seen this tile yet during writing?

	// Filter encoder for row mode
	EntropyEncoder<MAX_FILTERS, ZRLE_SYMS> _row_filter_encoder;

	// Chaos levels
	ChaosTable _chaos;						// Chaos bin lookup table
	u32 _chaos_entropy;						// Entropy after chaos applied

	// Write state
	u8 _write_filter;						// Current filter

	// TODO: Have entropy encoder select symbol count in initialization function
	EntropyEncoder<MAX_SYMS, ZRLE_SYMS> _encoder[MAX_CHAOS_LEVELS];

	void cleanup();

	CAT_INLINE u8 getTile(u16 x, u16 y) {
		x >>= _tile_bits_x;
		y >>= _tile_bits_y;
		return _tiles[x + y * _tiles_x];
	}

	// Mask function for child instance
	bool IsMasked(u16 x, u16 y);

	// Set tiles to MASK_TILE or TODO_TILE based on the provided mask (optimization)
	void maskTiles();

	// Choose a number of palette filters to try in addition to the usual ones
	void designPaletteFilters();

	// Choose which filters to use on entire data
	void designFilters();

	// Tiles where sympal can be used are obvious choices so do those up front
	void designPaletteTiles();

	// Choose which filters to use which tiles
	void designTiles();

	// Run filters to generate residual data (optimization)
	void computeResiduals();

	// Simple predictive row filter for tiles
	void designRowFilters();

	// Compress the tile data if possible
	void recurseCompress();

	// Determine number of chaos levels to use when encoding the data
	void designChaos();

	// Load up the encoders with symbol statistics
	void initializeEncoders();

	// Initialize the write engine
	void initializeWriter();

public:
	CAT_INLINE MonoWriter() {
		_tiles = 0;
		_filter_encoder = 0;
		_tile_row_filters = 0;
		_chaos = 0;
		_residuals = 0;
	}
	CAT_INLINE virtual ~MonoWriter() {
		cleanup();
	}

	// Process the data
	u32 process(const Parameters &params);

	// Write parameter tables for decoder
	void writeTables(ImageWriter &writer);

	// Writer header for a row that is just starting
	int writeRowHeader(u16 y, ImageWriter &writer); // Returns bits used

	// Write a symbol
	int write(u16 x, u16 y, ImageWriter &writer); // Returns bits used
};


} // namespace cat

#endif // MONO_WRITER_HPP

