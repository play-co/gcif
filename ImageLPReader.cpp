#include "ImageLPReader.hpp"
#include "EndianNeutral.hpp"
#include "GCIFReader.hpp"
#include "Filters.hpp"
#include "BitMath.hpp"
using namespace cat;

#ifdef CAT_COLLECT_STATS
#include "Log.hpp"
#include "Clock.hpp"

static cat::Clock *m_clock = 0;
#endif // CAT_COLLECT_STATS


//// ImageLPReader

void ImageLPReader::clear() {
	if (_colors) {
		delete []_colors;
		_colors = 0;
	}
	if (_zones) {
		delete []_zones;
		_zones = 0;
	}
}

int ImageLPReader::init(const ImageHeader *header) {
	clear();

	_width = header->width;
	_height = header->height;

	return RE_OK;
}

int ImageLPReader::readColorTable(ImageReader &reader) {
	// Read and validate match count
	const u32 colors_size = reader.readBits(16);
	_colors_size = colors_size;

	// If we abort early, ensure that triggers are cleared
	_zone_work_head = ZONE_NULL;
	_zone_trigger_x = ZONE_NULL;
	_zone_next_x = ZONE_NULL;
	_zone_trigger_y = ZONE_NULL;
	_zone_next_y = ZONE_NULL;

	// If no matches,
	if (colors_size <= 0) {
		return RE_OK;
	}

	_colors = new u32[colors_size];

	// If enough colors are in the list,
	if (colors_size >= HUFF_COLOR_THRESH) {
		// Read the color filter
		YUV2RGBFilterFunction cff = YUV2RGB_FILTERS[reader.readBits(4)];

		// Read color decoders
		HuffmanDecoder color_decoder[4];
		for (int ii = 0; ii < 4; ++ii) {
			if (!color_decoder[ii].init(256, reader, 8)) {
				return RE_LP_CODES;
			}
		}

		// Read colors
		for (int ii = 0; ii < colors_size; ++ii) {
			// Read YUV
			u8 yuv[3];
			yuv[0] = reader.nextHuffmanSymbol(&color_decoder[0]);
			yuv[1] = reader.nextHuffmanSymbol(&color_decoder[1]);
			yuv[2] = reader.nextHuffmanSymbol(&color_decoder[2]);
			u8 a = reader.nextHuffmanSymbol(&color_decoder[3]);

			// Reverse color filter
			u8 rgb[3];
			cff(yuv, rgb);

			// Store decoded color
			_colors[ii] = getLE((u32)rgb[0] |
								((u32)rgb[1] << 8) |
								((u32)rgb[2] << 16) |
								((u32)a << 24));
		}
	} else {
		// Read colors
		for (int ii = 0; ii < colors_size; ++ii) {
			_colors[ii] = reader.readWord();
		}
	}

	// If file truncated,
	if (reader.eof()) {
		return RE_LP_CODES;
	}

	return RE_OK;
}

int ImageLPReader::readZones(ImageReader &reader) {
	// Read zone list size
	const int zones_size = reader.readBits(16);
	_zones_size = zones_size;

	// If invalid data,
	if (zones_size > MAX_ZONE_COUNT) {
		return RE_LP_CODES;
	}

	// Skip if nothing to read
	if (zones_size <= 0) {
		return RE_LP_CODES;
	}

	// Allocate space for zones
	_zones = new Zone[zones_size];
	Zone *z = _zones;

	if (zones_size >= HUFF_ZONE_THRESH) {
		static const int NUM_SYMS[6] = {
			256, 256, 256, 256, 256, MAX_COLORS
		};

		// Read zone decoders
		HuffmanDecoder zone_decoders[6];
		for (int ii = 0; ii < 6; ++ii) {
			if (!zone_decoders[ii].init(NUM_SYMS[ii], reader, 8)) {
				return RE_LP_CODES;
			}
		}

		// Read color index decoders
		HuffmanDecoder index_decoders[2];
		if (!index_decoders[0].init(256, reader, 8)) {
			return RE_LP_CODES;
		}
		if (_colors_size > 256) {
			if (!index_decoders[1].init(256, reader, 8)) {
				return RE_LP_CODES;
			}
		}

		// For each zone to read,
		u16 last_x = 0, last_y = 0;
		for (int ii = 0; ii < zones_size; ++ii, ++z) {
			u32 x = reader.nextHuffmanSymbol(&zone_decoders[0]);
			u32 x1 = reader.nextHuffmanSymbol(&zone_decoders[1]);
			x |= x1 << 8;
			u32 y = reader.nextHuffmanSymbol(&zone_decoders[2]);
			u32 y1 = reader.nextHuffmanSymbol(&zone_decoders[3]);
			y |= y1 << 8;
			u32 w = reader.nextHuffmanSymbol(&zone_decoders[4]) + ZONEW;
			u32 h = reader.nextHuffmanSymbol(&zone_decoders[4]) + ZONEH;
			u32 used = reader.nextHuffmanSymbol(&zone_decoders[5]) + 1;

			// Context modeling for the offsets
			if (y == 0) {
				x += last_x;
			}
			y += last_y;

			z->x = x;
			z->y = y;
			z->w = w;
			z->h = h;
			z->used = used;

			if (z->x >= _width || z->y >= _height) {
				CAT_WARN("TEST") << x << ", " << y;
				return RE_LP_CODES;
			}

			if ((u32)z->x + (u32)z->w >= _width || (u32)z->y + (u32)z->h >= _height) {
				CAT_WARN("TEST2");
				return RE_LP_CODES;
			}

			if (z->used > MAX_COLORS) {
				CAT_WARN("TEST3");
				return RE_LP_CODES;
			}

			if (used <= 1) {
				u32 c = reader.nextHuffmanSymbol(&index_decoders[0]);
				if (_colors_size > 256) {
					u32 c1 = reader.nextHuffmanSymbol(&index_decoders[1]);
					c |= c1 << 8;
				}

				if (c >= _colors_size) {
					CAT_WARN("TEST4");
					return RE_LP_CODES;
				}

				z->colors[0] = _colors[c];
			} else {
				// Read pixel decoder
				z->decoder.init(used, reader, 8);

				// Read colors
				for (int ii = 0; ii < used; ++ii) {
					u32 c = reader.nextHuffmanSymbol(&index_decoders[0]);
					if (_colors_size > 256) {
						u32 c1 = reader.nextHuffmanSymbol(&index_decoders[1]);
						c |= c1 << 8;
					}

					if (c >= _colors_size) {
						CAT_WARN("TEST5");
						return RE_LP_CODES;
					}

					z->colors[ii] = _colors[c];
				}
			}

			last_x = x;
			last_y = y;
			++z;
		}
	} else {
		int colorIndexBits = (int)BSR32(_colors_size - 1) + 1;

		// For each zone to read,
		for (int ii = 0; ii < zones_size; ++ii, ++z) {
			Zone *z = &_zones[ii];

			z->x = reader.readBits(16);
			z->y = reader.readBits(16);

			if (z->x >= _width || z->y >= _height) {
				return RE_LP_CODES;
			}

			z->w = reader.readBits(8) + ZONEW;
			z->h = reader.readBits(8) + ZONEH;

			if ((u32)z->x + (u32)z->w >= _width || (u32)z->y + (u32)z->h >= _height) {
				return RE_LP_CODES;
			}

			z->used = reader.readBits(4) + 1;

			if (z->used > MAX_COLORS) {
				return RE_LP_CODES;
			}

			// If at least two colors are present,
			if (z->used > 1) {
				z->decoder.init(z->used, reader, 8);
			}

			for (int jj = 0; jj < z->used; ++jj) {
				u32 colorIndex = reader.readBits(colorIndexBits);

				if (colorIndex >= _colors_size) {
					return RE_LP_CODES;
				}

				z->colors[jj] = _colors[colorIndex];
			}
		}
	}

	// If file truncated,
	if (reader.eof()) {
		return RE_LP_CODES;
	}

	// Trigger on first zone
	_zone_next_y = _zones[0].y;
	_zone_trigger_y = 0;

	return RE_OK;
}

int ImageLPReader::triggerX(u8 *p, ImageReader &reader) {
	// Just triggered a line from zi
	u16 ii = _zone_next_x;
	Zone *zi = &_zones[ii];

	u32 *dest = reinterpret_cast<u32 *>( p );

	if (zi->used <= 1) {
		u32 color = zi->colors[0];

		for (int jj = 0, jjend = zi->w; jj < jjend; ++jj) {
			dest[jj] = color;
		}
	} else {
		for (int jj = 0, jjend = zi->w; jj < jjend; ++jj) {
			u8 sym = reader.nextHuffmanSymbol(&zi->decoder);
			dest[jj] = zi->colors[sym];
		}
	}

	// Iterate ahead to next in work list
	_zone_next_x = zi->next;

	// If work list is exhausted,
	if (_zone_next_x == ZONE_NULL) {
		// Disable x trigger
		_zone_trigger_x = ZONE_NULL;
	} else {
		// Set it to the next trigger dx
		_zone_trigger_x = _zones[_zone_next_x].x;
	}

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

	return zi->w;
}

void ImageLPReader::triggerY() {
	// Merge y trigger zone and its same-y friends into the work list in order
	const u16 same_dy = _zone_trigger_y;
	u16 ii = _zone_next_y, jj;
	Zone *zi = &_zones[ii], *zj = 0;

	// Find insertion point for all same-y zones
	for (jj = _zone_work_head; jj != ZONE_NULL; jj = zj->next) {
		zj = &_zones[jj];

		// If existing item is to the right of where we want to insert,
		while (zj->x >= zi->x) {
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
			if (++ii >= _zones_size) {
				// Stop triggering y
				_zone_trigger_y = ZONE_NULL;
				_zone_next_y = ZONE_NULL;
				goto setup_trigger_x;
			}

			// Update zi to point at next item
			zi = &_zones[ii];

			// If the next zone is on another row,
			if (zi->y != same_dy) {
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
		if (zj) {
			// Insert at end of list
			zj->next = ii;
		} else {
			_zone_work_head = ii;
		}

		// j = i
		zj = zi;
		jj = ii;

		// If out of zones,
		if (++ii >= _zones_size) {
			// Stop triggering y
			_zone_trigger_y = ZONE_NULL;
			_zone_next_y = ZONE_NULL;
			goto setup_trigger_x;
		}

		// i = next i
		zi = &_zones[ii];
	} while (zi->y == same_dy);

setup_trigger_y:
	// Set up next y trigger at zone index i
	_zone_trigger_y = zi->y;
	_zone_next_y = ii;

setup_trigger_x:
	// Set up next x trigger at head of work list
	_zone_next_x = _zone_work_head;
	_zone_trigger_x = _zones[_zone_work_head].x;
}

int ImageLPReader::read(ImageReader &reader) {
#ifdef CAT_COLLECT_STATS
	m_clock = Clock::ref();

	double t0 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	int err;

	if ((err = init(reader.getImageHeader()))) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t1 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	if ((err = readColorTable(reader))) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t2 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	if ((err = readZones(reader))) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t3 = m_clock->usec();

	Stats.initUsec = t1 - t0;
	Stats.readColorTableUsec = t2 - t1;
	Stats.readZonesUsec = t3 - t2;
	Stats.zoneCount = _zones_size;
	Stats.overallUsec = t3 - t0;
	Stats.zoneBytes = Stats.zoneCount * 10;
#endif // CAT_COLLECT_STATS

	return RE_OK;
}

#ifdef CAT_COLLECT_STATS

bool ImageLPReader::dumpStats() {
	CAT_INANE("stats") << "(LP Decode)   Initialization : " << Stats.initUsec << " usec (" << Stats.initUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(LP Decode) Read Color Table : " << Stats.readColorTableUsec << " usec (" << Stats.readColorTableUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(LP Decode)       Read Zones : " << Stats.readZonesUsec << " usec (" << Stats.readZonesUsec * 100.f / Stats.overallUsec << " %total)";
	CAT_INANE("stats") << "(LP Decode)          Overall : " << Stats.overallUsec << " usec";

	CAT_INANE("stats") << "(LP Decode)       Zone Count : " << Stats.zoneCount << " zones (" << Stats.zoneBytes << " bytes)";
	CAT_INANE("stats") << "(LP Decode)       Throughput : " << Stats.zoneBytes / Stats.overallUsec << " MBPS (output bytes/time)";

	return true;
}

#endif

