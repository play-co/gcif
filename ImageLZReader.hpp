#ifndef IMAGE_LZ_READER_HPP
#define IMAGE_LZ_READER_HPP

#include "Platform.hpp"
#include "ImageReader.hpp"

// See ImageLZWriter.hpp for a description of this algorithm

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
 * After switching to a new raster row that causes a y trigger, an O(N)
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
	static const u32 MAX_ZONE_COUNT = 65535;
	static const int HUFF_TABLE_BITS = 8; // Time/memory tradeoff

protected:
	struct Zone {
		u16 sx, sy;
		u16 dx, dy;
		u16 w, h;
		u16 prev, next;
	} *_zones;
	u32 _zone_count;

	// Lists
	u16 _zone_work_head;	// List of active work items sorted by x
	u16 _zone_trigger_x;	// Next trigger x for _zone_next_x
	u16 _zone_next_x;		// Next work item in list while decoding raster
	u16 _zone_trigger_y;	// Next trigger y for _zone_next_y
	u16 _zone_next_y;		// Start of next row of same-y items to merge

	HuffmanDecoder _huffman;

	void clear();

	int init(const ImageInfo *info);
	int readTables(ImageReader &reader);

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
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

