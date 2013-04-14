#ifndef IMAGE_FILTER_WRITER_HPP
#define IMAGE_FILTER_WRITER_HPP

#include "Platform.hpp"
#include "ImageWriter.hpp"
#include "ImageMaskWriter.hpp"
#include "ImageLZWriter.hpp"
#include "ImageLPWriter.hpp"
#include "ImageCMReader.hpp"
#include "EntropyEncoder.hpp"
#include "FilterScorer.hpp"
#include "Filters.hpp"

/*
 * Game Closure Context Modeling (GC-CM) Compression
 *
 * This is based heavily on BCIF by Stefano Brocchi
 * from his PhD thesis "Bidimensional pictures: reconstruction, expression and encoding" (Dec 2009)
 * http://www.dsi.unifi.it/DRIIA/RaccoltaTesi/Brocchi.pdf
 *
 * Notable improvements:
 * + Better compression ratios
 * + Maintainable codebase for future improvements
 * + 2D Local Palette, 2D LZ Exact Match, and Fully-Transparent Alpha Mask integration
 * + Uses 4x4 zones instead of 8x8
 * + More spatial and color filters supported
 * + Top (FILTER_SELECT_FUZZ) filters are submitted to entropy-based selection
 * + Better filter matrix compression
 * + Only 8 chaos levels
 * + Encodes zero runs > ~256 without emitting more symbols for better AZ stats
 * + Better chaos/color Huffman table compression
 */

namespace cat {


//// ImageCMWriter

class ImageCMWriter {
protected:
	static const int CHAOS_LEVELS = ImageCMReader::CHAOS_LEVELS;
	static const int FILTER_SELECT_FUZZ = 16;
	static const int COMPRESS_LEVEL = 1;
	static const u16 UNUSED_FILTER = 0xffff;

	int _w, _h;
	u16 *_matrix;
	u8 *_chaos;

	void clear();

	const u8 *_rgba;
	int _width, _height;
	ImageMaskWriter *_mask;
	ImageLZWriter *_lz;
	ImageLPWriter *_lp;

	// Filter Huffman codes
	u16 _sf_codes[SF_COUNT];
	u8 _sf_codelens[SF_COUNT];
	u8 _sf_unused_sym;
	u16 _cf_codes[CF_COUNT];
	u8 _cf_codelens[CF_COUNT];
	u8 _cf_unused_sym;

	EntropyEncoder _encoder[3][CHAOS_LEVELS];

	int init(int width, int height);
	void decideFilters();
	void chaosStats();

	void writeFilterHuffmanTable(int num_syms, u8 codelens[256], ImageWriter &writer, int stats_index);

	void writeFilters(ImageWriter &writer);
	bool writeChaos(ImageWriter &writer);

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		// For these SF = 0, CF = 1
		int filter_table_bits[2];
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

	int initFromRGBA(const u8 *rgba, int width, int height, ImageMaskWriter &mask, ImageLZWriter &lz, ImageLPWriter &lp);
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

