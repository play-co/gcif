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
   Through working on compressing 2D RGBA data I've noticed a few parts of the data are effectively monochrome data:
   (1) Which color filter to use for each SF/CF filter zone.
   (2) Which spatial filter to use for each SF/CF filter zone.
   (3) Palette mode encoding.
   (4) The alpha channel encoding.
   (5) Which alpha filter to use for each alpha filter zone. (new)

   What I realized while working more on adding better alpha channel filtering is that it would make a lot of sense to do it recursively.  And that made me step back.  This could be an encoding that recursively produces subresolution monochrome images that are then recompressed!  Any of the modeling that can be done for one of these monochrome data streams may be applicable to any of the others!

   Exciting!

   I've already got a pretty flexible entropy encoder that can do zero-run-length-encoding (zRLE) and order-1 modeling for after-zero symbols, but will also turn these features off if a basic Huffman coder makes more sense.  And the Huffman tables are compressed effectively regardless of the input.  So I can use this same entropy encoder for just about everything now, and it enables me to decide between filter choices just purely using entropy comparison.

   class MonochromeFilter

   The new MonochromeFilter class is going to combine all of the tricks I've picked up:

   (0) Select a power-of-two ZxZ zone size, starting with twice the size of the parent zone.


   (1) Mask Filters

   Provide a delegate that we can call per x,y pixel to check if the data is masked or not, and note that a filter tile is part of masked data so it can be skipped over tile-wise for the rest of the decision process.


   (2) Spatial Filters

   There will be a whole bunch of spatial filters that are complex, as well as a large set of tapped linear filters to swap in where it makes sense.  The filters will be listed in order of complexity, with most complex filters are the bottom.

   + "No modification". (degenerate linear tapped) [ALWAYS AVAILABLE]
   + "Same as A, B, C, D". (degenerate linear tapped) [ALWAYS AVAILABLE]
   + About 80 linear tapped filters that seem to do well.
   + All of the ones involving if-statements from the current codebase, plus more.  (At the bottom of preference list).
   + Whole zone is value "X". [Special encoding that requires a byte to be emitted]

   All of the filters will be tried against the input in ZxZ zones, and the top 4 matches will be awarded + REWARD[0..3].  Scored by L1 norm for speed.

   After this process, the filters that are scored above REWARD[0] * ZONE_COUNT * THRESHOLD_PERCENT are taken, up to MAX_FILTERS.  Some of the filters are always chosen regardless of how well they do and are just included in the measurement to avoid rewarding other filters unnecessarily.

   Pixels are passed to the mask delegate to see if they are considered or not.


   (3) Filter selection

   Use entropy analysis to decide which filter to select for each tile, starting in the upper left, to lower right.  After working over the whole 2D data, loop back to the front for 4096 iterations to allow the entropy histogram to stabilize and tune tighter.


   (4) Filter the filters

   For each filter row, select a 2-bit code meaning:

   00 "FF[n] = f[n]"
   01 "FF[n] = f[n] - f[n - 1], or 0"
   10 "FF[n] = f[n] - f[n - width], or 0"
   11 "FF[n] = f[n] - f[n - width - 1], or 0"

   Loop over the whole image twice, minimizing entropy of the FF[n] data.


   (5) Recursively compress the FF[n] data.

   Create a new instance of the MonochromeFilter to compress the resulting FF[n] data.


   (6) Calculate the number of bits required to encode the data in this way

   And loop back to step 0 to see if increasing Z helps.  Stop on the first one that makes it worse.


   Decoding

	Read and initialize the filters.

	Decoding the original data proceeds per scan-line.

	On lines that have (y & (Z-1) == 0), we check to see if FILTERS[x >> Z_BITS] is NULL.  If so, we expect the writer to have written out the filter selection.  We recursively call MonochromeFilter filter reading code, which will have its own FILTERS[] sub-array.  Until eventually it's just directly read in.
 */

namespace cat {


//// MonoWriter 

class MonoWriter {
public:
	static const int AWARD_COUNT = 4;	// Number of filters to award
	static const int MAX_FILTERS = 32;	// Maximum number of filters to use

	// bool IsMasked(u16 x, u16 y)
	typedef Delegate2<bool, u16, u16> MaskDelegate;

	// Parameters provided to process()
	struct Parameters {
		const GCIFKnobs *knobs;			// Global knobs
		const u8 *data;					// Input data
		u16 num_syms;					// Number of symbols in data [0..num_syms-1]
		u16 size_x, size_y;				// Data dimensions
		u16 tile_bits_x, tile_bits_y;	// Number of bits in size
		u16 max_filters;				// Maximum number of filters to use
		MaskDelegate mask;				// Function to call to determine if an element is masked out
		u32 AWARDS[AWARD_COUNT];		// Awards to give for top N filters
	};

protected:
	static const int MAX_SYMS = 256;
	static const int ZRLE_SYMS = 16;
	static const int MAX_CHAOS_LEVELS = 16;

	static const u8 MASK_TILE = 255;
	static const u8 TODO_TILE = 0;

	// Parameters
	const Parameters *_params;
	u16 _tile_bits_x, _tile_bits_y;		// Number of bits in size
	MaskDelegate _mask;					// Function to call to determine if an element is masked out

	// Generated filter tiles
	u8 *_tiles;							// Filter tiles
	u32 _tiles_count;					// Number of tiles
	u16 _tile_size_x, _tile_size_y;		// Number of tiles in each dimension

	// Filter choices
	int _filter_indices[MAX_FILTERS];		// First MF_FIXED are always the same
	MonoFilterFunc _filters[MAX_FILTERS];	// Chosen filters

	// TODO: Have entropy encoder select symbol count in initialization function
	EntropyEncoder<MAX_SYMS, ZRLE_SYMS> _encoder[MAX_CHAOS_LEVELS];

	void cleanup();

	// Set tiles to MASK_TILE or TODO_TILE based on the provided mask (optimization)
	void maskTiles();

	// Choose which filters to use on entire data
	void designFilters();

	// Choose which filters to use which tiles
	void designTiles();

	// Simple predictive row filter for tiles
	void filterTiles();

	// Compress the tile data if possible
	void recurseCompress();

	// Determine number of chaos levels to use when encoding the data
	void designChaos();

public:
	CAT_INLINE MonoWriter() {
		_tiles = 0;
	}
	CAT_INLINE virtual ~MonoWriter() {
		cleanup();
	}

	// Process the data
	bool process(const Parameters *params);

	// Determine number of bits it costs for this representation
	u32 simulate();

	// Write parameter tables for decoder
	void writeTables(ImageWriter &writer);

	// Write a symbol
	void write(u16 x, u16 y, ImageWriter &writer);
};


} // namespace cat

#endif // MONO_WRITER_HPP

