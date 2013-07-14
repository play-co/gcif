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

#include "ImageLZReader.hpp"
#include "EndianNeutral.hpp"
#include "HuffmanDecoder.hpp"
#include "GCIFReader.h"
using namespace cat;

#ifdef CAT_COLLECT_STATS
#include "../encoder/Log.hpp"
#include "../encoder/Clock.hpp"

static cat::Clock *m_clock = 0;
#endif // CAT_COLLECT_STATS


//// ImageLZReader

int ImageLZReader::readHuffmanTable(ImageReader & CAT_RESTRICT reader) {
	// Read and validate match count
	const u32 match_count = reader.readBits(16);

	// If we abort early, ensure that triggers are cleared
	_zone_work_head = ZONE_NULL;
	_zone_trigger_x = ZONE_NULL;
	_zone_next_x = ZONE_NULL;
	_zone_trigger_y = ZONE_NULL;
	_zone_next_y = ZONE_NULL;

	// If no matches,
	if (match_count <= 0) {
		return GCIF_RE_OK;
	}

	// If invalid data,
	if (match_count > MAX_ZONE_COUNT) {
		CAT_DEBUG_EXCEPTION();
		return GCIF_RE_LZ_CODES;
	}

	// If minimum count is met,
	_using_decoder = reader.readBit() != 0;
	if (_using_decoder) {
		// If not able to init Huffman decoder
		if (!_decoder.init(reader)) {
			CAT_DEBUG_EXCEPTION();
			return GCIF_RE_LZ_CODES;
		}
	}

	// Allocate zone array
	_zones.resize(match_count);

	return GCIF_RE_OK;
}

int ImageLZReader::readZones(ImageReader & CAT_RESTRICT reader) {
	const int match_count = _zones.size();

	// Skip if nothing to read
	if (match_count == 0) {
		return GCIF_RE_OK;
	}

	if (!_using_decoder) {
		u16 last_dx = 0, last_dy = 0;
		for (int ii = 0; ii < match_count; ++ii) {
			Zone * CAT_RESTRICT z = &_zones[ii];

			u16 sx = reader.read9();
			u16 sy = reader.read9();
			u16 dx = reader.read9();
			u16 dy = reader.read9();

			// Reverse CM
			if (dy == 0) {
				dx += last_dx;
			}
			dy += last_dy;
			sy = dy - sy;

			z->dx = dx;
			z->dy = dy;
			z->sox = (s16)sx - (s16)dx;
			z->soy = (s16)sy - (s16)dy;
			z->w = reader.readBits(8) + ZONEW;
			z->h = reader.readBits(8) + ZONEH;

#ifdef CAT_DUMP_LZ
			CAT_WARN("LZ") << sx << ", " << sy << " -> " << dx << ", " << dy << " [" << z->w << ", " << z->h << "]";
#endif

			// Input security checks
			if (sy > dy || (sy == dy && sx >= dx)) {
				CAT_DEBUG_EXCEPTION();
				return GCIF_RE_LZ_BAD;
			}

			if ((u32)sx + (u32)z->w > (u32)_size_x ||
					(u32)sy + (u32)z->h > (u32)_size_y) {
				CAT_DEBUG_EXCEPTION();
				return GCIF_RE_LZ_BAD;
			}

			if ((u32)z->dx + (u32)z->w > (u32)_size_x ||
					(u32)z->dy + (u32)z->h > (u32)_size_y) {
				CAT_DEBUG_EXCEPTION();
				return GCIF_RE_LZ_BAD;
			}

			last_dx = dx;
			last_dy = dy;
		}
	} else {
		// For each zone to read,
		u16 last_dx = 0, last_dy = 0;
		Zone * CAT_RESTRICT z = _zones.get();
		for (int ii = 0; ii < match_count; ++ii, ++z) {
			u8 b0 = (u8)_decoder.next(reader);
			u8 b1 = (u8)_decoder.next(reader);
			u16 sx = ((u16)b1 << 8) | b0;

			b0 = (u8)_decoder.next(reader);
			b1 = (u8)_decoder.next(reader);
			u16 sy = ((u16)b1 << 8) | b0;

			b0 = (u8)_decoder.next(reader);
			b1 = (u8)_decoder.next(reader);
			u16 dx = ((u16)b1 << 8) | b0;

			b0 = (u8)_decoder.next(reader);
			b1 = (u8)_decoder.next(reader);
			u16 dy = ((u16)b1 << 8) | b0;

			z->w = _decoder.next(reader) + ZONEW;
			z->h = _decoder.next(reader) + ZONEH;

			// Reverse CM
			if (dy == 0) {
				dx += last_dx;
			}
			dy += last_dy;
			sy = dy - sy;

			z->sox = (s16)sx - (s16)dx;
			z->soy = (s16)sy - (s16)dy;
			z->dx = dx;
			z->dy = dy;

#ifdef CAT_DUMP_LZ
			CAT_WARN("LZ") << sx << ", " << sy << " -> " << dx << ", " << dy << " [" << z->w << ", " << z->h << "]";
#endif

			// Input security checks
			if (sy > dy || (sy == dy && sx >= dx)) {
				CAT_DEBUG_EXCEPTION();
				return GCIF_RE_LZ_BAD;
			}

			if ((u32)sx + (u32)z->w > (u32)_size_x ||
					(u32)sy + (u32)z->h > (u32)_size_y) {
				CAT_DEBUG_EXCEPTION();
				return GCIF_RE_LZ_BAD;
			}

			if ((u32)z->dx + (u32)z->w > (u32)_size_x ||
					(u32)z->dy + (u32)z->h > (u32)_size_y) {
				CAT_DEBUG_EXCEPTION();
				return GCIF_RE_LZ_BAD;
			}

			last_dy = dy;
			last_dx = dx;
		}
	}

	// If file truncated,
	if (reader.eof()) {
		CAT_DEBUG_EXCEPTION();
		return GCIF_RE_LZ_CODES;
	}

	// Trigger on first zone
	_zone_next_y = 0;
	_zone_trigger_y = _zones[0].dy;
	return GCIF_RE_OK;
}

int ImageLZReader::triggerX(u8 * CAT_RESTRICT p, u32 * CAT_RESTRICT rgba) {
	// Just triggered a line from zi
	u16 ii = _zone_next_x;
	Zone * CAT_RESTRICT zi = &_zones[ii];
	const int lz_left = zi->w;

	// Copy scanline one at a time in case the pointers are aliased
	int offset = zi->sox + zi->soy * _size_x;
	if (rgba) {
		const volatile u32 *rgba_src = rgba + offset;
		volatile u32 *rgba_dst = rgba;
		for (int jj = 0; jj < lz_left; ++jj) {
			rgba_dst[jj] = rgba_src[jj];
		}
	}

	// Copy scanline one at a time in case the pointers are aliased
	const volatile u8 *p_src = p + offset;
	volatile u8 *p_dst = p;
	for (int jj = 0; jj < lz_left; ++jj) {
		p_dst[jj] = p_src[jj];
	}

	// Iterate ahead to next in work list
	_zone_next_x = zi->next;

	// If this is zi's last scanline,
	if (--zi->h <= 0) {
		// Unlink from list

		// If nothing behind it,
		if (zi->prev == ZONE_NULL) {
			// Link next as head
			_zone_work_head = zi->next;
		} else {
			// Link previous through to next
			_zones[zi->prev].next = zi->next;
		}

		// If there is a zone ahead of it,
		if (zi->next != ZONE_NULL) {
			// Link it back through to previous
			_zones[zi->next].prev = zi->prev;
		}
	}

	// If work list is exhausted,
	if (_zone_next_x == ZONE_NULL) {
		// Loop back to front of remaining list
		_zone_next_x = _zone_work_head;
	}

	// If list just emptied,
	if (_zone_next_x == ZONE_NULL) {
		// Disable triggers
		_zone_trigger_x = ZONE_NULL;
	} else {
		// Set it to the next trigger dx
		_zone_trigger_x = _zones[_zone_next_x].dx;
	}

	return lz_left;
}

void ImageLZReader::triggerY() {
	// Merge y trigger zone and its same-y friends into the work list in order
	const u16 same_dy = _zone_trigger_y;
	u16 ii = _zone_next_y, jj = ZONE_NULL, jnext;
	Zone * CAT_RESTRICT zi = &_zones[ii], *zj = 0;

	// Find insertion point for all same-y zones
	for (jnext = _zone_work_head; jnext != ZONE_NULL; jnext = zj->next) {
		jj = jnext;
		zj = &_zones[jj];

		// If existing item is to the right of where we want to insert,
		while (zj->dx >= zi->dx) {
			// Insert here and update zi to point at next item to insert
			zi->prev = zj->prev;
			zi->next = jj;
			if (zj->prev == ZONE_NULL) {
				_zone_work_head = ii;
			} else {
				_zones[zj->prev].next = ii;
			}
			zj->prev = ii;

			// If inserting before the zone work head,
			if (jj == _zone_work_head) {
				// Set it to this one instead
				_zone_work_head = ii;
			}

			// If out of zones,
			if (++ii >= _zones.size()) {
				// Stop triggering y
				_zone_trigger_y = ZONE_NULL;
				_zone_next_y = ZONE_NULL;
				goto setup_trigger_x;
			}

			// Update zi to point at next item
			zi = &_zones[ii];

			// If the next zone is on another row,
			if (zi->dy != same_dy) {
				goto setup_trigger_y;
			}

			// Loop around and possibly insert before zj again
		}

		// Walk the work list forward trying to find a zj ahead of zi
	}

	do {
		// Link after j
		zi->next = ZONE_NULL;
		zi->prev = jj;

		// If the list is not empty,
		if (jj != ZONE_NULL) {
			// Insert at end of list
			zj->next = ii;
		} else {
			_zone_work_head = ii;
		}

		// j = i
		zj = zi;
		jj = ii;

		// If out of zones,
		if (++ii >= _zones.size()) {
			// Stop triggering y
			_zone_trigger_y = ZONE_NULL;
			_zone_next_y = ZONE_NULL;
			goto setup_trigger_x;
		}

		// i = next i
		zi = &_zones[ii];
	} while (zi->dy == same_dy);

setup_trigger_y:
	// Set up next y trigger at zone index i
	_zone_trigger_y = zi->dy;
	_zone_next_y = ii;

setup_trigger_x:
	// Set up next x trigger at head of work list
	_zone_next_x = _zone_work_head;
	_zone_trigger_x = _zones[_zone_work_head].dx;
}

int ImageLZReader::read(ImageReader &reader, int size_x, int size_y) {
	_size_x = size_x;
	_size_y = size_y;

#ifdef CAT_COLLECT_STATS
	m_clock = Clock::ref();

	double t0 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	int err;

	if ((err = readHuffmanTable(reader))) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t1 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	if ((err = readZones(reader))) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t2 = m_clock->usec();

	Stats.readCodelensUsec = t1 - t0;
	Stats.readZonesUsec = t2 - t1;
	Stats.overallUsec = t2 - t0;
	Stats.zoneCount = _zones.size();
#endif // CAT_COLLECT_STATS

	return GCIF_RE_OK;
}

#ifdef CAT_COLLECT_STATS

bool ImageLZReader::dumpStats() {
	if (Stats.zoneCount <= 0) {
		CAT_INANE("stats") << "(LZ Decode) Disabled.";
	} else {
		CAT_INANE("stats") << "(LZ Decode) Read Huffman Table : " << Stats.readCodelensUsec << " usec (" << Stats.readCodelensUsec * 100.f / Stats.overallUsec << " %total)";
		CAT_INANE("stats") << "(LZ Decode)         Read Zones : " << Stats.readZonesUsec << " usec (" << Stats.readZonesUsec * 100.f / Stats.overallUsec << " %total)";
		CAT_INANE("stats") << "(LZ Decode)            Overall : " << Stats.overallUsec << " usec";
		CAT_INANE("stats") << "(LZ Decode)         Zone Count : " << Stats.zoneCount << " zones read";
	}

	return true;
}

#endif

