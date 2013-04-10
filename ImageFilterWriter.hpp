#ifndef IMAGE_FILTER_WRITER_HPP
#define IMAGE_FILTER_WRITER_HPP

#include "Platform.hpp"
#include "ImageWriter.hpp"
#include "ImageMaskWriter.hpp"
#include "EntropyEncoder.hpp"
#include "FilterScorer.hpp"
#include "Filters.hpp"

namespace cat {


//#define FUZZY_CHAOS
//#define CAT_FILTER_LZ
//#define CHAOS_CARE_LZ


static const int LZ_MINMATCH = 15; // Multiple of 3
static const int FILTER_MATCH_FUZZ = 20;
static const int COMPRESS_LEVEL = 1;
#ifdef FUZZY_CHAOS
static const int CHAOS_LEVELS = 16;
#else
static const int CHAOS_LEVELS = 8;
#endif

//// ImageFilterWriter

class ImageFilterWriter {
	int _w, _h;
	u16 *_matrix;
	u8 *_chaos;

	void clear();

	u8 *_rgba;
	int _width;
	int _height;
	ImageMaskWriter *_mask;

	EntropyEncoder _encoder[3][CHAOS_LEVELS];

#ifdef CAT_FILTER_LZ
	u16 lz_token_codes[256];
	u8 lz_token_lens[256];

	u16 lz_muck_codes[256];
	u8 lz_muck_lens[256];

	std::vector<u8> _lz_tokens, _lz_muck;
	u8 *_lz_mask;

	CAT_INLINE bool hasPixel(int x, int y) {
		return _lz_mask[x + y * _width];
	}

	void makeLZmask();
#endif

	int init(int width, int height);
	void decideFilters();
	void applyFilters();
	void chaosStats();

	void writeFilterHuffmanTable(u8 codelens[256], ImageWriter &writer, int stats_index);

	void writeFilters(ImageWriter &writer);
	bool writeChaos(ImageWriter &writer);

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
#ifdef CAT_FILTER_LZ
		int lz_token_ov, lz_other_ov, lz_huff_bits;
#endif

		// For these SF = 0, CF = 1
		int filter_bytes[2], filter_table_bits[2];
		int filter_compressed_bits[2];

		int chaos_overhead_bits;

		// RGB data
		int rgb_bits[3];

		int total_bits;
	} Stats;
#endif // CAT_COLLECT_STATS

public:
	CAT_INLINE ImageFilterWriter() {
		_matrix = 0;
		_chaos = 0;
	}
	CAT_INLINE virtual ~ImageFilterWriter() {
		clear();
	}

	CAT_INLINE void setFilter(int x, int y, u16 filter) {
		const int filterX = x >> FILTER_ZONE_SIZE_SHIFT;
		const int filterY = y >> FILTER_ZONE_SIZE_SHIFT;
		_matrix[filterX + filterY * _w] = filter;
	}

	CAT_INLINE u16 getFilter(int x, int y) {
		const int filterX = x >> FILTER_ZONE_SIZE_SHIFT;
		const int filterY = y >> FILTER_ZONE_SIZE_SHIFT;
		return _matrix[filterX + filterY * _w];
	}

	int initFromRGBA(u8 *rgba, int width, int height, ImageMaskWriter &mask);
	void write(ImageWriter &writer);

#ifdef CAT_COLLECT_STATS
	bool dumpStats();
#else
	CAT_INLINE bool dumpStats() {
		// Not implemented
		return false;
	}
#endif
};


} // namespace cat

#endif // IMAGE_FILTER_WRITER_HPP

