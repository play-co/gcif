#ifndef IMAGE_FILTER_WRITER_HPP
#define IMAGE_FILTER_WRITER_HPP

#include "ImageWriter.hpp"
#include "ImageMaskWriter.hpp"
#include "ImageLZWriter.hpp"
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
 * + Much better compression ratios
 * + Maintainable codebase for future improvements
 * + 2D LZ Exact Match and Fully-Transparent Alpha Mask integration
 * + Uses 4x4 zones instead of 8x8
 * + More spatial and color filters supported
 * + Top (FILTER_SELECT_FUZZ) filters are submitted to entropy-based selection
 * + Only 8 chaos levels
 * + Encodes zero runs > ~256 without emitting more symbols for better AZ stats
 * + Better Huffman table compression
 * + Recent color palette extra symbols
 */

namespace cat {


//#define USE_RECENT_PALETTE

//// ImageCMWriter

class ImageCMWriter {
protected:
	static const int CHAOS_LEVELS_MAX = ImageCMReader::CHAOS_LEVELS_MAX;
	static const int FILTER_SELECT_FUZZ = 20;
	static const int COMPRESS_LEVEL = 1;
	static const u16 UNUSED_FILTER = 0xffff;
	static const u16 TODO_FILTER = 0;
	static const int COLOR_PLANES = ImageCMReader::COLOR_PLANES;
#if 0
	static const int RECENT_SYMS_Y = ImageCMReader::RECENT_SYMS_Y; // >= U
	static const int RECENT_SYMS_U = ImageCMReader::RECENT_SYMS_U;
	static const int RECENT_AHEAD_Y = ImageCMReader::RECENT_AHEAD_Y;
	static const int RECENT_AHEAD_U = ImageCMReader::RECENT_AHEAD_U;
#endif
	static const int ZRLE_SYMS_Y = ImageCMReader::ZRLE_SYMS_Y;
	static const int ZRLE_SYMS_U = ImageCMReader::ZRLE_SYMS_U;
	static const int ZRLE_SYMS_V = ImageCMReader::ZRLE_SYMS_V;
	static const int ZRLE_SYMS_A = ImageCMReader::ZRLE_SYMS_A;
	static const int PALETTE_MIN = 2;
	static const int CHAOS_THRESH = 4000; // Number of chaos-encoded pixels required to use chaos levels

	int _w, _h;
	u16 *_matrix;
	u8 *_chaos;
	int _chaos_size;

	int _chaos_levels;
	const u8 *_chaos_table;

	void clear();

	const u8 *_rgba;
	int _width, _height;
	ImageMaskWriter *_mask;
	ImageLZWriter *_lz;

	// Filter Huffman codes
	HuffmanEncoder<SF_COUNT> _sf_encoder;
	HuffmanEncoder<CF_COUNT> _cf_encoder;

	EntropyEncoder<256 + 1, ZRLE_SYMS_Y> _y_encoder[CHAOS_LEVELS_MAX];
	EntropyEncoder<256 + 1, ZRLE_SYMS_U> _u_encoder[CHAOS_LEVELS_MAX];
	EntropyEncoder<256 + 1, ZRLE_SYMS_V> _v_encoder[CHAOS_LEVELS_MAX];
	EntropyEncoder<256 + 1, ZRLE_SYMS_A> _a_encoder[CHAOS_LEVELS_MAX];

	int init(int width, int height);
	void maskFilters();
	void decideFilters();
	bool applyFilters();
	void chaosStats();

	bool writeFilters(ImageWriter &writer);
	bool writeChaos(ImageWriter &writer);

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
		// For these SF = 0, CF = 1
		int filter_table_bits[2];
		int filter_compressed_bits[2];

		int chaos_overhead_bits;

		// RGB data
		int rgb_bits[COLOR_PLANES];

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

	int initFromRGBA(const u8 *rgba, int width, int height, ImageMaskWriter &mask, ImageLZWriter &lz);
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

