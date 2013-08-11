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

#include "ImageRGBAReader.hpp"
#include "Enforcer.hpp"
#include "EndianNeutral.hpp"
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


//// ImageRGBAReader

int ImageRGBAReader::readFilterTables(ImageReader & CAT_RESTRICT reader) {
	int err;

	// Read tile bits
	_tile_bits_x = reader.readBits(3) + 1;
	_tile_bits_y = _tile_bits_x;
	_tile_xsize = 1 << _tile_bits_x;
	_tile_ysize = 1 << _tile_bits_y;
	_tile_mask_x = _tile_xsize - 1;
	_tile_mask_y = _tile_ysize - 1;
	_tiles_x = (_xsize + _tile_mask_x) >> _tile_bits_x;
	_tiles_y = (_ysize + _tile_mask_y) >> _tile_bits_y;
	_filters.resize(_tiles_x);

	const int tile_count = _tiles_x * _tiles_y;
	_sf_tiles.resize(tile_count);
	_cf_tiles.resize(tile_count);

	DESYNC_TABLE();

	// Read filter choices
	_sf_count = reader.readBits(5) + 1;
	for (int ii = 0; ii < _sf_count; ++ii) {
		u8 sf = reader.readBits(7);

#ifdef CAT_DUMP_FILTERS
		CAT_WARN("RGBA") << "Filter " << ii << " = " << (int)sf;
#endif

		if (sf >= SF_COUNT) {
			CAT_DEBUG_EXCEPTION();
			return GCIF_RE_BAD_RGBA;
		}

		_sf[ii] = RGBA_FILTERS[sf];
	}

	DESYNC_TABLE();

	// Read SF decoder
	{
		MonoReader::Parameters params;
		params.data = _sf_tiles.get();
		params.xsize = _tiles_x;
		params.ysize = _tiles_y;
		params.num_syms = _sf_count;
		params.min_bits = 2;
		params.max_bits = 5;

#ifdef CAT_DUMP_FILTERS
		CAT_WARN("RGBA") << "Reading SF";
#endif
		if ((err = _sf_decoder.readTables(params, reader))) {
			return err;
		}
	}

	DESYNC_TABLE();

	// Read CF decoder
	{
		MonoReader::Parameters params;
		params.data = _cf_tiles.get();
		params.xsize = _tiles_x;
		params.ysize = _tiles_y;
		params.num_syms = CF_COUNT;
		params.min_bits = 2;
		params.max_bits = 5;

#ifdef CAT_DUMP_FILTERS
		CAT_WARN("RGBA") << "Reading CF";
#endif
		if ((err = _cf_decoder.readTables(params, reader))) {
			return err;
		}
	}

	DESYNC_TABLE();

	return GCIF_RE_OK;
}

int ImageRGBAReader::readRGBATables(ImageReader & CAT_RESTRICT reader) {
	int err;

	// Read alpha decoder
	{
		const int pixel_count = _xsize * _ysize;
		_a_tiles.resize(pixel_count);

		MonoReader::Parameters params;
		params.data = _a_tiles.get();
		params.xsize = _xsize;
		params.ysize = _ysize;
		params.num_syms = 256;
		params.min_bits = 2;
		params.max_bits = 5;

#ifdef CAT_DUMP_FILTERS
		CAT_WARN("RGBA") << "Reading alpha channel";
#endif
		if ((err = _a_decoder.readTables(params, reader))) {
			return err;
		}
	}

	DESYNC_TABLE();

	// Read chaos levels
	const int chaos_levels = reader.readBits(4) + 1;

	_chaos.init(chaos_levels, _xsize);

	// For each chaos level,
	for (int jj = 0; jj < chaos_levels; ++jj) {
		// Read the decoder tables
		if CAT_UNLIKELY(!_y_decoder[jj].init(NUM_Y_SYMS, NUM_ZRLE_SYMS, HUFF_LUT_BITS, reader)) {
			CAT_DEBUG_EXCEPTION();
			return GCIF_RE_BAD_RGBA;
		}
		DESYNC_TABLE();

		if CAT_UNLIKELY(!_u_decoder[jj].init(NUM_U_SYMS, NUM_ZRLE_SYMS, HUFF_LUT_BITS, reader)) {
			CAT_DEBUG_EXCEPTION();
			return GCIF_RE_BAD_RGBA;
		}
		DESYNC_TABLE();

		if CAT_UNLIKELY(!_v_decoder[jj].init(NUM_V_SYMS, NUM_ZRLE_SYMS, HUFF_LUT_BITS, reader)) {
			CAT_DEBUG_EXCEPTION();
			return GCIF_RE_BAD_RGBA;
		}
		DESYNC_TABLE();
	}

	if CAT_UNLIKELY(!_lz.init(_xsize, _ysize, reader)) {
		CAT_DEBUG_EXCEPTION();
		return GCIF_RE_LZ_CODES;
	}

	DESYNC_TABLE();

	return GCIF_RE_OK;
}

CAT_INLINE void ImageRGBAReader::readSafe(u16 &x, const u16 y, u8 * CAT_RESTRICT &p, ImageReader & CAT_RESTRICT reader, u32 &mask, const u32 * CAT_RESTRICT &mask_next, int &mask_left, const u32 MASK_COLOR, const u8 MASK_ALPHA) {
	DESYNC(x, y);

#ifndef CAT_DISABLE_MASK
	// Next mask word
	if (mask_left <= 0) {
		mask = *mask_next++;
		mask_left = 32;
	}

	if ((s32)mask < 0) {
		*reinterpret_cast<u32 *>( p ) = MASK_COLOR;
		u8 * CAT_RESTRICT Ap = _a_decoder.currentRow() + x;
		*Ap = MASK_ALPHA;
		_chaos.zero(x);
		_a_decoder.zero(x);
	} else {
#endif
		// Calculate YUV chaos
		u8 cy, cu, cv;
		_chaos.get(x, cy, cu, cv);

		u16 pixel_code = _y_decoder[cy].next(reader); 

		// If it is an LZ escape code,
		if (pixel_code >= 256) {
			int len = readLZMatch(pixel_code, reader, x, p);
			CAT_DEBUG_ENFORCE(len >= 2);
			DESYNC(x, y);

			// Move pointers ahead
			p += len << 2;
			x += len;

#ifndef CAT_DISABLE_MASK
			// Move mask ahead
			if (len >= mask_left) {
				len -= mask_left;

				// Remove mask multiples
				mask_next += len >> 5;
				len &= 31;

				mask = *mask_next++;
				mask_left = 32;
			}
			mask <<= len;
			mask_left -= len;
#endif

			return;
		} else {
			// Read YUV
			u8 YUV[3];
			YUV[0] = (u8)pixel_code;
			YUV[1] = (u8)_u_decoder[cu].next(reader);
			YUV[2] = (u8)_v_decoder[cv].next(reader);

			// Read alpha pixel
			p[3] = (u8)~_a_decoder.read(x, reader);

			DESYNC(x, y);

			FilterSelection *filter = readFilter(x, y, reader);

			// Reverse color filter
			filter->cf(YUV, p);

			// Reverse spatial filter
			u8 FPT[3];
			const u8 * CAT_RESTRICT pred = filter->sf.safe(p, FPT, x, y, _xsize);
			p[0] += pred[0];
			p[1] += pred[1];
			p[2] += pred[2];

			_chaos.store(x, YUV);
		}
#ifndef CAT_DISABLE_MASK
	}

	--mask_left;
	mask <<= 1;
#endif
	p += 4;
	++x;
}

CAT_INLINE void ImageRGBAReader::readUnsafe(u16 &x, const u16 y, u8 * CAT_RESTRICT &p, ImageReader & CAT_RESTRICT reader, u32 &mask, const u32 * CAT_RESTRICT &mask_next, int &mask_left, const u32 MASK_COLOR, const u8 MASK_ALPHA) {
	DESYNC(x, y);

#ifndef CAT_DISABLE_MASK
	// Next mask word
	if (mask_left <= 0) {
		mask = *mask_next++;
		mask_left = 32;
	}

	if ((s32)mask < 0) {
		*reinterpret_cast<u32 *>( p ) = MASK_COLOR;
		u8 * CAT_RESTRICT Ap = _a_decoder.currentRow() + x;
		*Ap = MASK_ALPHA;
		_chaos.zero(x);
		_a_decoder.zero(x);
	} else {
#endif
		// Calculate YUV chaos
		u8 cy, cu, cv;
		_chaos.get(x, cy, cu, cv);

		u16 pixel_code = _y_decoder[cy].next(reader); 

		// If it is an LZ escape code,
		if (pixel_code >= 256) {
			int len = readLZMatch(pixel_code, reader, x, p);
			CAT_DEBUG_ENFORCE(len >= 2);
			DESYNC(x, y);

			// Move pointers ahead
			p += len << 2;
			x += len;

			// Move mask ahead
			if (len >= mask_left) {
				len -= mask_left;

				// Remove mask multiples
				mask_next += len >> 5;
				len &= 31;

				mask = *mask_next++;
				mask_left = 32;
			}
			mask <<= len;
			mask_left -= len;

			return;
		} else {
			// Read YUV
			u8 YUV[3];
			YUV[0] = (u8)pixel_code;
			YUV[1] = (u8)_u_decoder[cu].next(reader);
			YUV[2] = (u8)_v_decoder[cv].next(reader);

			// Read alpha pixel
			p[3] = (u8)~_a_decoder.read(x, reader);

			DESYNC(x, y);

			FilterSelection *filter = readFilter(x, y, reader);

			// Reverse color filter
			filter->cf(YUV, p);

			// Reverse spatial filter
			u8 FPT[3];
			const u8 * CAT_RESTRICT pred = filter->sf.unsafe(p, FPT, x, y, _xsize);
			p[0] += pred[0];
			p[1] += pred[1];
			p[2] += pred[2];

			_chaos.store(x, YUV);
		}
#ifndef CAT_DISABLE_MASK
	}

	--mask_left;
	mask <<= 1;
#endif
	p += 4;
	++x;
}

int ImageRGBAReader::readPixels(ImageReader & CAT_RESTRICT reader) {
	const int xsize = _xsize;
	const u32 MASK_COLOR = _mask->getColor();
	const u8 MASK_ALPHA = (u8)~(getLE(MASK_COLOR) >> 24);

	_chaos.start();

	_cf_decoder.setupUnordered();
	_sf_decoder.setupUnordered();

	// Start from upper-left of image
	u8 * CAT_RESTRICT p = _rgba;

#ifdef CAT_UNROLL_READER

	// Unroll y = 0 scanline
	{
		const u16 y = 0;

		// Clear filters data
		_filters.fill_00();

		// Read row headers
		_sf_decoder.readRowHeader(y, reader);
		_cf_decoder.readRowHeader(y, reader);

		_a_decoder.readRowHeader(y, reader);

		// Read mask scanline
		const u32 * CAT_RESTRICT mask_next = _mask->nextScanline();
		int mask_left = 32;
		u32 mask = *mask_next++;

		// For each pixel,
		for (u16 x = 0; x < xsize;) {
			readSafe(x, y, p, reader, mask, mask_next, mask_left, MASK_COLOR, MASK_ALPHA);
		}
	}


	// For each scanline,
	for (u16 y = 1; y < _ysize; ++y) {
		// If it is time to clear the filters data,
		if ((y & _tile_mask_y) == 0) {
			// Zero filter holes
			for (u16 tx = 0; tx < _tiles_x; ++tx) {
				if (!_filters[tx].ready()) {
					_sf_decoder.zero(tx);
					_cf_decoder.zero(tx);
				}
			}

			// Clear filters data
			_filters.fill_00();

			const u16 ty = y >> _tile_bits_y;

			// Read row headers
			_sf_decoder.readRowHeader(ty, reader);
			_cf_decoder.readRowHeader(ty, reader);
		}

		_a_decoder.readRowHeader(y, reader);

		// Read mask scanline
		const u32 * CAT_RESTRICT mask_next = _mask->nextScanline();
		int mask_left = 32;
		u32 mask = *mask_next++;

		// Unroll x = 0 pixel
		u16 x = 0;
		readSafe(x, y, p, reader, mask, mask_next, mask_left, MASK_COLOR, MASK_ALPHA);

		// For each pixel,
		for (u16 xend = xsize - 1; x < xend;) {
			readUnsafe(x, y, p, reader, mask, mask_next, mask_left, MASK_COLOR, MASK_ALPHA);
		}

		// For right image edge,
		if (x < xsize) {
			readSafe(x, y, p, reader, mask, mask_next, mask_left, MASK_COLOR, MASK_ALPHA);
		}
	}

#else

	// For each row,
	for (u16 y = 0; y < _ysize; ++y) {
		// If it is time to clear the filters data,
		if ((y & _tile_mask_y) == 0) {
			if (y > 0) {
				// Zero filter holes
				for (u16 tx = 0; tx < _tiles_x; ++tx) {
					if (!_filters[tx].ready()) {
						_sf_decoder.zero(tx);
						_cf_decoder.zero(tx);
					}
				}
			}

			// Clear filters data
			_filters.fill_00();

			const u16 ty = y >> _tile_bits_y;

			// Read row headers
			_sf_decoder.readRowHeader(ty, reader);
			_cf_decoder.readRowHeader(ty, reader);
		}

		_a_decoder.readRowHeader(y, reader);

		// Read mask scanline
		const u32 * CAT_RESTRICT mask_next = _mask->nextScanline();
		int mask_left = 32;
		u32 mask = *mask_next++;

		// For each pixel,
		for (u16 x = 0; x < xsize;) {
			readSafe(x, y, p, reader, mask, mask_next, mask_left, MASK_COLOR, MASK_ALPHA);
		}
	}

#endif

	return GCIF_RE_OK;
}

int ImageRGBAReader::readLZMatch(u16 pixel_code, ImageReader & CAT_RESTRICT reader, int x, u8 * CAT_RESTRICT p) {
	// Decode LZ bitstream
	u32 dist, len;
	len = _lz.read(pixel_code - 256, reader, dist);

	CAT_DEBUG_ENFORCE(len >= 2 && len <= 256);
	CAT_DEBUG_ENFORCE(dist != 0);

	// Calculate source address of copy
	const u32 * CAT_RESTRICT src = reinterpret_cast<const u32 * CAT_RESTRICT>( p );

	// If LZ copy source is invalid,
	if CAT_UNLIKELY(src < reinterpret_cast<const u32 * CAT_RESTRICT>( _rgba ) + dist) {
		// Unfortunately need to add dist twice to avoid pointer wrap around near 0
		CAT_DEBUG_EXCEPTION();
		return GCIF_RE_LZ_BAD;
	}

	src -= dist;

	u32 * CAT_RESTRICT dst = reinterpret_cast<u32 * CAT_RESTRICT>( p );
	CAT_DEBUG_ENFORCE(src < dst);

	// If LZ destination is invalid,
	if CAT_UNLIKELY(x + len > _xsize) {
		CAT_DEBUG_EXCEPTION();
		return GCIF_RE_LZ_BAD;
	}

	// Calculate destination address for alpha
	u8 * CAT_RESTRICT Ap_dst = _a_decoder.currentRow() + x;
	const u8 * CAT_RESTRICT Ap_src = Ap_dst - dist;

	// Copy blocks at a time
	int copy = len;
	while (copy >= 4) {
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst[3] = src[3];
		dst += 4;
		src += 4;
		Ap_dst[0] = Ap_src[0];
		Ap_dst[1] = Ap_src[1];
		Ap_dst[2] = Ap_src[2];
		Ap_dst[3] = Ap_src[3];
		Ap_dst += 4;
		Ap_src += 4;
		copy -= 4;
	}

	// Copy words at a time
	while (copy > 0) {
		dst[0] = src[0];
		++dst;
		++src;
		Ap_dst[0] = Ap_src[0];
		++Ap_dst;
		++Ap_src;
		--copy;
	}

	// Execute remaining chaos zeroing
	_chaos.zeroRegion(x, len);
	_a_decoder.zeroRegion(x, len);

	// Return match length
	return len;
}

int ImageRGBAReader::read(ImageReader & CAT_RESTRICT reader, ImageMaskReader & CAT_RESTRICT maskReader, GCIFImage * CAT_RESTRICT image) {
#ifdef CAT_COLLECT_STATS
	m_clock = Clock::ref();

	double t0 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	int err;

	_mask = &maskReader;

	_rgba = image->rgba;
	_xsize = image->xsize;
	_ysize = image->ysize;

	// Read filter selection tables
	if ((err = readFilterTables(reader))) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t1 = m_clock->usec();
#endif	

	// Read Huffman tables for each RGB channel and chaos level
	if ((err = readRGBATables(reader))) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t2 = m_clock->usec();
#endif	

	// Read RGB data and decompress it
	if ((err = readPixels(reader))) {
		return err;
	}

	// Pass image data reference back to caller
	_rgba = 0;


#ifdef CAT_COLLECT_STATS
	double t3 = m_clock->usec();

	Stats.readFilterTablesUsec = t1 - t0;
	Stats.readChaosTablesUsec = t2 - t1;
	Stats.readPixelsUsec = t3 - t2;
	Stats.overallUsec = t3 - t0;
#endif	
	return GCIF_RE_OK;
}

#ifdef CAT_COLLECT_STATS

bool ImageRGBAReader::dumpStats() {
	CAT_INANE("stats") << "(RGBA Decode) Read Filter Tables : " << Stats.readFilterTablesUsec << " usec (" << Stats.readFilterTablesUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(RGBA Decode)   Read RGBA Tables : " << Stats.readChaosTablesUsec << " usec (" << Stats.readChaosTablesUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(RGBA Decode)      Decode Pixels : " << Stats.readPixelsUsec << " usec (" << Stats.readPixelsUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(RGBA Decode)            Overall : " << Stats.overallUsec << " usec";

	CAT_INANE("stats") << "(RGBA Decode)         Throughput : " << (_xsize * _ysize * 4) / Stats.overallUsec << " MBPS (output bytes/time)";
	CAT_INANE("stats") << "(RGBA Decode)   Image Dimensions : " << _xsize << " x " << _ysize << " pixels";

	return true;
}

#endif

