#include "ImageLZReader.hpp"
#include "EndianNeutral.hpp"
#include "HuffmanDecoder.hpp"
using namespace cat;

#ifdef CAT_COLLECT_STATS
#include "Log.hpp"
#include "Clock.hpp"

static cat::Clock *m_clock = 0;
#endif // CAT_COLLECT_STATS


//// ImageLZReader

void ImageLZReader::clear() {
	if (_zones) {
		delete []_zones;
		_zones = 0;
	}
}

int ImageLZReader::init(const ImageInfo *info) {
	clear();

	return RE_OK;
}

int ImageLZReader::readTables(ImageReader &reader) {
	static const int NUM_SYMS = 256;

	// Read and validate match count
	u32 match_count = reader.readBits(16);
	_zone_count = match_count;

	_zones = new u8[match_count * 10];

	// If invalid data,
	if (match_count > MAX_ZONE_COUNT) {
		return RE_LZ_CODES;
	}

	// If no matches,
	if (match_count <= 0) {
		return RE_OK;
	}

	// Read codelens
	u8 codelens[NUM_SYMS];
	for (int ii = 0; ii < NUM_SYMS; ++ii) {
		u32 s, len = 0;
		while ((s = reader.readBits(4)) >= 15) { // EOF will read zeroes back so no need to check it really
			len += 15;
		}
		len += s;

		codelens[ii] = len;
	}

	// If file truncated,
	if (reader.eof()) {
		return RE_LZ_CODES;
	}

	// If not able to init Huffman decoder
	if (!_huffman.init(NUM_SYMS, codelens, HUFF_TABLE_BITS)) {
		return RE_LZ_CODES;
	}

	return RE_OK;
}

int ImageLZReader::read(ImageReader &reader) {
#ifdef CAT_COLLECT_STATS
	m_clock = Clock::ref();

	double t0 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	int err;

	if ((err = init(reader.getImageInfo()))) {
		return err;
	}

#ifdef CAT_COLLECT_STATS
	double t1 = m_clock->usec();
#endif // CAT_COLLECT_STATS


#ifdef CAT_COLLECT_STATS
	double t2 = m_clock->usec();
#endif // CAT_COLLECT_STATS

	return RE_OK;
}

#ifdef CAT_COLLECT_STATS

bool ImageLZReader::dumpStats() {
	//CAT_INFO("stats") << "(LZ Decompress) Initial matches : " << Stats.initial_matches;

	return true;
}

#endif

