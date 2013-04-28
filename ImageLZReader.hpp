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

#ifndef IMAGE_LZ_READER_HPP
#define IMAGE_LZ_READER_HPP

#include "ImageReader.hpp"
#include "EntropyDecoder.hpp"

/*
 * Game Closure 2D LZ (GC-2D-LZ) Decompression
 *
 * The encoder ensures that the zone list is sorted in increasing y order, and
 * then increasing x order for each same-y.
 *
 * For each pixel to decode, a trigger x and y value is set to activate a
 * reaction from the LZ reader.  For x triggers, an RLE match copy occurs for
 * a run of pixels.  For y triggers, more work items are added to the work
 * list.
 *
 * After an RLE copy occurs, the zone (work item) height is decremented by 1.
 * And the trigger x is set ahead to the next work item in the list.
 * When height reaches zero it is removed from the work list so it will not
 * affect the next line.
 *
 * After switching to a new scanline that causes a y trigger, an O(N)
 * insertion algorithm is used to merge all the new pre-sorted work items for
 * that row with the active work items that are also sorted.  The number of
 * items to merge is expected to be very low and it only happens once per row.
 *
 * This algorithm means that the 2D LZ decompression can be interleaved easily
 * with the CM decoding (it has set triggers that can be evaluated).
 */

namespace cat {


//// ImageLZReader

class ImageLZReader {
public:
	static const int ZONEW = 3;
	static const int ZONEH = 3;
	static const u32 MAX_ZONE_COUNT = 65535;
	static const int ENCODER_ZRLE_SYMS = 16;

protected:
	static const u16 ZONE_NULL = 0xffff;

	int _width, _height;

	struct Zone {
		s16 sox, soy;		// Source read offset in x and y from dx,dy
		u16 dx, dy;			// Destination for setting up triggers
		u16 w, h;			// Width and height of zone, height decrements
		u16 prev, next;		// Doubly-linked work list
	} *_zones;			// Array of zones
	u32 _zones_size;	// Size of array
	int _zones_alloc;

	// Lists
	u16 _zone_work_head;	// List of active work items sorted by x
	u16 _zone_trigger_x;	// Next trigger x for _zone_next_x
	u16 _zone_next_x;		// Next work item in list while decoding scanline 
	u16 _zone_trigger_y;	// Next trigger y for _zone_next_y
	u16 _zone_next_y;		// Start of next row of same-y items to merge

	EntropyDecoder<256, ENCODER_ZRLE_SYMS> _decoder;
	bool _using_decoder;

	void clear();

	int init(const ImageHeader *header);
	int readHuffmanTable(ImageReader &reader);
	int readZones(ImageReader &reader);

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		double initUsec;
		double readCodelensUsec;
		double readZonesUsec;
		double overallUsec;

		int zoneCount;
	} Stats;
#endif

public:
	CAT_INLINE ImageLZReader() {
		_zones = 0;
	}
	virtual CAT_INLINE ~ImageLZReader() {
		clear();
	}

	int read(ImageReader &reader);

	CAT_INLINE u16 getTriggerX() {
		return _zone_trigger_x;
	}
	CAT_INLINE u16 getTriggerY() {
		return _zone_trigger_y;
	}

	// Call when x reaches next trigger x
	// Skip the number of pixels returned
	// Returns pixel count which is always at least 1
	// p: Pointer to first byte of current RGBA pixel
	int triggerX(u8 *p, int &lz_lines_left);

	// Call when y reaches next trigger y before looping over x values
	void triggerY();

#ifdef CAT_COLLECT_STATS
	bool dumpStats();
#else
	CAT_INLINE bool dumpStats() {
		return false;
	}
#endif
};


} // namespace cat

#endif // IMAGE_LZ_READER_HPP

