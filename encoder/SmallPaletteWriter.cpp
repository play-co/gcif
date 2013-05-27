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

#include "SmallPaletteWriter.hpp"
#include "../decoder/EndianNeutral.hpp"
#include "Log.hpp"
#include "../decoder/Enforcer.hpp"
using namespace cat;
using namespace std;


//// SmallPaletteWriter

bool SmallPaletteWriter::generatePalette() {
	u32 hist[SMALL_PALETTE_MAX] = {0};

	const u32 *color = reinterpret_cast<const u32 *>( _rgba );
	int palette_size = 0;

	for (int y = 0; y < _size_y; ++y) {
		for (int x = 0, xend = _size_x; x < xend; ++x) {
			u32 c = *color++;

			// Determine palette index
			int index;
			if (_map.find(c) == _map.end()) {
				// If ran out of palette slots,
				if (palette_size >= SMALL_PALETTE_MAX) {
					return false;
				}

				index = palette_size;
				_map[c] = index;
				_palette.push_back(c);

				++palette_size;
			} else {
				index = _map[c];
			}

			// Record how often each palette index is used
			hist[index]++;
		}
	}

	// If palette size is degenerate,
	if (palette_size <= 0) {
		CAT_DEBUG_EXCEPTION();
		return false;
	}

	// Store the palette size
	_palette_size = palette_size;

#ifdef CAT_COLLECT_STATS
	Stats.palette_size = _palette_size;
#endif

	return true;
}

void SmallPaletteWriter::generatePacked() {
	if (_palette_size > 4) { // 3-4 bits/pixel
		/*
		 * Combine pairs of pixels on the same scanline together.
		 * Final odd pixel in each row is encoded in the low bits.
		 */
		_pack_x = (_size_x + 1) >> 1;
		_pack_y = _size_y;
		const int image_size = _pack_x * _pack_y;
		_image.resize(image_size);

		const u32 *color = reinterpret_cast<const u32 *>( _rgba );
		u8 *image = _image.get();

		// For each scanline,
		for (int y = 0; y < _size_y; ++y) {
			u8 b = 0;
			int packed = 0;

			// For each pixel,
			for (int x = 0, xend = _size_x; x < xend; ++x) {
				// Lookup palette index
				u32 c = *color++;
				u8 p = _map[c];

				// Pack pixel
				b <<= 4;
				b |= p;
				++packed;

				// If it is time to write,
				if (packed >= 2) {
					*image++ = b;
					b = 0;
					packed = 0;
				}
			}

			// If there is a partial packing,
			if (packed > 0) {
				*image++ = b;
			}
		}
	} else if (_palette_size > 2) { // 2 bits/pixel
		/*
		 * Combine blocks of 4 pixels together:
		 *
		 * 0 0 1 1 2 2 3
		 * 0 0 1 1 2 2 3 <- example 7x3 image
		 * 4 4 5 5 6 6 7
		 *
		 * Each 2x2 block is packed like so:
		 * 0 1  -->  HI:[ 3 3 2 2 1 1 0 0 ]:LO
		 * 2 3
		 */
		_pack_x = (_size_x + 1) >> 1;
		_pack_y = (_size_y + 1) >> 1;
		const int image_size = _pack_x * _pack_y;
		_image.resize(image_size);

		const u32 *color = reinterpret_cast<const u32 *>( _rgba );
		u8 *image = _image.get();

		for (int y = 0; y < _size_y; y += 2) {
			for (int x = 0, xend = _size_x; x < xend; x += 2) {
				// Lookup palette index
				u32 c0 = color[0];
				u8 p0 = _map[c0], p1 = 0, p2 = 0, p3 = 0;

				// Read off palette indices and increment color pointer
				if (y < _size_y-1) {
					u32 c2 = color[_size_x];
					p2 = _map[c2];
				}
				if (x < _size_x-1) {
					if (y < _size_y-1) {
						u32 c3 = color[_size_x + 1];
						p3 = _map[c3];
					}

					u32 c1 = color[1];
					p1 = _map[c1];
					++color;
				}
				++color;

				// Store packed pixels
				*image++ = (p3 << 6) | (p2 << 4) | (p1 << 2) | p0;
			}
		}
	} else if (_palette_size > 1) { // 1 bit/pixel
		/*
		 * Combine blocks of 8 pixels together:
		 *
		 * 0 0 0 0 1 1 1 1 2 2 2
		 * 0 0 0 0 1 1 1 1 2 2 2 <- example 11x3 image
		 * 3 3 3 3 4 4 4 4 5 5 5
		 *
		 * Each 4x2 block is packed like so:
		 * 0 1 2 3  -->  HI:[ 0 1 2 3 4 5 6 7 ]:LO
		 * 4 5 6 7
		 */
		_pack_x = (_size_x + 3) >> 2;
		_pack_y = (_size_y + 1) >> 1;
		const int image_size = _pack_x * _pack_y;
		_image.resize(image_size);

		const u32 *color = reinterpret_cast<const u32 *>( _rgba );
		u8 *image = _image.get();

		for (int y = 0; y < _size_y; y += 2) {
			for (int x = 0, xend = _size_x; x < xend; x += 4) {
				u8 b = 0;

				for (int jj = 0; jj < 2; ++jj) {
					for (int ii = 0; ii < 4; ++ii) {
						int px = x + ii, py = y + jj;

						b <<= 1;

						if (px < _size_x && py < _size_y) {
							u32 c = color[px + py * _size_x];

							u8 p = (u8)_map[c];

							CAT_DEBUG_ENFORCE(p < 2);

							b |= p;
						}
					}
				}

				*image++ = b;
			}
		}
	}
	// Else: 0 bits per pixel, just need to transmit palette
}

int SmallPaletteWriter::init(const u8 *rgba, int size_x, int size_y, const GCIFKnobs *knobs) {
	_knobs = knobs;
	_rgba = rgba;
	_size_x = size_x;
	_size_y = size_y;

	// Off by default
	_palette_size = 0;

	// If palette was generated,
	if (generatePalette()) {
		generatePacked();
	}

	return GCIF_WE_OK;
}

void SmallPaletteWriter::writeHead(ImageWriter &writer) {
	if (enabled()) {
		writer.writeBit(1);
		writeSmallPalette(writer);
	} else {
		writer.writeBit(0);
	}
}

void SmallPaletteWriter::writeSmallPalette(ImageWriter &writer) {
	int bits = 0;

	CAT_DEBUG_ENFORCE(SMALL_PALETTE_MAX <= 16);

	writer.writeBits(_palette_size - 1, 4);
	bits += 4;

	for (int ii = 0; ii < _palette_size; ++ii) {
		u32 color = getLE(_palette[ii]);

		writer.writeWord(color);
		bits += 32;
	}

#ifdef CAT_COLLECT_STATS
	Stats.small_palette_bits = bits;
#endif
}

bool SmallPaletteWriter::IsMasked(u16 x, u16 y) {
	return _mask->masked(x, y) || _lz->visited(x, y);
}

void SmallPaletteWriter::convertPacked() {
	// Find used symbols
	u8 seen[MAX_SYMS] = { 0 };
	u8 *image = _image.get();

	for (int y = 0; y < _pack_y; ++y) {
		for (int x = 0, xend = _pack_x; x < xend; ++x) {
			seen[*image++] = 1;
		}
	}

	// Generate mapping
	u8 reverse[MAX_SYMS];

	int num_syms = 0;
	for (int ii = 0; ii < MAX_SYMS; ++ii) {
		if (seen[ii] != 0) {
			reverse[ii] = num_syms;
			_pack_palette[num_syms++] = (u8)ii;
		}
	}
	_pack_palette_size = num_syms;

	// Convert image
	image = _image.get();
	for (int y = 0; y < _pack_y; ++y) {
		for (int x = 0, xend = _pack_x; x < xend; ++x) {
			const u8 p = *image;
			*image++ = reverse[p];
		}
	}
}

void SmallPaletteWriter::optimizeImage() {
	CAT_INANE("Palette") << "Optimizing palette...";

	_optimizer.process(_image.get(), _pack_x, _pack_y, _pack_palette_size,
		PaletteOptimizer::MaskDelegate::FromMember<SmallPaletteWriter, &SmallPaletteWriter::IsMasked>(this));

	// Replace palette image
	const u8 *src = _optimizer.getOptimizedImage();
	memcpy(_image.get(), src, _pack_x * _pack_y);

	// Fix pack palette array
	u8 better_palette[MAX_SYMS];

	for (int ii = 0; ii < _pack_palette_size; ++ii) {
		better_palette[_optimizer.forward(ii)] = _pack_palette[ii];
	}
	memcpy(_pack_palette, better_palette, _pack_palette_size * sizeof(_pack_palette[0]));
}

void SmallPaletteWriter::generateMonoWriter() {
	CAT_INANE("Palette") << "Compressing index matrix...";

	MonoWriter::Parameters params;

	params.knobs = _knobs;
	params.data = _image.get();
	params.num_syms = _pack_palette_size;
	params.size_x = _pack_x;
	params.size_y = _pack_y;
	params.max_filters = 32;
	params.min_bits = 2;
	params.max_bits = 5;
	params.sympal_thresh = 0.1;
	params.filter_cover_thresh = 0.6;
	params.filter_inc_thresh = 0.05;
	params.mask.SetMember<SmallPaletteWriter, &SmallPaletteWriter::IsMasked>(this);
	params.AWARDS[0] = 5;
	params.AWARDS[1] = 3;
	params.AWARDS[2] = 1;
	params.AWARDS[3] = 1;
	params.award_count = 4;
	params.write_order = 0;

	_mono_writer.init(params);
}

int SmallPaletteWriter::compress(ImageMaskWriter &mask, ImageLZWriter &lz) {
	_mask = &mask;
	_lz = &lz;

	CAT_DEBUG_ENFORCE(enabled());

	convertPacked();
	optimizeImage();
	generateMonoWriter();

	return GCIF_WE_OK;
}

void SmallPaletteWriter::writePackPalette(ImageWriter &writer) {
	int bits = 0;

	// If using mask,
	if (_mask->enabled()) {
		u32 maskColor = _mask->getColor();

		CAT_DEBUG_ENFORCE(maskColor < 256);

		writer.writeBits(maskColor, 8);
	}

	writer.writeBits(_pack_palette_size - 1, 8);
	bits += 8;

	for (int ii = 0; ii < _pack_palette_size; ++ii) {
		u8 packed = _pack_palette[ii];

		writer.writeBits(packed, 8);
		bits += 8;
	}

#ifdef CAT_COLLECT_STATS
	Stats.pack_palette_bits = bits;
#endif
}

void SmallPaletteWriter::writePixels(ImageWriter &writer) {
	int bits = 0, pixels = 0;

	// Write tables for pixel data
	bits += _mono_writer.writeTables(writer);

	const u8 *image = _image.get();

	for (int y = 0; y < _pack_y; ++y) {
		bits += _mono_writer.writeRowHeader(y, writer);

		for (int x = 0, xend = _pack_x; x < xend; ++x, ++image) {
			if (IsMasked(x, y)) {
				_mono_writer.zero(x);
			} else {
				bits += _mono_writer.write(x, y, writer);
				++pixels;
			}
		}
	}

#ifdef CAT_COLLECT_STATS
	Stats.pixel_bits = bits;
	Stats.packed_pixels = pixels;
	Stats.total_bits = Stats.small_palette_bits + Stats.pack_palette_bits + Stats.pixel_bits;
	Stats.compression_ratio = _size_x * _size_y * 32 / (double)Stats.total_bits;
#endif
}

void SmallPaletteWriter::writeTail(ImageWriter &writer) {
	CAT_DEBUG_ENFORCE(enabled());

	writePackPalette(writer);
	writePixels(writer);
}

#ifdef CAT_COLLECT_STATS

bool SmallPaletteWriter::dumpStats() {
	if (!enabled()) {
		CAT_INANE("stats") << "(Small Palette) Disabled.";
	} else {
		CAT_INANE("stats") << "(Small Palette)              Size : " << Stats.palette_size << " colors";
		CAT_INANE("stats") << "(Small Palette)     Small Palette : " << Stats.small_palette_bits / 8 << " bytes (" << Stats.small_palette_bits * 100.f / Stats.total_bits << "% total)";
		CAT_INANE("stats") << "(Small Palette)    Packed Palette : " << Stats.pack_palette_bits / 8 << " bytes (" << Stats.pack_palette_bits * 100.f / Stats.total_bits << "% total)";
		CAT_INANE("stats") << "(Small Palette)        Pixel Data : " << Stats.pixel_bits / 8 << " bytes (" << Stats.pixel_bits * 100.f / Stats.total_bits << "% total)";
		CAT_INANE("stats") << "(Small Palette)     Packed Pixels : " << Stats.packed_pixels << " packed pixels written";
		CAT_INANE("stats") << "(Small Palette) Compression Ratio : " << Stats.compression_ratio << ":1 compression ratio";
	}

	return true;
}

#endif

