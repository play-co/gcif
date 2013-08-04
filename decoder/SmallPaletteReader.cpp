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

#include "SmallPaletteReader.hpp"
#include "EndianNeutral.hpp"
#include "Enforcer.hpp"
using namespace cat;

#ifdef CAT_COLLECT_STATS
#include "../encoder/Log.hpp"
#include "../encoder/Clock.hpp"

static cat::Clock *m_clock = 0;
#endif // CAT_COLLECT_STATS


//// SmallPaletteReader

int SmallPaletteReader::readSmallPalette(ImageReader & CAT_RESTRICT reader) {
	_palette_size = reader.readBits(4) + 1;

	for (int ii = 0; ii < _palette_size; ++ii) {
		_palette[ii] = getLE(reader.readWord());
	}

	if (_palette_size > 4) { // 3-4 bits/pixel
		_pack_x = (_xsize + 1) >> 1;
		_pack_y = _ysize;
	} else if (_palette_size > 2) { // 2 bits/pixel
		_pack_x = (_xsize + 1) >> 1;
		_pack_y = (_ysize + 1) >> 1;
	} else if (_palette_size > 1) { // 1 bit/pixel
		_pack_x = (_xsize + 3) >> 2;
		_pack_y = (_ysize + 1) >> 1;
	} else {
		// Just emit that single color and done!
		int count = _ysize * _xsize;
		const u32 COLOR = _palette[0];
		u32 * CAT_RESTRICT rgba = reinterpret_cast<u32 *>( _rgba );

		// Unroll sets of 4 words for this incredibly important case
		while (count >= 4) {
			rgba[0] = COLOR;
			rgba[1] = COLOR;
			rgba[2] = COLOR;
			rgba[3] = COLOR;
			rgba += 4;
			count -= 4;
		}

		while (count > 0) {
			*rgba++ = COLOR;
			--count;
		}
	}

	return GCIF_RE_OK;
}

int SmallPaletteReader::readPackPalette(ImageReader & CAT_RESTRICT reader) {
	// If mask is enabled,
	if (_mask->enabled()) {
		_mask_palette = reader.readBits(8);
	} else {
		_mask_palette = 0;
	}

	_pack_palette_size = reader.readBits(8) + 1;

	for (int ii = 0; ii < _pack_palette_size; ++ii) {
		_pack_palette[ii] = reader.readBits(8);
	}

	return GCIF_RE_OK;
}

int SmallPaletteReader::readTables(ImageReader & CAT_RESTRICT reader) {
	// Allocate image matrix
	_image.resize(_pack_x * _pack_y);

	// Build parameters for decoder
	MonoReader::Parameters params;
	params.data = _image.get();
	params.xsize = _pack_x;
	params.ysize = _pack_y;
	params.min_bits = 2;
	params.max_bits = 5;
	params.num_syms = _pack_palette_size;

	return _mono_decoder.readTables(params, reader);
}

int SmallPaletteReader::readPixels(ImageReader & CAT_RESTRICT reader) {
	const u8 MASK_PAL = _mask_palette;

#ifdef CAT_UNROLL_READER

	// Unroll y = 0 scanline
	{
		const int y = 0;

		_mono_decoder.readRowHeader(y, reader);

		const u32 *mask_next = _mask->nextScanline();
		int mask_left = 0;
		u32 mask;

		for (int x = 0, xend = _pack_x; x < xend; ++x) {
			// Next mask word
			if (mask_left-- <= 0) {
				mask = *mask_next++;
				mask_left = 31;
			}

			if ((s32)mask < 0) {
				u8 *p = _mono_decoder.currentRow() + x;
				_mono_decoder.zero(x);
				*p = MASK_PAL;
			} else {
#ifdef CAT_DEBUG
				u8 index =
#endif
				_mono_decoder.read(x, reader);

				CAT_DEBUG_ENFORCE(index < _pack_palette_size);
			}

			mask <<= 1;
		}
	}

	// For each remaining scanline,
	for (int y = 1, yend = _pack_y; y < yend; ++y) {
		_mono_decoder.readRowHeader(y, reader);

		const u32 *mask_next = _mask->nextScanline();
		int mask_left;
		u32 mask;

		// Unroll x = 0
		{
			const u16 x = 0;

			// Next mask word
			mask = *mask_next++;
			mask_left = 31;

			if ((s32)mask < 0) {
				u8 *p = _mono_decoder.currentRow() + x;
				_mono_decoder.zero(x);
				*p = MASK_PAL;
			} else {
#ifdef CAT_DEBUG
				u8 index =
#endif
				_mono_decoder.read(x, reader);

				CAT_DEBUG_ENFORCE(index < _pack_palette_size);
			}

			mask <<= 1;
		}

		//// THIS IS THE INNER LOOP ////

		for (int x = 1, xend = (int)_pack_x - 1; x < xend; ++x) {
			// Next mask word
			if (mask_left-- <= 0) {
				mask = *mask_next++;
				mask_left = 31;
			}

			if ((s32)mask < 0) {
				u8 *p = _mono_decoder.currentRow() + x;
				_mono_decoder.zero(x);
				*p = MASK_PAL;
			} else {
#ifdef CAT_DEBUG
				u8 index =
#endif
				_mono_decoder.read_unsafe(x, reader);

				CAT_DEBUG_ENFORCE(index < _pack_palette_size);
			}

			mask <<= 1;
		}

		//// THIS IS THE INNER LOOP ////

		// Unroll x = pack_x - 1
		{
			const int x = _pack_x - 1;

			// Next mask word
			if (mask_left-- <= 0) {
				mask = *mask_next++;
				mask_left = 31;
			}

			if ((s32)mask < 0) {
				u8 *p = _mono_decoder.currentRow() + x;
				_mono_decoder.zero(x);
				*p = MASK_PAL;
			} else {
#ifdef CAT_DEBUG
				u8 index =
#endif
				_mono_decoder.read(x, reader);

				CAT_DEBUG_ENFORCE(index < _pack_palette_size);
			}
		}
	}

#else

	for (int y = 0, yend = _pack_y; y < yend; ++y) {
		_mono_decoder.readRowHeader(y, reader);

		const u32 *mask_next = _mask->nextScanline();
		int mask_left = 0;
		u32 mask;

		for (int x = 0, xend = _pack_x; x < xend; ++x) {
			// Next mask word
			if (mask_left-- <= 0) {
				mask = *mask_next++;
				mask_left = 31;
			}

			if ((s32)mask < 0) {
				u8 *p = _mono_decoder.currentRow() + x;
				_mono_decoder.zero(x);
				*p = MASK_PAL;
			} else {
#ifdef CAT_DEBUG
				u8 index =
#endif
				_mono_decoder.read(x, reader);

				CAT_DEBUG_ENFORCE(index < _pack_palette_size);
			}

			mask <<= 1;
		}
	}

#endif

	return GCIF_RE_OK;
}

int SmallPaletteReader::unpackPixels() {
	CAT_DEBUG_ENFORCE(_pack_palette_size > 1);

	u32 *rgba = reinterpret_cast<u32 *>( _rgba );
	const u8 *image = _image.get();

	if (_palette_size > 4) { // 3-4 bits/pixel
		CAT_DEBUG_ENFORCE(_pack_y == _ysize);
		CAT_DEBUG_ENFORCE(_pack_x == (_xsize+1)/2);

		for (int y = 0; y < _pack_y; ++y) {
			u32 *pixel = rgba;

			for (int x = 0, xlen = _xsize >> 1; x < xlen; ++x, pixel += 2) {
				u8 p = *image++;

				CAT_DEBUG_ENFORCE(p < _pack_palette_size);

				p = _pack_palette[p];

				pixel[0] = _palette[p >> 4];
				pixel[1] = _palette[p & 15];
			}

			if (_xsize & 1) {
				u8 p = *image++;

				CAT_DEBUG_ENFORCE(p < _pack_palette_size);

				p = _pack_palette[p];

				pixel[0] = _palette[p];
			}

			rgba += _xsize;
		}
	} else if (_palette_size > 2) { // 2 bits/pixel
		CAT_DEBUG_ENFORCE(_pack_y == (_ysize+1)/2);
		CAT_DEBUG_ENFORCE(_pack_x == (_xsize+1)/2);

		for (int y = 0, ylen = _ysize >> 1; y < ylen; ++y) {
			u32 *pixel = rgba;

			for (int x = 0, xlen = _xsize >> 1; x < xlen; ++x, pixel += 2) {
				u8 p = *image++;

				CAT_DEBUG_ENFORCE(p < _pack_palette_size);

				p = _pack_palette[p];

				pixel[0] = _palette[p & 3];
				pixel[1] = _palette[(p >> 2) & 3];
				pixel[_xsize] = _palette[(p >> 4) & 3];
				pixel[_xsize+1] = _palette[p >> 6];
			}

			if (_xsize & 1) {
				u8 p = *image++;

				CAT_DEBUG_ENFORCE(p < _pack_palette_size);

				p = _pack_palette[p];

				pixel[0] = _palette[p & 3];
				pixel[_xsize] = _palette[(p >> 4) & 3];
			}

			rgba += _xsize;
		}

		if (_ysize & 1) {
			u32 *pixel = rgba;

			for (int x = 0, xlen = _xsize >> 1; x < xlen; ++x, pixel += 2) {
				u8 p = *image++;

				CAT_DEBUG_ENFORCE(p < _pack_palette_size);

				p = _pack_palette[p];

				pixel[0] = _palette[p & 3];
				pixel[1] = _palette[(p >> 2) & 3];
			}

			if (_xsize & 1) {
				u8 p = *image++;

				CAT_DEBUG_ENFORCE(p < _pack_palette_size);

				p = _pack_palette[p];

				pixel[0] = _palette[p & 3];
			}
		}
	} else { // 1 bit/pixel
		CAT_DEBUG_ENFORCE(_pack_y == (_ysize+1)/2);
		CAT_DEBUG_ENFORCE(_pack_x == (_xsize+3)/4);

		int x, xlen = _xsize >> 2;
		for (int y = 0, ylen = _ysize >> 1; y < ylen; ++y) {
			u32 *pixel = rgba;

			for (x = 0; x < xlen; ++x, pixel += 4) {
				u8 p = *image++;

				CAT_DEBUG_ENFORCE(p < _pack_palette_size);

				p = _pack_palette[p];

				// Unpack byte into 8 pixels
				pixel[0] = _palette[(p >> 7) & 1];
				pixel[1] = _palette[(p >> 6) & 1];
				pixel[2] = _palette[(p >> 5) & 1];
				pixel[3] = _palette[(p >> 4) & 1];
				u32 *next = pixel + _xsize;
				next[0] = _palette[(p >> 3) & 1];
				next[1] = _palette[(p >> 2) & 1];
				next[2] = _palette[(p >> 1) & 1];
				next[3] = _palette[p & 1];
			}

			if (_xsize & 3) {
				u8 p = *image++;

				CAT_DEBUG_ENFORCE(p < _pack_palette_size);

				p = _pack_palette[p];

				u32 *out = pixel;
				for (int jj = 0; jj < 2; ++jj) {
					for (int ii = 0; ii < 4; ++ii) {
						int px = x + ii, py = y + jj;

						if (px < _xsize && py < _ysize) {
							out[ii] = _palette[p >> 7];
						}

						p <<= 1;
					}
					out += _xsize;
				}
			}

			rgba += _xsize << 1;
		}

		if (_ysize & 1) {
			u32 *pixel = rgba;

			for (x = 0, xlen = _xsize >> 2; x < xlen; ++x, pixel += 4) {
				u8 p = *image++;

				CAT_DEBUG_ENFORCE(p < _pack_palette_size);

				p = _pack_palette[p];

				// Unpack byte into 8 pixels
				pixel[0] = _palette[(p >> 7) & 1];
				pixel[1] = _palette[(p >> 6) & 1];
				pixel[2] = _palette[(p >> 5) & 1];
				pixel[3] = _palette[(p >> 4) & 1];
			}

			if (_xsize & 3) {
				u8 p = *image++;

				CAT_DEBUG_ENFORCE(p < _pack_palette_size);

				p = _pack_palette[p];

				for (int ii = 0; ii < 4; ++ii) {
					int px = x + ii;

					if (px < _xsize) {
						pixel[ii] = _palette[p >> 7];
					}

					p <<= 1;
				}
			}
		}
	}

	return GCIF_RE_OK;
}

int SmallPaletteReader::readHead(ImageReader & CAT_RESTRICT reader, u8 * CAT_RESTRICT rgba) {
	// Initialize dimensions
	ImageReader::Header *header = reader.getHeader();
	_xsize = header->xsize;
	_ysize = header->ysize;
	_rgba = rgba;

#ifdef CAT_COLLECT_STATS
	m_clock = Clock::ref();

	double t0 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	// If enabled,
	if (reader.readBit()) {
		// Read small palette table
		readSmallPalette(reader);
	} else {
		// Disabled
		_palette_size = 0;
	}

#ifdef CAT_COLLECT_STATS
	double t1 = m_clock->usec();

	Stats.smallPaletteUsec = t1 - t0;
#endif // CAT_COLLECT_STATS

	return GCIF_RE_OK;
}

int SmallPaletteReader::readTail(ImageReader & CAT_RESTRICT reader, ImageMaskReader & CAT_RESTRICT mask) {
	_mask = &mask;

	CAT_DEBUG_ENFORCE(multipleColors());

#ifdef CAT_COLLECT_STATS
	m_clock = Clock::ref();

	double t0 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	int err;

	if ((err = readPackPalette(reader))) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t1 = m_clock->usec();
#endif // CAT_COLLECT_STATS

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
#endif // CAT_COLLECT_STATS

	if ((err = unpackPixels())) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t4 = m_clock->usec();

	Stats.packPaletteUsec = t1 - t0;
	Stats.tablesUsec = t2 - t1;
	Stats.pixelsUsec = t3 - t2;
	Stats.unpackUsec = t4 - t3;
	Stats.overallUsec = t4 - t0 + Stats.smallPaletteUsec;
#endif // CAT_COLLECT_STATS

	return GCIF_RE_OK;
}

#ifdef CAT_COLLECT_STATS

bool SmallPaletteReader::dumpStats() {
	if (!enabled()) {
		CAT_INANE("stats") << "(Small Palette) Disabled.";
	} else if (!multipleColors()) {
		CAT_INANE("stats") << "(Small Palette) Small Palette : " << Stats.smallPaletteUsec << " usec";
		CAT_INANE("stats") << "(Small Palette) Only one color.";
	} else {
		CAT_INANE("stats") << "(Small Palette) Small Palette : " << Stats.smallPaletteUsec << " usec (" << Stats.smallPaletteUsec * 100.f / Stats.overallUsec << " %total)";
		CAT_INANE("stats") << "(Small Palette)  Pack Palette : " << Stats.packPaletteUsec << " usec (" << Stats.packPaletteUsec * 100.f / Stats.overallUsec << " %total)";
		CAT_INANE("stats") << "(Small Palette)        Tables : " << Stats.tablesUsec << " usec (" << Stats.tablesUsec * 100.f / Stats.overallUsec << " %total)";
		CAT_INANE("stats") << "(Small Palette)        Pixels : " << Stats.pixelsUsec << " usec (" << Stats.pixelsUsec * 100.f / Stats.overallUsec << " %total)";
		CAT_INANE("stats") << "(Small Palette)        Unpack : " << Stats.unpackUsec << " usec (" << Stats.unpackUsec * 100.f / Stats.overallUsec << " %total)";
		CAT_INANE("stats") << "(Small Palette)       Overall : " << Stats.overallUsec << " usec";
	}

	return true;
}

#endif

