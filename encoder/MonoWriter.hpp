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
#include "../decoder/MonoReader.hpp"
#include "ImageWriter.hpp"
#include "../decoder/Filters.hpp"
#include "EntropyEncoder.hpp"
#include "GCIFWriter.h"
#include "../decoder/Delegates.hpp"
#include "../decoder/SmartArray.hpp"
#include "PaletteOptimizer.hpp"
#include "LZMatchFinder.hpp"

#include <vector>

/*
 * Game Closure Fractal Monochrome (GC-FM) Image Compression
 *
 * Used to compress any monochrome data that is generated during compression.
 *
 * Produces a subresolution filter matrix that is used to filter and reduce
 * entropy of the input matrix.  This filter decision matrix is then
 * recursively compressed.
 *
 * The algorithm is:
 *
 * (1) Determine which tiles are completely masked out so they can be ignored.
 *
 * (2) Design palette filters, which are filters that emit color data directly
 * for blocks that are all the same color.  Especially useful for pixel alpha
 * data, since most of the image is visible.
 *
 * (3) Decide the subset of spatial filters / palette filters to use.  The
 * first SF_FIXED spatial filters are always used.  Up to 32 filters may be
 * used in this step.
 *
 * (4) For palette filters that are used in step 3, fill in the tiles matrix
 * with those decisions up front.
 *
 * (5) For tiles not masked in step 1 or 4, decide which spatial filters to use
 * by comparing the resulting entropy for the encoded pixels.
 *
 * (6) Compute the residual data after applying filters to the entire image.
 *
 * (7) Optimize the indices of the spatial and palette filters so that the
 * resulting tile matrix is more easily compressed.  This does not affect the
 * residuals for the image matrix at all, but should improve the compression of
 * the filter decision tiles matrix.
 *
 * (8) Design simple predictive row filters for the tiles matrix as a fall-back
 * in case recursion is too expensive for compressing the tiles matrix.
 *
 * (9) Attempt to recurse by compressing the tiles matrix as a monochrome
 * paletted "indexed" image.  If the simulated bits required to represent it
 * using the recursive method fail to be less than the simple predictive row
 * filters chosen in step 8, then do not use a recursive approach.  Will also
 * not bother doing a recursive approach below a certain size.
 *
 * (10) Determine the number of chaos levels to use when encoding the image
 * data, between 1 and 16.  More chaos levels means more overhead but likely
 * also better compression of the image data.
 *
 * (11) Repeat it all again from step 1 for various tile sizes and pick the
 * best tile size.
 */

namespace cat {

class MonoWriter;
class MonoWriterProfile;


//// MonoWriter
	
class MonoWriter {
public:
	static const int MAX_FILTERS = MonoReader::MAX_FILTERS;
	static const int MAX_CHAOS_LEVELS = MonoReader::MAX_CHAOS_LEVELS;
	static const int MAX_PALETTE = MonoReader::MAX_PALETTE;
	static const int MAX_SYMS = MonoReader::MAX_SYMS;
	static const int ZRLE_SYMS = MonoReader::ZRLE_SYMS;

	static const int MAX_AWARDS = 8;	// Maximum filters to award

	// bool IsMasked(u16 x, u16 y)
	typedef Delegate2<bool, u16, u16> MaskDelegate;

	// Parameters provided to process()
	struct Parameters {
		// Shared
		u16 xsize, ysize;				// Data dimensions
		u16 min_bits, max_bits;			// Tile size bit range to try
		MaskDelegate mask;				// Function to call to determine if an element is masked out
		u16 num_syms;					// Number of symbols in data [0..num_syms-1]
		bool lz_enable;					// Enable LZ encoder

		// Encoder-only
		const GCIFKnobs *knobs;			// Global knobs
		const u8 *data;					// Input data
		const u16 *write_order;			// Write order for input pixels
		u16 max_filters;				// Maximum number of filters to use
		float sympal_thresh;			// Normalized coverage to add a symbol palette filter (1.0 = entire image)
		float filter_cover_thresh;		// 0.6 Normalized coverage to stop adding filters (1.0 = entire image)
		float filter_inc_thresh;		// 0.05 Normalized coverage increment to stop adding filters
		u32 AWARDS[MAX_AWARDS];			// Awards to give for top N filters
		int award_count;				// Number of awards to give out
	};

	struct _Stats {
		int basic_overhead_bits;
		int encoder_overhead_bits;
		int filter_overhead_bits;
		int data_bits;
		int lz_bits, lz_table_bits;
	} Stats;

protected:
	static const int MAX_PASSES = 4;
	static const int MAX_ROW_PASSES = 4;
	static const int TILE_THRESH = 32*32;	// Pixel count minimum to use natural compressor
	static const int LZ_THRESH = 32*32;		// Pixel count minimum to use LZ77 compressor

	static const u8 UNUSED_SYMPAL = 255;
	static const int ORDER_SENTINEL = 0xffff;

	// Parameters
	Parameters _params;						// Input parameters
#ifdef CAT_DEBUG
	const u16 *_next_write_tile_order;		// For validating write order
	const u16 *_next_write_pixel_order;		// For validating write order
#endif

	MonoWriterProfile *_profile;			// Selected write profile

	// Workspace
	bool _lz_enable;						// Enable LZ during processing
	int _tile_bits_field_bc;				// Bits for tile bits field
	u8 _sympal_filter_map[MAX_PALETTE];		// Filter index for this palette entry
	u8 _prev_filter;						// Previous filter for row encoding
	MonoMatchFinder _lz;					// LZ match finder
	LZMatchFinder::LZMatch *_lz_next;		// Next LZ match
	PaletteOptimizer _optimizer;			// Optimizer for filter indices
	u32 _residual_entropy;					// Calculated entropy of residuals
	SmartArray<u8> _ecodes;					// Used when evaluating options
	SmartArray<u8> _tile_seen;				// Tile seen yet during a tile row
	SmartArray<u8> _replay;					// Used during computing residuals

	// Row filter mode
	SmartArray<u8> _row_filters;			// Selected row filters
	u32 _row_filter_entropy;				// Calculated entropy from using row filters
	EntropyEncoder _row_filter_encoder;
	bool _use_row_filters;					// Using row filters?
	bool _one_row_filter;					// Only one row filter?

	// LZ workspace
	SmartArray<u8> _prices;					// Cost of encoding each pixel without LZ

	// Mask function for child instance
	bool IsMasked(u16 x, u16 y);

	// Calculate natural compressor cost upper bound per pixel
	void priceResiduals();

	// Check for LZ matches
	void designLZ();

	// Try out a simple row filter for input data
	void designRowFilters();

	// Initializes the _mask matrix to easily look up entirely masked out tiles
	void maskTiles();

	// Choose a number of palette filters to try in addition to the usual ones
	void designPaletteFilters();

	// Choose which filters to use on entire data
	void designFilters();

	// Tiles where sympal can be used are obvious choices so do those up front
	void designPaletteTiles();

	// Choose which filters to use which tiles
	void designTiles();

	// Generate write order matrix for filter data
	void generateWriteOrder();

	// Run filters to generate residual data (optimization)
	void computeResiduals();

	// Optimize the spatial filter sorting
	void optimizeTiles();

	// Compress the tile data if possible
	void recurseCompress();

	// Determine number of chaos levels to use when encoding the data
	void designChaos();

	// Simulate number of bits required to encode the data this way
	u32 simulate();

	// Free dynamic objects
	void cleanup();

	// Get filter and write it if needed
	u8 writeFilter(u16 x, u16 y, ImageWriter &writer, int &overhead_bits); // Returns filter for pixel

	// So this bit is pretty complicated.  Sorry.
	// The idea is that PF tiles emit zero symbols on the first unmasked pixel
	// as a trade-off for more efficient LZ match encoding.
	bool sympalCovered(u16 x, u16 y);

public:
	CAT_INLINE MonoWriter() {
		_profile = 0;
	}
	CAT_INLINE virtual ~MonoWriter() {
		cleanup();
	}

	// Generate write order to pass in
	static void generateWriteOrder(u16 xsize, u16 ysize, MaskDelegate mask, u16 tile_shift_bits, std::vector<u16> &order);

	// Generate writer from this configuration
	void init(const Parameters &params);

	// Write parameter tables for decoder
	int writeTables(ImageWriter &writer);

	// Writer header for a row that is just starting
	int writeRowHeader(u16 y, ImageWriter &writer); // Returns bits used

	// Write a symbol
	int write(u16 x, u16 y, ImageWriter &writer); // Returns bits used

	// Indicate a masked symbol
	void zero(u16 x);

	void dumpStats();
};


//// MonoWriterProfile

class MonoWriterProfile {
	friend class MonoWriter;

	static const int MAX_FILTERS = MonoWriter::MAX_FILTERS;
	static const int MAX_PALETTE = MonoWriter::MAX_PALETTE;
	static const int ZRLE_SYMS = MonoWriter::ZRLE_SYMS;
	static const int MAX_SYMS = MonoWriter::MAX_SYMS;
	static const int MAX_CHAOS_LEVELS = MonoWriter::MAX_CHAOS_LEVELS;

	// Generated filter tiles
	SmartArray<u8> mask;					// Masked tile boolean matrix
	SmartArray<u8> tiles;					// Filter chosen per tile matrix
	std::vector<u16> write_order;			// Write order for output tiles
	u32 tiles_count;						// Number of tiles
	int tiles_x, tiles_y;					// Tiles in x,y
	u16 tile_bits_x, tile_bits_y;			// Number of bits in size
	u16 tile_xsize, tile_ysize;			// Size of tile

	CAT_INLINE u8 getTile(u16 tx, u16 ty) {
		return tiles[tx + ty * tiles_x];
	}

	// Residuals
	SmartArray<u8> residuals;

	// Filter choices
	int filter_indices[MAX_FILTERS];		// First MF_FIXED are always the same
	MonoFilterFuncs filters[MAX_FILTERS];	// Chosen filters
	int normal_filter_count;				// Number of normal filters
	int filter_count;						// Total filters chosen

	// Palette filters
	u8 sympal[MAX_PALETTE];					// Palette filter values
	int sympal_filter_count;				// Number of palette filters

	// Filter encoder
	MonoWriter *filter_encoder;				// Child instance

	struct Encoders {
		u32 bits;							// Bits required to encode residuals
		MonoChaos chaos;					// Chaos bin lookup table
		EntropyEncoder encoder[MAX_CHAOS_LEVELS];
	} *encoders;

	void cleanup();

public:
	CAT_INLINE MonoWriterProfile() {
		filter_encoder = 0;
		encoders = 0;
	}
	CAT_INLINE virtual ~MonoWriterProfile() {
		cleanup();
	}

	void init(u16 xsize, u16 ysize, u16 bits);
};


} // namespace cat

#endif // MONO_WRITER_HPP

