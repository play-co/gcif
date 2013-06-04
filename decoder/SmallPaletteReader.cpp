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


//// SmallPaletteReader

int SmallPaletteReader::readSmallPalette(ImageReader & CAT_RESTRICT reader) {
	_palette_size = reader.readBits(4) + 1;

	for (int ii = 0; ii < _palette_size; ++ii) {
		_palette[ii] = getLE(reader.readWord());
	}

	if (_pack_palette_size > 4) { // 3-4 bits/pixel
		_pack_x = (_size_x + 1) >> 1;
		_pack_y = _size_y;
	} else if (_pack_palette_size > 2) { // 2 bits/pixel
		_pack_x = (_size_x + 1) >> 1;
		_pack_y = (_size_y + 1) >> 1;
	} else if (_palette_size > 1) {
		_pack_x = (_size_x + 3) >> 2;
		_pack_y = (_size_y + 1) >> 1;
	} else {
		// Just emit that single color and done!
		u32 color = _palette[0];
		u32 *rgba = reinterpret_cast<u32 *>( _rgba );
		for (int y = 0; y < _size_y; ++y) {
			for (int x = 0, xend = _size_x; x < xend; ++x) {
				*rgba++ = color;
			}
		}
	}

	return GCIF_RE_OK;
}

int SmallPaletteReader::readPackPalette(ImageReader & CAT_RESTRICT reader) {
	// If mask is enabled,
	if (_mask->enabled()) {
		_mask_palette = reader.readBits(8);
	}

	_pack_palette_size = reader.readBits(8) + 1;

	for (int ii = 0; ii < _pack_palette_size; ++ii) {
		_pack_palette[ii] = reader.readBits(8);
	}

	return GCIF_RE_OK;
}

int SmallPaletteReader::readPixels(ImageReader & CAT_RESTRICT reader) {
	const u8 MASK_PAL = _mask_palette;

	u16 trigger_x_lz = _lz->getTriggerX();

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
			// If LZ triggered,
			if (x == trigger_x_lz) {
				u8 * CAT_RESTRICT p = _image.get() + x + y * _size_x;

				lz_skip = _lz->triggerXPal(p, 0);
				trigger_x_lz = _lz->getTriggerX();
			}

			// Next mask word
			if (mask_left-- <= 0) {
				mask = *mask_next++;
				mask_left = 31;
			}

			if (lz_skip > 0) {
				--lz_skip;
				_mono_decoder.maskedSkip(x);
			} else if ((s32)mask < 0) {
				_mono_decoder.maskedWrite(x, MASK_PAL);
			} else {
				// TODO: Unroll to use unsafe version
				u8 index = _mono_decoder.read(x, y, reader);

				CAT_DEBUG_ENFORCE(index < _palette_size);
			}

			mask <<= 1;
		}
	}

	return GCIF_RE_OK;
}

int SmallPaletteReader::unpackPixels() {
	CAT_DEBUG_ENFORCE(_pack_palette_size > 1);

	u32 *rgba = reinterpret_cast<u32 *>( _rgba );
	const u8 *image = _image.get();

	if (_pack_palette_size > 4) { // 3-4 bits/pixel
		CAT_DEBUG_ENFORCE(_pack_y == _size_y);
		CAT_DEBUG_ENFORCE(_pack_x == (_size_x+1)/2);

		for (int y = 0; y < _pack_y; ++y) {
			u32 *pixel = rgba;

			for (int x = 0, xlen = _size_x >> 1; x < xlen; ++x, pixel += 2) {
				u8 p = *image++;

				CAT_DEBUG_ENFORCE(p < _pack_palette_size);

				p = _pack_palette[p];

				pixel[0] = _palette[p >> 4];
				pixel[1] = _palette[p & 15];
			}

			if (_size_x & 1) {
				u8 p = *image++;

				CAT_DEBUG_ENFORCE(p < _pack_palette_size);

				p = _pack_palette[p];

				pixel[0] = _palette[p];
			}

			rgba += _size_x;
		}
	} else if (_pack_palette_size > 2) { // 2 bits/pixel
		CAT_DEBUG_ENFORCE(_pack_y == (_size_y+1)/2);
		CAT_DEBUG_ENFORCE(_pack_x == (_size_x+1)/2);

		for (int y = 0, ylen = _size_y >> 1; y < ylen; ++y) {
			u32 *pixel = rgba;

			for (int x = 0, xlen = _size_x >> 1; x < xlen; ++x, pixel += 2) {
				u8 p = *image++;

				CAT_DEBUG_ENFORCE(p < _pack_palette_size);

				p = _pack_palette[p];

				pixel[0] = _palette[p & 3];
				pixel[1] = _palette[(p >> 2) & 3];
				pixel[_size_x] = _palette[(p >> 4) & 3];
				pixel[_size_x+1] = _palette[p >> 6];
			}

			if (_size_x & 1) {
				u8 p = *image++;

				CAT_DEBUG_ENFORCE(p < _pack_palette_size);

				p = _pack_palette[p];

				pixel[0] = _palette[p & 3];
				pixel[_size_x] = _palette[(p >> 4) & 3];
			}

			rgba += _size_x;
		}

		if (_size_y & 1) {
			u32 *pixel = rgba;

			for (int x = 0, xlen = _size_x >> 1; x < xlen; ++x, pixel += 2) {
				u8 p = *image++;

				CAT_DEBUG_ENFORCE(p < _pack_palette_size);

				p = _pack_palette[p];

				pixel[0] = _palette[p & 3];
				pixel[1] = _palette[(p >> 2) & 3];
			}

			if (_size_x & 1) {
				u8 p = *image++;

				CAT_DEBUG_ENFORCE(p < _pack_palette_size);

				p = _pack_palette[p];

				pixel[0] = _palette[p & 3];
			}
		}
	} else { // 1 bit/pixel
		CAT_DEBUG_ENFORCE(_pack_y == (_size_y+1)/2);
		CAT_DEBUG_ENFORCE(_pack_x == (_size_x+3)/4);

		int x, xlen = _size_x >> 2;
		for (int y = 0, ylen = _size_y >> 1; y < ylen; ++y) {
			u32 *pixel = rgba;

			for (x = 0; x < xlen; ++x, pixel += 4) {
				u8 p = *image++;

				CAT_DEBUG_ENFORCE(p < _pack_palette_size);

				p = _pack_palette[p];

				// Unpack byte into 8 pixels
				pixel[0] = _palette[p >> 7]; p <<= 1;
				pixel[1] = _palette[p >> 7]; p <<= 1;
				pixel[2] = _palette[p >> 7]; p <<= 1;
				pixel[3] = _palette[p >> 7]; p <<= 1;
				u32 *next = pixel + _size_x;
				next[0] = _palette[p >> 7]; p <<= 1;
				next[1] = _palette[p >> 7]; p <<= 1;
				next[2] = _palette[p >> 7]; p <<= 1;
				next[3] = _palette[p >> 7];
			}

			if (_size_x & 3) {
				u8 p = *image++;

				CAT_DEBUG_ENFORCE(p < _pack_palette_size);

				p = _pack_palette[p];

				u32 *out = pixel;
				for (int jj = 0; jj < 2; ++jj) {
					for (int ii = 0; ii < 4; ++ii) {
						int px = x + ii, py = y + jj;

						if (px < _size_x && py < _size_y) {
							out[ii] = _palette[p >> 7];
						}

						p <<= 1;
					}
					out += _size_x;
				}
			}

			rgba += _size_x;
		}

		if (_size_y & 1) {
			u32 *pixel = rgba;

			for (int x = 0, xlen = _size_x >> 2; x < xlen; ++x, pixel += 4) {
				u8 p = *image++;

				CAT_DEBUG_ENFORCE(p < _pack_palette_size);

				p = _pack_palette[p];

				// Unpack byte into 8 pixels
				pixel[0] = _palette[p >> 7]; p <<= 1;
				pixel[1] = _palette[p >> 7]; p <<= 1;
				pixel[2] = _palette[p >> 7]; p <<= 1;
				pixel[3] = _palette[p >> 7];
			}

			if (_size_x & 3) {
				u8 p = *image++;

				CAT_DEBUG_ENFORCE(p < _pack_palette_size);

				p = _pack_palette[p];

				for (int ii = 0; ii < 4; ++ii) {
					int px = x + ii;

					if (px < _size_x) {
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
	_size_x = header->size_x;
	_size_y = header->size_y;
	_rgba = rgba;

	// If enabled,
	if (reader.readBit()) {
		// Read small palette table
		readSmallPalette(reader);
	} else {
		// Disabled
		_palette_size = 0;
	}

	return GCIF_RE_OK;
}

int SmallPaletteReader::readTail(ImageReader & CAT_RESTRICT reader, ImageMaskReader & CAT_RESTRICT mask, ImageLZReader & CAT_RESTRICT lz) {
	_mask = &mask;
	_lz = &lz;

	CAT_DEBUG_ENFORCE(multipleColors());

	int err;

	if ((err = readPackPalette(reader))) {
		return err;
	}

	if ((err = readPixels(reader))) {
		return err;
	}

	if ((err = unpackPixels())) {
		return err;
	}

	return GCIF_RE_OK;
}

#ifdef CAT_COLLECT_STATS

bool SmallPaletteReader::dumpStats() {
	return true;
}

#endif

