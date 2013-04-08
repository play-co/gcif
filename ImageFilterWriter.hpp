#ifndef IMAGE_FILTER_WRITER_HPP
#define IMAGE_FILTER_WRITER_HPP

#include "Platform.hpp"
#include "ImageWriter.hpp"
#include "ImageMaskWriter.hpp"
#include "EntropyEncoder.hpp"
#include "FilterScorer.hpp"

namespace cat {


static const int FILTER_ZONE_SIZE = 4;
static const int FILTER_MATCH_FUZZ = 20;

//#define CAT_FILTER_LZ


//// ImageFilterWriter

class ImageFilterWriter {
	int _w, _h;
	u16 *_matrix;
	u8 *_chaos;

	std::vector<u8> _lz;

	u8 *_lz_mask;

	void clear();

	u8 *_rgba;
	int _width;
	int _height;
	ImageMaskWriter *_mask;

	EntropyEncoder _encoder[3][16];

#ifdef CAT_FILTER_LZ
	CAT_INLINE bool hasR(int x, int y) {
		return _lz_mask[(x + y * _width) * 3];
	}

	CAT_INLINE bool hasG(int x, int y) {
		return _lz_mask[(x + y * _width) * 3 + 1];
	}

	CAT_INLINE bool hasB(int x, int y) {
		return _lz_mask[(x + y * _width) * 3 + 2];
	}

	CAT_INLINE bool hasPixel(int x, int y) {
		return hasR(x, y) || hasG(x, y) || hasB(x, y);
	}
#endif

	bool init(int width, int height);
#ifdef CAT_FILTER_LZ
	void makeLZmask();
#endif
	void decideFilters();
	void applyFilters();
	void chaosStats();

	void writeFilterHuffmanTable(u8 codelens[256], ImageWriter &writer, int stats_index);

	void writeFilters(ImageWriter &writer);
	void writeChaos(ImageWriter &writer);

#ifdef CAT_COLLECT_STATS
public:
	struct _Stats {
#ifdef CAT_FILTER_LZ
		u32 lz_lit_len_ov, lz_token_ov, lz_offset_ov, lz_match_len_ov, lz_overall_ov;
		u32 lz_huff_bits;
#endif

		// For these SF = 0, CF = 1
		int filter_bytes[2], filter_pivot[2], filter_table_bits[2];

		int rleBytes, lzBytes;

		double filterUsec, rleUsec, lzUsec, histogramUsec;
		double generateTableUsec, tableEncodeUsec, dataEncodeUsec;
		double overallUsec;

		int originalDataBytes, compressedDataBytes;

		double compressionRatio;
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
		_matrix[(x / FILTER_ZONE_SIZE) + (y / FILTER_ZONE_SIZE) * _w] = filter;
	}

	CAT_INLINE u16 getFilter(int x, int y) {
		return _matrix[(x / FILTER_ZONE_SIZE) + (y / FILTER_ZONE_SIZE) * _w];
	}

	int initFromRGBA(u8 *rgba, int width, int height, ImageMaskWriter &mask);
	void write(ImageWriter &writer);
};


} // namespace cat

#endif // IMAGE_FILTER_WRITER_HPP

