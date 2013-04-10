#ifndef IMAGE_FILTER_WRITER_HPP
#define IMAGE_FILTER_WRITER_HPP

#include "Platform.hpp"
#include "ImageWriter.hpp"
#include "ImageMaskWriter.hpp"
#include "ImageLZWriter.hpp"
#include "EntropyEncoder.hpp"
#include "FilterScorer.hpp"
#include "Filters.hpp"

/*
 * Image Context Modeling Compressor
 *
 * This is based heavily on BCIF.  Notable improvements:
 *
 * + Better compression ratios
 * + Maintainable codebase for future improvements
 * + 2D LZ Exact Match and Fully-Transparent Alpha Mask integration
 * + Uses 4x4 zones instead of 8x8
 * + More spatial and color filters supported
 * + Top N heuristic filters are submitted to entropy-based selection
 * + Better filter matrix compression
 * + Only 8 chaos levels
 * + Encodes zero runs > ~256 without emitting more symbols for better AZ stats
 * + Better chaos/color huffman table compression
 */

namespace cat {


//#define FUZZY_CHAOS


static const int FILTER_SELECT_FUZZ = 16;
static const int COMPRESS_LEVEL = 1;
#ifdef FUZZY_CHAOS
static const int CHAOS_LEVELS = 16;
#else
static const int CHAOS_LEVELS = 8;
#endif

//// ImageCMWriter

class ImageCMWriter {
	int _w, _h;
	u16 *_matrix;
	u8 *_chaos;

	void clear();

	u8 *_rgba;
	int _width, _height;
	ImageMaskWriter *_mask;
	ImageLZWriter *_lz;

	EntropyEncoder _encoder[3][CHAOS_LEVELS];

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
		// For these SF = 0, CF = 1
		int filter_bytes[2], filter_table_bits[2];
		int filter_compressed_bits[2];

		int chaos_overhead_bits;

		// RGB data
		int rgb_bits[3];

		int chaos_bits;
		int total_bits;

		u32 chaos_count;
		double chaos_compression_ratio;

		double overall_compression_ratio;
	} Stats;
#endif // CAT_COLLECT_STATS

public:
	CAT_INLINE ImageCMWriter() {
		_matrix = 0;
		_chaos = 0;
	}
	CAT_INLINE virtual ~ImageCMWriter() {
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

	int initFromRGBA(u8 *rgba, int width, int height, ImageMaskWriter &mask, ImageLZWriter &lz);
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

