#ifndef IMAGE_LP_READER_HPP
#define IMAGE_LP_READER_HPP

#include "Platform.hpp"
#include "ImageReader.hpp"
#include "HuffmanDecoder.hpp"

/*
 * Game Closure Local Palette (GC-2D-LP) Decompression
 *
 * Similar to the GC-2D-LZ decompression algorithm, this one triggers on X and
 * Y positions during decoding.
 *
 * When it triggers it takes over decoding the image data stream, doing color
 * palette lookups in place of CM.
 */

namespace cat {


//// ImageLPReader

class ImageLPReader {
public:
	static const int ZONEW = 3;
	static const int ZONEH = 3;
	static const int MAX_COLORS = 16;
	static const u32 MAX_ZONE_COUNT = 65535;
	static const int MAX_HUFF_SYMS = MAX_COLORS;
	static const int HUFF_COLOR_THRESH = 20;		// Minimum color count to compress color information
	static const int HUFF_ZONE_THRESH = 12;			// Minimum zone count to compress zone information

protected:
	static const u16 ZONE_NULL = 0xffff;

	int _width, _height;

	struct Zone {
		u32 colors[MAX_COLORS];
		int used;

		HuffmanDecoder decoder;

		u16 x, y;
		u16 w, h;

		u16 prev, next;		// Doubly-linked work list
	} *_zones;			// Array of zones
	u32 _zones_size;	// Size of array

	// Lists
	u16 _zone_work_head;	// List of active work items sorted by x
	u16 _zone_trigger_x;	// Next trigger x for _zone_next_x
	u16 _zone_next_x;		// Next work item in list while decoding scanline 
	u16 _zone_trigger_y;	// Next trigger y for _zone_next_y
	u16 _zone_next_y;		// Start of next row of same-y items to merge

	u32 *_colors;
	int _colors_size;

	void clear();

	int init(const ImageHeader *header);
	int readColorTable(ImageReader &reader);
	int readZones(ImageReader &reader);

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		double initUsec;
		double readColorTableUsec;
		double readZonesUsec;
		double overallUsec;

		int zoneCount;
	} Stats;
#endif

public:
	CAT_INLINE ImageLPReader() {
		_zones = 0;
		_colors = 0;
	}
	virtual CAT_INLINE ~ImageLPReader() {
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
	int triggerX(u8 *p, ImageReader &reader);

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

#endif // IMAGE_LP_READER_HPP

