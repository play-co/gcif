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


//// ImagePaletteReader

int ImagePaletteReader::readPalette(ImageReader & CAT_RESTRICT reader) {
	// If disabled,
	if (!reader.readBit()) {
		_palette_size = 0;
		return GCIF_RE_OK;
	}

	// Read palette size
	_palette_size = reader.readBits(8) + 1;

	// Read mask palette index
	_mask_palette = reader.readBits(8);

	// If using compressed palette,
	if (reader.readBit()) {
		// Read color filter
		CAT_DEBUG_ENFORCE(CF_COUNT == 17);
		u32 cf = reader.read17();

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
			u8 a = 255 - decoder.next(reader);

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

	DESYNC_TABLE();

	if CAT_UNLIKELY(reader.eof()) {
		return GCIF_RE_BAD_PAL;
	}

	return GCIF_RE_OK;
}

int ImagePaletteReader::readTables(ImageReader & CAT_RESTRICT reader) {
	_image.resize(_size_x * _size_y);

	MonoReader::Parameters params;
	params.data = _image.get();
	params.size_x = _size_x;
	params.size_y = _size_y;
	params.min_bits = 2;
	params.max_bits = 5;
	params.num_syms = _palette_size;
	
	int err = _mono_decoder.readTables(params, reader);

	DESYNC_TABLE();

	if CAT_UNLIKELY(reader.eof()) {
		return GCIF_RE_BAD_PAL;
	}

	return err;
}

int ImagePaletteReader::readPixels(ImageReader & CAT_RESTRICT reader) {
	const u32 MASK_COLOR = _mask->getColor();
	const u8 MASK_PAL = _mask_palette;

	u32 * CAT_RESTRICT rgba = reinterpret_cast<u32 *>( _rgba );
	u8 * CAT_RESTRICT p = _image.get();

	u16 trigger_x_lz = _lz->getTriggerX();

#ifdef CAT_UNROLL_READER

	// Unroll y = 0 scanline
	{
		const int y = 0;

		_mono_decoder.readRowHeader(y, reader);

		if (y == _lz->getTriggerY()) {
			_lz->triggerY();
			trigger_x_lz = _lz->getTriggerX();
		}

		const u32 *mask_next = _mask->nextScanline();
		int mask_left = 0;
		u32 mask;

		int lz_skip = 0;

		for (int x = 0, xend = _size_x; x < xend; ++x) {
			DESYNC(x, y);

			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerX(p, rgba);
				trigger_x_lz = _lz->getTriggerX();
			}

			// Next mask word
			if (mask_left-- <= 0) {
				mask = *mask_next++;
				mask_left = 31;
			}

			if (lz_skip > 0) {
				--lz_skip;
				_mono_decoder.masked(x);
			} else if ((s32)mask < 0) {
				*rgba = MASK_COLOR;
				*p = MASK_PAL;
				_mono_decoder.masked(x);
			} else {
				u8 index = _mono_decoder.read(x, y, p, reader);

				CAT_DEBUG_ENFORCE(index < _palette_size);

				*rgba = _palette[index];
			}

			++rgba;
			mask <<= 1;
			++p;
		}
	}

	// For each remaining scanline,
	for (int y = 1, yend = _size_y; y < yend; ++y) {
		_mono_decoder.readRowHeader(y, reader);

		if (y == _lz->getTriggerY()) {
			_lz->triggerY();
			trigger_x_lz = _lz->getTriggerX();
		}

		const u32 *mask_next = _mask->nextScanline();
		int mask_left;
		u32 mask;

		int lz_skip = 0;

		// Unroll x = 0
		{
			const int x = 0;

			DESYNC(x, y);

			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerX(p, rgba);
				trigger_x_lz = _lz->getTriggerX();
			}

			// Next mask word
			mask = *mask_next++;
			mask_left = 31;

			if (lz_skip > 0) {
				--lz_skip;
				_mono_decoder.masked(x);
			} else if ((s32)mask < 0) {
				*rgba = MASK_COLOR;
				*p = MASK_PAL;
				_mono_decoder.masked(x);
			} else {
				u8 index = _mono_decoder.read(x, y, p, reader);

				CAT_DEBUG_ENFORCE(index < _palette_size);

				*rgba = _palette[index];
			}

			++rgba;
			mask <<= 1;
			++p;
		}

		//// THIS IS THE INNER LOOP ////

		for (int x = 1, xend = (int)_size_x - 1; x < xend; ++x) {
			DESYNC(x, y);

			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerX(p, rgba);
				trigger_x_lz = _lz->getTriggerX();
			}

			// Next mask word
			if (mask_left-- <= 0) {
				mask = *mask_next++;
				mask_left = 31;
			}

			if (lz_skip > 0) {
				--lz_skip;
				_mono_decoder.masked(x);
			} else if ((s32)mask < 0) {
				*rgba = MASK_COLOR;
				*p = MASK_PAL;
				_mono_decoder.masked(x);
			} else {
				u8 index = _mono_decoder.read_unsafe(x, y, p, reader);

				CAT_DEBUG_ENFORCE(index < _palette_size);

				*rgba = _palette[index];
			}

			++rgba;
			mask <<= 1;
			++p;
		}

		//// THIS IS THE INNER LOOP ////

		// Unroll x = _size_x - 1
		{
			const int x = _size_x - 1;

			DESYNC(x, y);

			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerX(p, rgba);
				trigger_x_lz = _lz->getTriggerX();
			}

			// Next mask word
			if (mask_left-- <= 0) {
				mask = *mask_next++;
				mask_left = 31;
			}

			if (lz_skip > 0) {
				--lz_skip;
				_mono_decoder.masked(x);
			} else if ((s32)mask < 0) {
				*rgba = MASK_COLOR;
				*p = MASK_PAL;
				_mono_decoder.masked(x);
			} else {
				u8 index = _mono_decoder.read(x, y, p, reader);

				CAT_DEBUG_ENFORCE(index < _palette_size);

				*rgba = _palette[index];
			}

			++rgba;
			++p;
		}
	}

#else

	for (int y = 0, yend = _size_y; y < yend; ++y) {
		_mono_decoder.readRowHeader(y, reader);

		if (y == _lz->getTriggerY()) {
			_lz->triggerY();
			trigger_x_lz = _lz->getTriggerX();
		}

		const u32 *mask_next = _mask->nextScanline();
		int mask_left = 0;
		u32 mask;

		int lz_skip = 0;

		for (int x = 0, xend = _size_x; x < xend; ++x) {
			DESYNC(x, y);

			// If LZ triggered,
			if (x == trigger_x_lz) {
				lz_skip = _lz->triggerX(p, rgba);
				trigger_x_lz = _lz->getTriggerX();
			}

			// Next mask word
			if (mask_left-- <= 0) {
				mask = *mask_next++;
				mask_left = 31;
			}

			if (lz_skip > 0) {
				--lz_skip;
				_mono_decoder.masked(x);
			} else if ((s32)mask < 0) {
				*rgba = MASK_COLOR;
				*p = MASK_PAL;
				_mono_decoder.masked(x);
			} else {
				// TODO: Unroll to use unsafe version
				u8 index = _mono_decoder.read(x, y, p, reader);

				CAT_DEBUG_ENFORCE(index < _palette_size);

				*rgba = _palette[index];
			}

			++rgba;
			mask <<= 1;
			++p;
		}
	}

#endif

	return GCIF_RE_OK;
}

int ImagePaletteReader::read(ImageReader & CAT_RESTRICT reader, ImageMaskReader & CAT_RESTRICT mask, ImageLZReader & CAT_RESTRICT lz, GCIFImage * CAT_RESTRICT image) {
#ifdef CAT_COLLECT_STATS
	m_clock = Clock::ref();

	double t0 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	int err;
	if ((err = readPalette(reader))) {
		return err;
	}

	// If not enabled,
	if (!enabled()) {
		return GCIF_RE_OK;
	}

#ifdef CAT_COLLECT_STATS
	double t1 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	_rgba = image->rgba;
	_size_x = image->size_x;
	_size_y = image->size_y;
	_mask = &mask;
	_lz = &lz;

	if ((err = readTables(reader))) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t2 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	if ((err = readPixels(reader))) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t3 = m_clock->usec();

	Stats.paletteUsec = t1 - t0;
	Stats.tablesUsec = t2 - t1;
	Stats.pixelsUsec = t3 - t2;
	Stats.colorCount = _palette_size;
#endif // CAT_COLLECT_STATS

	return GCIF_RE_OK;
}

#ifdef CAT_COLLECT_STATS

bool ImagePaletteReader::dumpStats() {
	if (!enabled()) {
		CAT_INANE("stats") << "(Palette Decode)    Disabled.";
	} else {
		CAT_INANE("stats") << "(Palette Decode) Palette Read Time : " << Stats.paletteUsec << " usec";
		CAT_INANE("stats") << "(Palette Decode)  Tables Read Time : " << Stats.tablesUsec << " usec";
		CAT_INANE("stats") << "(Palette Decode)  Pixels Read Time : " << Stats.pixelsUsec << " usec";
		CAT_INANE("stats") << "(Palette Decode)      Palette Size : " << Stats.colorCount << " colors";
	}

	return true;
}

#endif

