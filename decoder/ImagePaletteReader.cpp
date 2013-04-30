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

#include "ImagePaletteReader.hpp"
#include "EndianNeutral.hpp"
#include "EntropyDecoder.hpp"
#include "GCIFReader.h"
#include "Filters.hpp"
using namespace cat;

#ifdef CAT_COLLECT_STATS
#include "../encoder/Log.hpp"
#include "../encoder/Clock.hpp"

static cat::Clock *m_clock = 0;
#endif // CAT_COLLECT_STATS


//// ImagePaletteReader

int ImagePaletteReader::readPalette(ImageReader &reader) {
	// If disabled,
	if (!reader.readBit()) {
		_palette_size = 0;
		return GCIF_RE_OK;
	}

	// Read palette size
	_palette_size = reader.readBits(8) + 1;

	// If using compressed palette,
	if (reader.readBit()) {
		// Read color filter
		u32 cf = reader.readBits(4);
		CAT_DEBUG_ENFORCE(CF_COUNT == 16);
		YUV2RGBFilterFunction filter = YUV2RGB_FILTERS[cf];

		// Initialize the decoder
		EntropyDecoder<PALETTE_MAX, ENCODER_ZRLE_SYMS> decoder;
		if (!decoder.init(reader)) {
			return GCIF_RE_BAD_PAL;
		}

		// For each palette color,
		for (int ii = 0, iiend = _palette_size; ii < iiend; ++ii) {
			// Decode
			u8 yuv[3];
			yuv[0] = decoder.next(reader);
			yuv[1] = decoder.next(reader);
			yuv[2] = decoder.next(reader);
			u8 a = decoder.next(reader);

			// Unfilter
			u8 rgb[3];
			filter(yuv, rgb);

			// Rebuild
			u32 color = a;
			color <<= 8;
			color |= rgb[2];
			color <<= 8;
			color |= rgb[1];
			color <<= 8;
			color |= rgb[0];

			// Store
			_palette[ii] = getLE(color);
		}
	} else {
		// Read palette without compression
		for (int ii = 0, iiend = _palette_size; ii < iiend; ++ii) {
			_palette[ii] = getLE(reader.readWord());
		}
	}

	if (reader.eof()) {
		return GCIF_RE_BAD_PAL;
	}

	return GCIF_RE_OK;
}

int ImagePaletteReader::read(ImageReader &reader) {
#ifdef CAT_COLLECT_STATS
	m_clock = Clock::ref();

	double t0 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	int err;
	if ((err = readPalette(reader))) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t1 = m_clock->usec();

	Stats.readUsec = t1 - t0;
	Stats.colorCount = _palette_size;
#endif // CAT_COLLECT_STATS

	return GCIF_RE_OK;
}

#ifdef CAT_COLLECT_STATS

bool ImagePaletteReader::dumpStats() {
	if (!enabled()) {
		CAT_INANE("stats") << "(Palette Decode)    Disabled.";
	} else {
		CAT_INANE("stats") << "(Palette Decode)     Read Time : " << Stats.readUsec << " usec";
		CAT_INANE("stats") << "(Palette Decode)  Palette Size : " << Stats.colorCount << " colors";
	}

	return true;
}

#endif

