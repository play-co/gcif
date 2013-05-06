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

#ifndef IMAGE_PALETTE_WRITER_HPP
#define IMAGE_PALETTE_WRITER_HPP

#include "../decoder/Platform.hpp"
#include "ImageWriter.hpp"
#include "GCIFWriter.h"
#include "ImageLZWriter.hpp"
#include "ImageMaskWriter.hpp"
#include "../decoder/ImagePaletteReader.hpp"
#include "MonoWriter.hpp"
#include "../decoder/SmartArray.hpp"

#include <vector>
#include <map>

/*
 * Game Closure Global Palette Compression
 */

namespace cat {


//// ImagePaletteWriter

class ImagePaletteWriter {
	static const int PALETTE_MAX = ImagePaletteReader::PALETTE_MAX;
	static const int ENCODER_ZRLE_SYMS = ImagePaletteReader::ENCODER_ZRLE_SYMS;

	const GCIFKnobs *_knobs;

	const u8 *_rgba;		// Original image
	SmartArray<u8> _image;				// Palette-encoded image
	int _size_x, _size_y;	// In pixels
	u16 _masked_palette;		// Palette index for the mask

	ImageMaskWriter *_mask;
	ImageLZWriter *_lz;

	std::vector<u32> _palette;
	std::map<u32, u16> _map;
	bool _enabled;

	MonoWriter _mono_writer;

	bool IsMasked(u16 x, u16 y);

	bool generatePalette();
	void sortPalette();
	void generateImage();
	void generateMonoWriter();

	void writeTable(ImageWriter &writer);
	void writePixels(ImageWriter &writer);

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		int palette_size;
		int overhead_bits;
	} Stats;
#endif

public:
	int init(const u8 *rgba, int size_x, int size_y, const GCIFKnobs *knobs, ImageMaskWriter &mask, ImageLZWriter &lz);

	CAT_INLINE bool enabled() {
		return _enabled;
	}

	CAT_INLINE u8 getPaletteFromColor(u32 color) {
		return _map[color];
	}

	CAT_INLINE u32 getColorFromPalette(u8 palette) {
		return _palette[palette];
	}

	CAT_INLINE int getPaletteSize() {
		return (int)_palette.size();
	}

	CAT_INLINE u8 *get(int x, int y) {
		return _image.get() + x + y * _size_x;
	}

	void write(ImageWriter &writer);
#ifdef CAT_COLLECT_STATS
	bool dumpStats();
#else
	CAT_INLINE bool dumpStats() {
		return false;
	}
#endif
};


} // namespace cat

#endif // IMAGE_PALETTE_WRITER_HPP

