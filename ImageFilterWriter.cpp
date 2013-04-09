#include "ImageFilterWriter.hpp"
#include "BitMath.hpp"
#include "Filters.hpp"
#include "EntropyEstimator.hpp"
#include "Log.hpp"
using namespace cat;

#include <vector>
using namespace std;

#include "lz4.h"
#include "lz4hc.h"
#include "Log.hpp"
#include "HuffmanEncoder.hpp"
#include "lodepng.h"

#include <iostream>
using namespace std;


static CAT_INLINE int chaosScore(u8 p) {
	if (p < 128) {
		return p;
	} else {
		return 256 - p;
	}
}

static CAT_INLINE int score(u8 p) {
	if (p < 128) {
		return p;
	} else {
		return 256 - p;
	}
}

static CAT_INLINE int scoreYUV(u8 *yuv) {
	return score(yuv[0]) + score(yuv[1]) + score(yuv[2]);
}

static CAT_INLINE int wrapNeg(u8 p) {
	if (p == 0) {
		return 0;
	} else if (p < 128) {
		return ((p - 1) << 1) | 1;
	} else {
		return (256 - p) << 1;
	}
}


static int calcBits(vector<u8> &lz, u8 codelens[256]) {
	int bits = 0;

	for (int ii = 0; ii < lz.size(); ++ii) {
		int sym = lz[ii];
		bits += codelens[sym];
	}

	return bits;
}

static void filterColor(int cf, const u8 *p, const u8 *pred, u8 *out) {
	u8 r = p[0] - pred[0];
	u8 g = p[1] - pred[1];
	u8 b = p[2] - pred[2];

	u8 temp[3] = {r, g, b};

	convertRGBtoYUV(cf, temp, out);
}


//// ImageFilterWriter

void ImageFilterWriter::clear() {
	if (_matrix) {
		delete []_matrix;
		_matrix = 0;
	}
	if (_chaos) {
		delete []_chaos;
		_chaos = 0;
	}
}

int ImageFilterWriter::init(int width, int height) {
	clear();

	if (width < FILTER_ZONE_SIZE || height < FILTER_ZONE_SIZE) {
		return WE_BAD_DIMS;
	}

	if ((width & FILTER_ZONE_SIZE_MASK) || (height & FILTER_ZONE_SIZE_MASK)) {
		return WE_BAD_DIMS;
	}

	_w = width >> FILTER_ZONE_SIZE_SHIFT;
	_h = height >> FILTER_ZONE_SIZE_SHIFT;
	_matrix = new u16[_w * _h];
	_chaos = new u8[width * 3 + 3];

	return WE_OK;
}

void ImageFilterWriter::decideFilters() {
	u16 *filterWriter = _matrix;

	EntropyEstimator<u8> ee[3];
	ee[0].clear(256);
	ee[1].clear(256);
	ee[2].clear(256);

	FilterScorer scores;
	scores.init(SF_COUNT * CF_COUNT);

	int compressLevel = 1;
	const int width = _width;

	for (int y = 0; y < _height; y += FILTER_ZONE_SIZE) {
		for (int x = 0; x < width; x += FILTER_ZONE_SIZE) {

			// Determine best filter combination to use
			int bestSF = 0, bestCF = 0;

			// If filter zone has RGB data,
#ifdef LOWRES_MASK
			if (!_mask->hasRGB(x, y))
#endif
			{
				// Lower compression level that is a lot faster:
				if (compressLevel == 0) {
					scores.reset();

					// For each pixel in the 8x8 zone,
					for (int yy = 0; yy < FILTER_ZONE_SIZE; ++yy) {
						for (int xx = 0; xx < FILTER_ZONE_SIZE; ++xx) {
							int px = x + xx, py = y + yy;
#ifndef LOWRES_MASK
							if (_mask->hasRGB(px, py)) {
								continue;
							}
#endif
#ifdef CAT_FILTER_LZ
							if (!hasPixel(px, py)) {
								continue;
							}
#endif

							u8 *p = _rgba + (px + py * width) * 4;

							for (int ii = 0; ii < SF_COUNT; ++ii) {
								const u8 *pred = spatialFilterPixel(p, ii, px, py, width);

								for (int jj = 0; jj < CF_COUNT; ++jj) {
									u8 yuv[3];
									filterColor(jj, p, pred, yuv);
									int error = scoreYUV(yuv);

									scores.add(ii + jj*SF_COUNT, error);
								}
							}
						}
					}

					FilterScorer::Score *best = scores.getLowest();

					// Write it out
					bestSF = best->index % SF_COUNT;
					bestCF = best->index / SF_COUNT;

				} else { // Higher compression level that uses entropy estimate:

					scores.reset();

					// For each pixel in the 8x8 zone,
					for (int yy = 0; yy < FILTER_ZONE_SIZE; ++yy) {
						for (int xx = 0; xx < FILTER_ZONE_SIZE; ++xx) {
							int px = x + xx, py = y + yy;
#ifndef LOWRES_MASK
							if (_mask->hasRGB(px, py)) {
								continue;
							}
#endif
#ifdef CAT_FILTER_LZ
							if (!hasPixel(px, py)) {
								continue;
							}
#endif

							u8 *p = _rgba + (px + py * width) * 4;

							for (int ii = 0; ii < SF_COUNT; ++ii) {
								const u8 *pred = spatialFilterPixel(p, ii, px, py, width);

								for (int jj = 0; jj < CF_COUNT; ++jj) {
									u8 sp[3] = {
										p[0] - pred[0],
										p[1] - pred[1],
										p[2] - pred[2]
									};

									u8 yuv[3];
									convertRGBtoYUV(jj, sp, yuv);

									int error = scoreYUV(yuv);

									scores.add(ii + SF_COUNT*jj, error);
								}
							}
						}
					}


					FilterScorer::Score *lowest = scores.getLowest();

					if (lowest->score <= 4) {
						bestSF = lowest->index % SF_COUNT;
						bestCF = lowest->index / SF_COUNT;
					} else {
						const int TOP_COUNT = FILTER_MATCH_FUZZ;

						FilterScorer::Score *top = scores.getTop(TOP_COUNT);

						double bestScore = 0;

						for (int ii = 0; ii < TOP_COUNT; ++ii) {
							// Write it out
							u8 sf = top[ii].index % SF_COUNT;
							u8 cf = top[ii].index / SF_COUNT;

							ee[0].setup();
							ee[1].setup();
							ee[2].setup();

							for (int yy = 0; yy < FILTER_ZONE_SIZE; ++yy) {
								for (int xx = 0; xx < FILTER_ZONE_SIZE; ++xx) {
									int px = x + xx, py = y + yy;
#ifndef LOWRES_MASK
									if (_mask->hasRGB(px, py)) {
										continue;
									}
#endif
#ifdef CAT_FILTER_LZ
									if (!hasPixel(px, py)) {
										continue;
									}
#endif

									u8 *p = _rgba + (px + py * width) * 4;
									const u8 *pred = spatialFilterPixel(p, sf, px, py, width);

									u8 sp[3] = {
										p[0] - pred[0],
										p[1] - pred[1],
										p[2] - pred[2]
									};

									u8 yuv[3];
									convertRGBtoYUV(cf, sp, yuv);

									ee[0].push(yuv[0]);
									ee[1].push(yuv[1]);
									ee[2].push(yuv[2]);
								}
							}

							double score = ee[0].entropy() + ee[1].entropy() + ee[2].entropy();
							if (ii == 0) {
								bestScore = score;
								bestSF = sf;
								bestCF = cf;
								ee[0].save();
								ee[1].save();
								ee[2].save();
							} else {
								if (score < bestScore) {
									bestSF = sf;
									bestCF = cf;
									ee[0].save();
									ee[1].save();
									ee[2].save();
									bestScore = score;
								}
							}
						}

						ee[0].commit();
						ee[1].commit();
						ee[2].commit();
					}
				}
			}

			u16 filter = ((u16)bestSF << 8) | bestCF;
			setFilter(x, y, filter);
		}
	}
}

void ImageFilterWriter::applyFilters() {
	u16 *filterWriter = _matrix;
	const int width = _width;

	// For each zone,
	for (int y = _height - 1; y >= 0; --y) {
		for (int x = width - 1; x >= 0; --x) {
			u16 filter = getFilter(x, y);
			u8 cf = (u8)filter;
			u8 sf = (u8)(filter >> 8);
			if (_mask->hasRGB(x, y)) {
				continue;
			}

			u8 *p = _rgba + (x + y * width) * 4;
			const u8 *pred = spatialFilterPixel(p, sf, x, y, width);

			filterColor(cf, p, pred, p);

#if 0
			p[0] = p[0];
			p[1] = p[0];
			p[2] = p[0];
#endif

#if 0
			p[0] = score(p[0]);
			p[1] = score(p[1]);
			p[2] = score(p[2]);
#endif


#if 0
			if (!(y & FILTER_ZONE_SIZE_MASK) && !(x & FILTER_ZONE_SIZE_MASK)) {
				rgba[(x + y * width) * 4] = 200;
				rgba[(x + y * width) * 4 + 1] = 200;
				rgba[(x + y * width) * 4 + 2] = 200;
			}
#endif
		}
	}
}



//#define GENERATE_CHAOS_TABLE
#ifdef GENERATE_CHAOS_TABLE
static int CalculateChaos(int sum) {
	if (sum <= 0) {
		return 0;
	} else {
		int chaos = BSR32(sum - 1) + 1;
		if (chaos > 7) {
			chaos = 7;
		}
		return chaos;
	}
}
#include <iostream>
using namespace std;
void GenerateChaosTable() {
	cout << "static const u8 CHAOS_TABLE[512] = {";

	for (int sum = 0; sum < 256*2; ++sum) {
		if ((sum & 31) == 0) {
			cout << endl << '\t';
		}
		cout << CalculateChaos(sum) << ",";
	}

	cout << endl << "};" << endl;
}
#endif // GENERATE_CHAOS_TABLE


static const u8 CHAOS_TABLE[512] = {
	0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
};






void ImageFilterWriter::chaosStats() {
#ifdef GENERATE_CHAOS_TABLE
	GenerateChaosTable();
#endif

	int bitcount[3] = {0};
	const int width = _width;

#ifdef FUZZY_CHAOS
	u8 *last_chaos = _chaos;
	CAT_CLR(last_chaos, width * 3 + 3);
#endif

	u8 *last = _rgba;
	u8 *now = _rgba;

	vector<u8> test;

	for (int y = 0; y < _height; ++y) {
		u8 left_rgb[3] = {0};
#ifdef FUZZY_CHAOS
		u8 *last_chaos_read = last_chaos + 3;
#endif

		for (int x = 0; x < width; ++x) {
			u16 chaos[3] = {left_rgb[0], left_rgb[1], left_rgb[2]};
			if (y > 0) {
				chaos[0] += chaosScore(last[0]);
				chaos[1] += chaosScore(last[1]);
				chaos[2] += chaosScore(last[2]);
				last += 4;
			}

			chaos[0] = CHAOS_TABLE[chaos[0]];
			chaos[1] = CHAOS_TABLE[chaos[1]];
			chaos[2] = CHAOS_TABLE[chaos[2]];
#ifdef FUZZY_CHAOS
			u16 isum = last_chaos_read[0] + last_chaos_read[-3];
			chaos[0] += (isum + chaos[0] + (isum >> 1)) >> 2;
			chaos[1] += (last_chaos_read[1] + last_chaos_read[-2] + chaos[1] + (chaos[0] >> 1)) >> 2;
			chaos[2] += (last_chaos_read[2] + last_chaos_read[-1] + chaos[2] + (chaos[1] >> 1)) >> 2;
#else
			//chaos[1] = (chaos[1] + chaos[0]) >> 1;
			//chaos[2] = (chaos[2] + chaos[1]) >> 1;
#endif
			left_rgb[0] = chaosScore(now[0]);
			left_rgb[1] = chaosScore(now[1]);
			left_rgb[2] = chaosScore(now[2]);
			{
				test.push_back(chaos[1] * 256/8);
				test.push_back(chaos[1] * 256/8);
				test.push_back(chaos[1] * 256/8);

				if (!_mask->hasRGB(x, y)) {
					for (int ii = 0; ii < 3; ++ii) {
#ifdef CAT_FILTER_LZ
						if (_lz_mask[(x + y * _width) * 3 + ii])
#endif
						{
							_encoder[ii][chaos[ii]].push(now[ii]);
						}
					}
				}
			}

#ifdef FUZZY_CHAOS
			last_chaos_read[0] = (chaos[0] + 1) >> 1;
			last_chaos_read[1] = (chaos[1] + 1) >> 1;
			last_chaos_read[2] = (chaos[2] + 1) >> 1;
			last_chaos_read += 3;
#endif

			now += 4;
		}
	}

	for (int ii = 0; ii < 3; ++ii) {
		for (int jj = 0; jj < CHAOS_LEVELS; ++jj) {
			_encoder[ii][jj].finalize();
		}
	}

}



#if 0

void colorSpace(u8 *rgba, int width, int height, ImageMaskWriter &mask) {
	for (int cf = 0; cf < CF_COUNT; ++cf) {
		EntropyEstimator<u32> ee[3];
		for (int ii = 0; ii < 3; ++ii) {
			ee[ii].clear(256);
			ee[ii].setup();
		}

		u8 *p = rgba;
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				u8 yuv[3];
				convertRGBtoYUV(cf, p, yuv);

				ee[0].push(yuv[0]);
				ee[1].push(yuv[1]);
				ee[2].push(yuv[2]);

				p += 4;
			}
		}
/*
		if (cf == CF_YCgCo_R) {
			ee[0].drawHistogram(rgba, width);
			ee[1].drawHistogram(rgba + 800, width);
			ee[2].drawHistogram(rgba + 1600, width);
			return;
		}
*/
		double e[3], score = 0;
		for (int ii = 0; ii < 3; ++ii) {
			e[ii] = ee[ii].entropy();
			score += e[ii];
		}

		cout << "YUV888 Entropy for " << GetColorFilterString(cf) << " = { " << e[0] << ", " << e[1] << ", " << e[2] << " } : SCORE=" << score << endl;
	}
}
#endif



#ifdef CAT_FILTER_LZ

void ImageFilterWriter::makeLZmask() {
	static const int MINMATCH = 20;

	const int lz_mask_size = _width * _height * 3;
	_lz_mask = new u8[lz_mask_size];

	u8 *pixel = _rgba;
	u8 *writer = _lz_mask;
	int x, y;

	vector<u8> lz_input;

	for (y = 0; y < _height; ++y) {
		for (x = 0; x < _width; ++x) {
			if (!_mask->hasRGB(x, y)) {
				lz_input.push_back(pixel[0]);
				lz_input.push_back(pixel[1]);
				lz_input.push_back(pixel[2]);
			} else {
				writer[0] = 0;
				writer[1] = 0;
				writer[2] = 0;
			}
			writer += 3;
			pixel += 4;
		}
	}

	_lz.resize(LZ4_compressBound(static_cast<int>( lz_input.size() )));

	_lz.resize(LZ4_compressHCMinMatch((char*)&lz_input[0], (char*)&_lz[0], lz_input.size(), MINMATCH));

#if 0
	cout << "Original data:" << endl;
	for (int ii = 0; ii < 100; ++ii) {
		cout << hex << (int)lz_input[ii] << dec << " ";
	}
	cout << endl;

	cout << "LZ stream:" << endl;
	for (int ii = 0; ii < 100; ++ii) {
		cout << hex << (int)_lz[ii] << dec << " ";
	}
	cout << endl;

	cout << "LZ bytes = " << _lz.size() << endl;

	vector<u8> test;
	test.resize(lz_input.size());

	int result = LZ4_uncompress((char*)&_lz[0], (char*)&test[0], test.size());

	if (result < 0) {
		cout << "WARNING: Decompression test failed!  Only recovered " << result << " bytes of " << lz_input.size() << endl;
	}

	for (int ii = 0; ii < test.size(); ++ii) {
		if (test[ii] != lz_input[ii]) {
//			for (int jj = 0; jj < test.size(); ++jj) {
//				cout << hex << (int)test[ii] << dec << " ";
//			}
			cout << "DATA MISMATCH at " << ii << " expected " << hex << (int)lz_input[ii] << dec << endl;
			break;
		}
	}
#endif

	const int size = _lz.size();

	int moffset = 0;

	u8 *lzmask = _lz_mask;

	vector<u8> lz_overhead;

	int lit_len_ov = 0;
	int token_ov = 0;
	int match_len_ov = 0;
	int offset_ov = 0;

	for (int ii = 0; ii < size;) {
		// Read token
		u8 token = _lz[ii++];
		lz_overhead.push_back(token);
		++token_ov;

		// Read Literal Length
		int literalLength = token >> 4;
		if (literalLength == 15) {
			int s;
			do {
				s = _lz[ii++];
				lz_overhead.push_back(s);
				literalLength += s;
				++lit_len_ov;
			} while (s == 255 && ii < size);
		}

		// TODO: Not good input checking
		for (int jj = 0; jj < literalLength; ++jj) {
			while (_mask->hasRGB((moffset/3) % _width, (moffset/3) / _width)) {
				moffset += 3;
			}

			int symbol = _lz[ii++];

			lzmask[moffset++] = 1;
		}

		if (ii >= size) break;

		// Read match offset
		u8 offset0 = _lz[ii++];
		if (ii >= size) break;
		u8 offset1 = _lz[ii++];
		if (ii >= size) break;
		u16 woffset = ((u16)offset1 << 8) | offset0;
		lz_overhead.push_back(offset0);
		lz_overhead.push_back(offset1);
		offset_ov += 2;

		// Read match length
		int matchLength = token & 15;
		if (matchLength == 15) {
			int s;
			do {
				s = _lz[ii++];
				lz_overhead.push_back(s);
				matchLength += s;
				++match_len_ov;
			} while (s == 255 && ii < size);
		}
		matchLength += MINMATCH;

		for (int jj = 0; jj < matchLength; ++jj) {
			while (_mask->hasRGB((moffset/3) % _width, (moffset/3) / _width)) {
				moffset += 3;
			}

			lzmask[moffset++] = 0;
		}
	}

	cout << endl;

	Stats.lz_lit_len_ov = lit_len_ov;
	Stats.lz_token_ov = token_ov;
	Stats.lz_match_len_ov = match_len_ov;
	Stats.lz_offset_ov = offset_ov;
	Stats.lz_overall_ov = (int)lz_overhead.size();

	cout << "Literal length overhead = " << lit_len_ov << endl;
	cout << "Token overhead = " << token_ov << endl;
	cout << "Match length overhead = " << match_len_ov << endl;
	cout << "Offset overhead = " << offset_ov << endl;

	// NOTE: Do not need this after it works
	while (moffset < lz_mask_size &&
		   _mask->hasRGB((moffset/3) % _width, (moffset/3) / _width)) {
		moffset += 3;
	}

	if (moffset != 1024 * 1024 * 3) {
		cout << ">>> Offset " << moffset << " does not match expectation " << 1024*1024*3 << ".  There is no way this just worked!" << endl;
	}

	u16 freq[256];

	collectFreqs(256, lz_overhead, freq);

	u16 codes[256];
	u8 lens[256];
	generateHuffmanCodes(256, freq, codes, lens);

	int bits_overhead = calcBits(lz_overhead, lens);
	Stats.lz_huff_bits = bits_overhead;
	Stats.lz_huff_bits = 0;

	cout << "LZ overhead = " << bits_overhead/8 << endl;
}


#endif

int ImageFilterWriter::initFromRGBA(u8 *rgba, int width, int height, ImageMaskWriter &mask) {
	int err;

	if ((err = init(width, height))) {
		return err;
	}

	_width = width;
	_height = height;
	_rgba = rgba;
	_mask = &mask;

#if 0
	testColorFilters();
	colorSpace(rgba, width, height, mask);
	return 0;
#endif

#ifdef CAT_FILTER_LZ
	makeLZmask();
#endif
	decideFilters();
	applyFilters();

	chaosStats();

	return WE_OK;
}

void ImageFilterWriter::writeFilterHuffmanTable(u8 codelens[256], ImageWriter &writer, int stats_index) {
	const int HUFF_TABLE_SIZE = 256;

#ifdef CAT_COLLECT_STATS
	int bitcount = 0;
#endif

	for (int ii = 0; ii < HUFF_TABLE_SIZE; ++ii) {
		u8 len = codelens[ii];

		while (len >= 15) {
			writer.writeBits(15, 4);
			len -= 15;
#ifdef CAT_COLLECT_STATS
			bitcount += 4;
#endif
		}

		writer.writeBits(len, 4);
#ifdef CAT_COLLECT_STATS
		bitcount += 4;
#endif
	}

#ifdef CAT_COLLECT_STATS
	Stats.filter_table_bits[stats_index] = bitcount;
#endif // CAT_COLLECT_STATS
}

void ImageFilterWriter::writeFilters(ImageWriter &writer) {
	vector<u8> data[2];

	bool writing = false;
	u8 sf_last = 0, cf_last = 0;

	// For each zone,
	for (int y = 0, height = _height; y < height; y += FILTER_ZONE_SIZE) {
		for (int x = 0, width = _width; x < width; x += FILTER_ZONE_SIZE) {
			// Encode SF and CF separately and combine consecutive filters
			// together for the smallest representation
#ifdef LOWRES_MASK
			if (!_mask->hasRGB(x, y))
#else
			bool on = false;

			for (int ii = 0; ii < FILTER_ZONE_SIZE; ++ii) {
				for (int jj = 0; jj < FILTER_ZONE_SIZE; ++jj) {
					if (!_mask->hasRGB(x + ii, y + jj)) {
						on = true;
						ii = FILTER_ZONE_SIZE;
						break;
					}
				}
			}

			if (on)
#endif
			{
				u16 filter = getFilter(x, y);
				u8 cf = (u8)filter;
				u8 sf = (u8)(filter >> 8);

				if (writing) {
					data[0].push_back((sf << 4) | sf_last);
					data[1].push_back((cf << 4) | cf_last);
				} else {
					sf_last = sf;
					cf_last = cf;
				}

				writing = !writing;
			}
		}
	}

	if (writing) {
		data[0].push_back(sf_last);
		data[1].push_back(cf_last);
	}

	for (int ii = 0; ii < 2; ++ii) {
#ifdef CAT_COLLECT_STATS
		Stats.filter_bytes[ii] = (int)data[ii].size();
		int bitcount = 0;
#endif

		u16 freq[256];

		collectFreqs(256, data[ii], freq);

		u16 codes[256];
		u8 lens[256];
		generateHuffmanCodes(256, freq, codes, lens);

		writeFilterHuffmanTable(lens, writer, ii);

		for (int jj = 0, len = (int)data[ii].size(); jj < len; ++jj) {
			u8 symbol = data[ii][jj];

			u8 bits = lens[symbol];
			u16 code = codes[symbol];

			writer.writeBits(code, bits);

#ifdef CAT_COLLECT_STATS
			bitcount += bits;
#endif
		}

#ifdef CAT_COLLECT_STATS
		Stats.filter_compressed_bits[ii] = bitcount;
#endif
	}
}

void ImageFilterWriter::writeChaos(ImageWriter &writer) {
#ifdef CAT_COLLECT_STATS
	int overhead_bits = 0;
	int bitcount[3] = {0};
#endif

	for (int ii = 0; ii < 3; ++ii) {
		for (int jj = 0; jj < CHAOS_LEVELS; ++jj) {
			int bits = _encoder[ii][jj].writeOverhead(writer);
#ifdef CAT_COLLECT_STATS
			overhead_bits += bits;
#endif
		}
	}

	const int width = _width;

	u8 *last_chaos = _chaos;
	CAT_CLR(last_chaos, width * 3 + 3);

	u8 *last = _rgba;
	u8 *now = _rgba;

	for (int y = 0; y < _height; ++y) {
		u8 left_rgb[3] = {0};
		u8 *last_chaos_read = last_chaos + 3;

		for (int x = 0; x < width; ++x) {
			u16 chaos[3] = {left_rgb[0], left_rgb[1], left_rgb[2]};
			if (y > 0) {
				chaos[0] += chaosScore(last[0]);
				chaos[1] += chaosScore(last[1]);
				chaos[2] += chaosScore(last[2]);
				last += 4;
			}

			chaos[0] = CHAOS_TABLE[chaos[0]];
			chaos[1] = CHAOS_TABLE[chaos[1]];
			chaos[2] = CHAOS_TABLE[chaos[2]];
#if 0
			u16 isum = last_chaos_read[0] + last_chaos_read[-3];
			chaos[0] += (isum + chaos[0] + (isum >> 1)) >> 2;
			chaos[1] += (last_chaos_read[1] + last_chaos_read[-2] + chaos[1] + (chaos[0] >> 1)) >> 2;
			chaos[2] += (last_chaos_read[2] + last_chaos_read[-1] + chaos[2] + (chaos[1] >> 1)) >> 2;
#else
			//chaos[1] = (chaos[1] + chaos[0]) >> 1;
			//chaos[2] = (chaos[2] + chaos[1]) >> 1;
#endif
			left_rgb[0] = chaosScore(now[0]);
			left_rgb[1] = chaosScore(now[1]);
			left_rgb[2] = chaosScore(now[2]);
			{
				if (!_mask->hasRGB(x, y)) {
					for (int ii = 0; ii < 3; ++ii) {
#ifdef CAT_FILTER_LZ
						if (_lz_mask[(x + y * _width) * 3 + ii])
#endif
						{
							int bits = _encoder[ii][chaos[ii]].encode(now[ii], writer);
#ifdef CAT_COLLECT_STATS
							bitcount[ii] += bits;
#endif
						}
					}
				}
			}

			last_chaos_read[0] = chaos[0] >> 1;
			last_chaos_read[1] = chaos[1] >> 1;
			last_chaos_read[2] = chaos[2] >> 1;
			last_chaos_read += 3;

			now += 4;
		}
	}

#ifdef CAT_COLLECT_STATS
	for (int ii = 0; ii < 3; ++ii) {
		Stats.rgb_bits[ii] = bitcount[ii];
	}
	Stats.chaos_overhead_bits = overhead_bits;
#endif
}

void ImageFilterWriter::write(ImageWriter &writer) {
	writeFilters(writer);
	writeChaos(writer);

#ifdef CAT_COLLECT_STATS
	int total = 0;
	for (int ii = 0; ii < 2; ++ii) {
		total += Stats.filter_table_bits[ii];
		total += Stats.filter_compressed_bits[ii];
	}
	for (int ii = 0; ii < 3; ++ii) {
		total += Stats.rgb_bits[ii];
	}
	total += Stats.chaos_overhead_bits;
	Stats.total_bits = total;
#endif
}

#ifdef CAT_COLLECT_STATS

bool ImageFilterWriter::dumpStats() {
	CAT_INFO("stats") << "(RGB Compress) Spatial Filter Table Size : " <<  Stats.filter_table_bits[0] << " bits (" << Stats.filter_table_bits[0]/8 << " bytes)";
	CAT_INFO("stats") << "(RGB Compress) Spatial Filter Raw Size : " <<  Stats.filter_bytes[0] << " bytes";
	CAT_INFO("stats") << "(RGB Compress) Spatial Filter Compressed Size : " <<  Stats.filter_compressed_bits[0] << " bits (" << Stats.filter_compressed_bits[0]/8 << " bytes)";

	CAT_INFO("stats") << "(RGB Compress) Color Filter Table Size : " <<  Stats.filter_table_bits[1] << " bits (" << Stats.filter_table_bits[1]/8 << " bytes)";
	CAT_INFO("stats") << "(RGB Compress) Color Filter Raw Size : " <<  Stats.filter_bytes[1] << " bytes";
	CAT_INFO("stats") << "(RGB Compress) Color Filter Compressed Size : " <<  Stats.filter_compressed_bits[1] << " bits (" << Stats.filter_compressed_bits[1]/8 << " bytes)";

	CAT_INFO("stats") << "(RGB Compress) Y-Channel Compressed Size : " <<  Stats.rgb_bits[0] << " bits (" << Stats.rgb_bits[0]/8 << " bytes)";
	CAT_INFO("stats") << "(RGB Compress) U-Channel Compressed Size : " <<  Stats.rgb_bits[1] << " bits (" << Stats.rgb_bits[1]/8 << " bytes)";
	CAT_INFO("stats") << "(RGB Compress) V-Channel Compressed Size : " <<  Stats.rgb_bits[2] << " bits (" << Stats.rgb_bits[2]/8 << " bytes)";

	CAT_INFO("stats") << "(RGB Compress) YUV Overhead Size : " << Stats.chaos_overhead_bits << " bits (" << Stats.chaos_overhead_bits/8 << " bytes)";
	CAT_INFO("stats") << "(RGB Compress) Total size : " << Stats.total_bits << " bits (" << Stats.total_bits/8 << " bytes)";

	return true;
}

#endif

