#include "ImageCMWriter.hpp"
#include "BitMath.hpp"
#include "Filters.hpp"
#include "EntropyEstimator.hpp"
#include "Log.hpp"
#include "ImageLZWriter.hpp"
#include "GCIFWriter.hpp"
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


//// ImageCMWriter

void ImageCMWriter::clear() {
	if (_matrix) {
		delete []_matrix;
		_matrix = 0;
	}
	if (_chaos) {
		delete []_chaos;
		_chaos = 0;
	}
}

int ImageCMWriter::init(int width, int height) {
	clear();

	if (width < FILTER_ZONE_SIZE || height < FILTER_ZONE_SIZE) {
		return WE_BAD_DIMS;
	}

	if ((width & FILTER_ZONE_SIZE_MASK) || (height & FILTER_ZONE_SIZE_MASK)) {
		return WE_BAD_DIMS;
	}

	_width = width;
	_height = height;

	_w = width >> FILTER_ZONE_SIZE_SHIFT;
	_h = height >> FILTER_ZONE_SIZE_SHIFT;
	_matrix = new u16[_w * _h];

	// And last row of chaos data
	_chaos_size = (width + RECENT_SYMS_Y) * PLANES;
	_chaos = new u8[_chaos_size];

	return WE_OK;
}

void ImageCMWriter::decideFilters() {
	EntropyEstimator<u8> ee[3];
	for (int ii = 0; ii < 3; ++ii) {
		ee[ii].clear(256);
	}

	FilterScorer scores;
	scores.init(SF_COUNT * CF_COUNT);

	int compressLevel = COMPRESS_LEVEL;
	const int width = _width;

	for (int y = 0; y < _height; y += FILTER_ZONE_SIZE) {
		for (int x = 0; x < width; x += FILTER_ZONE_SIZE) {

			// Determine best filter combination to use
			int bestSF = 0, bestCF = 0;

			// If filter zone has RGB data,
			{
				// Lower compression level that is a lot faster:
				if (compressLevel == 0) {
					scores.reset();

					// For each pixel in the 8x8 zone,
					for (int yy = 0; yy < FILTER_ZONE_SIZE; ++yy) {
						for (int xx = 0; xx < FILTER_ZONE_SIZE; ++xx) {
							int px = x + xx, py = y + yy;
							if (_mask->hasRGB(px, py)) {
								continue;
							}
							if (_lz->visited(px, py)) {
								continue;
							}

							const u8 *p = _rgba + (px + py * width) * 4;

							for (int ii = 0; ii < SF_COUNT; ++ii) {
								const u8 *pred = SPATIAL_FILTERS[ii](p, px, py, width);

								u8 temp[3];
								for (int jj = 0; jj < 3; ++jj) {
									temp[jj] = p[jj] - pred[jj];
								}

								for (int jj = 0; jj < CF_COUNT; ++jj) {
									u8 yuv[3];
									RGB2YUV_FILTERS[jj](temp, yuv);

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
							if (_mask->hasRGB(px, py)) {
								continue;
							}
							if (_lz->visited(px, py)) {
								continue;
							}

							const u8 *p = _rgba + (px + py * width) * 4;

							for (int ii = 0; ii < SF_COUNT; ++ii) {
								const u8 *pred = SPATIAL_FILTERS[ii](p, px, py, width);
								u8 temp[3];
								for (int jj = 0; jj < 3; ++jj) {
									temp[jj] = p[jj] - pred[jj];
								}

								for (int jj = 0; jj < CF_COUNT; ++jj) {
									u8 yuv[3];
									RGB2YUV_FILTERS[jj](temp, yuv);

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
						const int TOP_COUNT = FILTER_SELECT_FUZZ;

						FilterScorer::Score *top = scores.getTop(TOP_COUNT);

						double bestScore = 0;

						for (int ii = 0; ii < TOP_COUNT; ++ii) {
							// Write it out
							u8 sf = top[ii].index % SF_COUNT;
							u8 cf = top[ii].index / SF_COUNT;

							for (int jj = 0; jj < 3; ++jj) {
								ee[jj].setup();
							}

							for (int yy = 0; yy < FILTER_ZONE_SIZE; ++yy) {
								for (int xx = 0; xx < FILTER_ZONE_SIZE; ++xx) {
									int px = x + xx, py = y + yy;
									if (_mask->hasRGB(px, py)) {
										continue;
									}
									if (_lz->visited(px, py)) {
										continue;
									}

									const u8 *p = _rgba + (px + py * width) * 4;
									const u8 *pred = SPATIAL_FILTERS[sf](p, px, py, width);
									u8 temp[3];
									for (int jj = 0; jj < 3; ++jj) {
										temp[jj] = p[jj] - pred[jj];
									}

									u8 yuv[3];
									RGB2YUV_FILTERS[cf](temp, yuv);

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
								for (int jj = 0; jj < 3; ++jj) {
									ee[jj].save();
								}
							} else {
								if (score < bestScore) {
									bestSF = sf;
									bestCF = cf;
									for (int jj = 0; jj < 3; ++jj) {
										ee[jj].save();
									}
									bestScore = score;
								}
							}
						}

						for (int jj = 0; jj < 3; ++jj) {
							ee[jj].commit();
						}
					}
				}
			}

			u16 filter = ((u16)bestSF << 8) | bestCF;
			setFilter(x, y, filter);
		}
	}
}

void ImageCMWriter::chaosStats() {
#ifdef GENERATE_CHAOS_TABLE
	GenerateChaosTable();
#endif

	// Find number of pixels to encode
	int chaos_count = 0;
	for (int y = 0; y < _height; ++y) {
		for (int x = 0; x < _width; ++x) {
			if (!_lz->visited(x, y) && !_mask->hasRGB(x, y)) {
				++chaos_count;
			}
		}
	}

#ifdef CAT_COLLECT_STATS
	Stats.chaos_count = chaos_count;
#endif

	// If it is above a threshold,
	if (chaos_count >= CHAOS_THRESH) {
		CAT_DEBUG_ENFORCE(CHAOS_LEVELS_MAX == 8);

		// Use more chaos levels for better compression
		_chaos_levels = CHAOS_LEVELS_MAX;
		_chaos_table = CHAOS_TABLE_8;
	} else {
		_chaos_levels = 1;
		_chaos_table = CHAOS_TABLE_1;
	}

	const int width = _width;

	// For each scanline,
	const u8 *p = _rgba;
	u8 *lastStart = _chaos + RECENT_SYMS_Y * PLANES;
	CAT_CLR(_chaos, _chaos_size);

	const u8 *CHAOS_TABLE = _chaos_table;

	for (int y = 0; y < _height; ++y) {
		u8 *last = lastStart;

		// For each pixel,
		for (int x = 0; x < width; ++x) {
			// If not masked out,
			if (!_lz->visited(x, y) && !_mask->hasRGB(x, y)) {
				// Get filter for this pixel
				u16 filter = getFilter(x, y);
				u8 cf = (u8)filter;
				u8 sf = (u8)(filter >> 8);

				// Apply spatial filter
				const u8 *pred = SPATIAL_FILTERS[sf](p, x, y, width);
				u8 temp[3];
				for (int jj = 0; jj < 3; ++jj) {
					temp[jj] = p[jj] - pred[jj];
				}

				// Apply color filter
				u8 yuv[PLANES];
				RGB2YUV_FILTERS[cf](temp, yuv);
				if (x > 0) {
					yuv[3] = p[-1] - p[3];
				} else {
					yuv[3] = 255 - p[3];
				}

				bool matched = false;
				u8 chaos = CHAOS_TABLE[chaosScore(last[0 - 4]) + chaosScore(last[0])];
#ifdef USE_RECENT_PALETTE
				if (yuv[0] != 0) {
					for (int ii = 0; ii < RECENT_SYMS_Y; ++ii) {
						const int offset = ii - RECENT_AHEAD_Y;

						if (yuv[0] == last[0 - offset * 4] &&
							yuv[1] == last[1 - offset * 4] &&
							yuv[2] == last[2 - offset * 4] &&
							yuv[3] == last[3 - offset * 4]) {
							if ((yuv[0] != 0) + (yuv[1] != 0) + (yuv[2] != 0) + (yuv[3] != 0) < PALETTE_MIN) {
								continue;
							}
							_y_encoder[chaos].add(256 + ii);
							matched = true;
							break;
						}
					}
#if 1
				} else if (yuv[1] != 0) {
					for (int ii = 0; ii < RECENT_SYMS_U; ++ii) {
						const int offset = ii - RECENT_AHEAD_U;

						if (yuv[1] == last[1 - offset * 4] &&
							yuv[2] == last[2 - offset * 4] &&
							yuv[3] == last[3 - offset * 4]) {
							if ((yuv[0] != 0) + (yuv[1] != 0) + (yuv[2] != 0) + (yuv[3] != 0) < PALETTE_MIN) {
								continue;
							}
							_y_encoder[chaos].add(0);
							_u_encoder[chaos].add(256 + ii);
							matched = true;
							break;
						}
					}
#endif
				}
#endif

				if (!matched) {
					_y_encoder[chaos].add(yuv[0]);
					chaos = CHAOS_TABLE[chaosScore(last[1 - 4]) + chaosScore(last[1])];
					_u_encoder[chaos].add(yuv[1]);
					chaos = CHAOS_TABLE[chaosScore(last[2 - 4]) + chaosScore(last[2])];
					_v_encoder[chaos].add(yuv[2]);
					chaos = CHAOS_TABLE[chaosScore(last[3 - 4]) + chaosScore(last[3])];
					_a_encoder[chaos].add(yuv[3]);
				}

				for (int c = 0; c < PLANES; ++c) {
					last[c] = yuv[c];
				}
			} else {
				for (int c = 0; c < PLANES; ++c) {
					last[c] = 0;
				}
			}

			// Next pixel
			last += PLANES;
			p += 4;
		}
	}

	// Finalize
	for (int jj = 0; jj < _chaos_levels; ++jj) {
		_y_encoder[jj].finalize();
		_u_encoder[jj].finalize();
		_v_encoder[jj].finalize();
		_a_encoder[jj].finalize();
	}
}

int ImageCMWriter::initFromRGBA(const u8 *rgba, int width, int height, ImageMaskWriter &mask, ImageLZWriter &lz) {
	int err;

	if ((err = init(width, height))) {
		return err;
	}

	_rgba = rgba;
	_mask = &mask;
	_lz = &lz;

#ifdef TEST_COLOR_FILTERS
	testColorFilters();
	return -1;
#endif

	decideFilters();

	chaosStats();

	return WE_OK;
}

bool ImageCMWriter::writeFilters(ImageWriter &writer) {
	FreqHistogram<SF_COUNT> sf_hist;
	FreqHistogram<CF_COUNT> cf_hist;
	u32 unused_count = 0;

	// For each zone,
	for (int y = 0, height = _height; y < height; y += FILTER_ZONE_SIZE) {
		for (int x = 0, width = _width; x < width; x += FILTER_ZONE_SIZE) {
			// Encode SF and CF separately and combine consecutive filters
			// together for the smallest representation
			bool on = true;

			int w, h;
			if (!_lz->findExtent(x, y, w, h) ||
				w < FILTER_ZONE_SIZE || h < FILTER_ZONE_SIZE) {
				for (int ii = 0; ii < FILTER_ZONE_SIZE; ++ii) {
					for (int jj = 0; jj < FILTER_ZONE_SIZE; ++jj) {
						if (!_mask->hasRGB(x + ii, y + jj)) {
							on = false;
							ii = FILTER_ZONE_SIZE;
							break;
						}
					}
				}
			}

			if (!on)
			{
				u16 filter = getFilter(x, y);
				u8 cf = (u8)filter;
				u8 sf = (u8)(filter >> 8);

				sf_hist.add(sf);
				cf_hist.add(cf);
			} else {
				if (y == 0) CAT_WARN("TEST") << x << "," << y;
				setFilter(x, y, UNUSED_FILTER);

				unused_count++;
			}
		}
	}

	// Geneerate huffman codes from final histogram
	if (!_sf_encoder.init(sf_hist)) {
		return false;
	}
	if (!_cf_encoder.init(cf_hist)) {
		return false;
	}

	// Write out filter huffman tables
	int sf_table_bits = _sf_encoder.writeTable(writer);
	int cf_table_bits = _cf_encoder.writeTable(writer);
#ifdef CAT_COLLECT_STATS
	Stats.filter_table_bits[0] = sf_table_bits;
	Stats.filter_table_bits[1] = cf_table_bits;
#endif // CAT_COLLECT_STATS

	return true;
}

bool ImageCMWriter::writeChaos(ImageWriter &writer) {
#ifdef CAT_COLLECT_STATS
	int overhead_bits = 0;
	int bitcount[PLANES] = {0};
	int filter_table_bits[2] = {0};
#endif

	CAT_DEBUG_ENFORCE(_chaos_levels <= 8);

	writer.writeBits(_chaos_levels - 1, 3);

	int bits = 3;

	for (int jj = 0; jj < _chaos_levels; ++jj) {
		bits += _y_encoder[jj].writeTables(writer);
		bits += _u_encoder[jj].writeTables(writer);
		bits += _v_encoder[jj].writeTables(writer);
		bits += _a_encoder[jj].writeTables(writer);
	}
#ifdef CAT_COLLECT_STATS
	overhead_bits += bits;
#endif

	const int width = _width;

	// For each scanline,
	const u8 *p = _rgba;
	u8 *lastStart = _chaos + RECENT_SYMS_Y * PLANES;
	CAT_CLR(_chaos, _chaos_size);

	const u8 *CHAOS_TABLE = _chaos_table;

	for (int y = 0; y < _height; ++y) {
		u8 *last = lastStart;

		writer.writeWord(1234567);

		// For each pixel,
		for (int x = 0; x < width; ++x) {
			// If it is time to write out a filter,
			if ((x & FILTER_ZONE_SIZE_MASK) == 0 &&
				(y & FILTER_ZONE_SIZE_MASK) == 0) {

				u16 filter = getFilter(x, y);

				// If filter is not unused,
				if (filter != UNUSED_FILTER) {
					u8 sf = filter >> 8;
					u8 cf = (u8)filter;

					int sf_bits = _sf_encoder.writeSymbol(sf, writer);
					int cf_bits = _cf_encoder.writeSymbol(cf, writer);
#ifdef CAT_COLLECT_STATS
					filter_table_bits[0] += sf_bits;
					filter_table_bits[1] += cf_bits;
#endif
					if (y == 1) CAT_WARN("COMP") << x << " : FILTER " << (int)sf << "," << (int)cf;
				}
			}

			// If not masked out,
			if (!_lz->visited(x, y) && !_mask->hasRGB(x, y)) {
				// Get filter for this pixel
				u16 filter = getFilter(x, y);
				CAT_DEBUG_ENFORCE(filter != UNUSED_FILTER);
				u8 cf = (u8)filter;
				u8 sf = (u8)(filter >> 8);

				if (y == 2) CAT_WARN("COMP") << x << " : ORIG " << (int)p[0] << "," << (int)p[1] << "," << (int)p[2] << "," << (int)p[3];

				// Apply spatial filter
				const u8 *pred = SPATIAL_FILTERS[sf](p, x, y, width);
				u8 temp[3];
				for (int jj = 0; jj < 3; ++jj) {
					temp[jj] = p[jj] - pred[jj];
				}

				// Apply color filter
				u8 YUVA[PLANES];
				RGB2YUV_FILTERS[cf](temp, YUVA);
				if (x > 0) {
					YUVA[3] = p[-1] - p[3];
				} else {
					YUVA[3] = 255 - p[3];
				}

				bool matched = false;
				u8 chaos = CHAOS_TABLE[chaosScore(last[0 - 4]) + chaosScore(last[0])];
#ifdef USE_RECENT_PALETTE
				if (YUVA[0] != 0) {
					for (int ii = 0; ii < RECENT_SYMS_Y; ++ii) {
						const int offset = ii - RECENT_AHEAD_Y;

						if (YUVA[0] == last[0 - offset * 4] &&
							YUVA[1] == last[1 - offset * 4] &&
							YUVA[2] == last[2 - offset * 4] &&
							YUVA[3] == last[3 - offset * 4]) {
							if ((YUVA[0] != 0) + (YUVA[1] != 0) + (YUVA[2] != 0) + (YUVA[3] != 0) < PALETTE_MIN) {
								continue;
							}
							int bits = _y_encoder[chaos].write(256 + ii, writer);
#ifdef CAT_COLLECT_STATS
							bitcount[0] += bits;
#endif
							matched = true;
							break;
						}
					}
#if 1
				} else if (YUVA[1] != 0) {
					for (int ii = 0; ii < RECENT_SYMS_U; ++ii) {
						const int offset = ii - RECENT_AHEAD_U;

						if (YUVA[1] == last[1 - offset * 4] &&
							YUVA[2] == last[2 - offset * 4] &&
							YUVA[3] == last[3 - offset * 4]) {
							if ((YUVA[0] != 0) + (YUVA[1] != 0) + (YUVA[2] != 0) + (YUVA[3] != 0) < PALETTE_MIN) {
								continue;
							}
							int bits = _y_encoder[chaos].write(0, writer);
#ifdef CAT_COLLECT_STATS
							bitcount[0] += bits;
#endif
							bits = _u_encoder[chaos].write(256 + ii, writer);
#ifdef CAT_COLLECT_STATS
							bitcount[1] += bits;
#endif
							matched = true;
							break;
						}
					}
#endif
				}
#endif

				if (!matched) {
					int bits = _y_encoder[chaos].write(YUVA[0], writer);
#ifdef CAT_COLLECT_STATS
					bitcount[0] += bits;
#endif
					chaos = CHAOS_TABLE[chaosScore(last[1 - 4]) + chaosScore(last[1])];
					bits = _u_encoder[chaos].write(YUVA[1], writer);
#ifdef CAT_COLLECT_STATS
					bitcount[1] += bits;
#endif
					chaos = CHAOS_TABLE[chaosScore(last[2 - 4]) + chaosScore(last[2])];
					bits = _v_encoder[chaos].write(YUVA[2], writer);
#ifdef CAT_COLLECT_STATS
					bitcount[2] += bits;
#endif
					chaos = CHAOS_TABLE[chaosScore(last[3 - 4]) + chaosScore(last[3])];
					if (y == 2) CAT_WARN("CHAOS") << x << " : " << (int)chaos << " from " << (int)last[3 - 4] << " and " << (int)last[3] << " - " << (int)(last - lastStart);
					bits = _a_encoder[chaos].write(YUVA[3], writer);
#ifdef CAT_COLLECT_STATS
					bitcount[3] += bits;
#endif
				}

				if (y == 2) CAT_WARN("COMP") << x << " : READ " << (int)YUVA[0] << "," << (int)YUVA[1] << "," << (int)YUVA[2] << "," << (int)YUVA[3];

				for (int c = 0; c < PLANES; ++c) {
					last[c] = YUVA[c];
				}
			} else {
				if (y == 2) CAT_WARN("COMP") << x << " : LZ/ALPHA SKIP";
				for (int c = 0; c < PLANES; ++c) {
					last[c] = 0;
				}
			}

			writer.writeWord(7654321);

			// Next pixel
			last += PLANES;
			p += 4;
		}
	}

#ifdef CAT_COLLECT_STATS
	for (int ii = 0; ii < PLANES; ++ii) {
		Stats.rgb_bits[ii] = bitcount[ii];
	}
	Stats.chaos_overhead_bits = overhead_bits;
	Stats.filter_compressed_bits[0] = filter_table_bits[0];
	Stats.filter_compressed_bits[1] = filter_table_bits[1];
#endif

	return true;
}

void ImageCMWriter::write(ImageWriter &writer) {
	writeFilters(writer);

	writeChaos(writer);

#ifdef CAT_COLLECT_STATS
	int total = 0;
	for (int ii = 0; ii < 2; ++ii) {
		total += Stats.filter_table_bits[ii];
		total += Stats.filter_compressed_bits[ii];
	}
	for (int ii = 0; ii < PLANES; ++ii) {
		total += Stats.rgb_bits[ii];
	}
	total += Stats.chaos_overhead_bits;
	Stats.chaos_bits = total;
	total += _lz->Stats.huff_bits;
	total += _mask->Stats.compressedDataBits;
	Stats.total_bits = total;

	Stats.overall_compression_ratio = _width * _height * 4 * 8 / (double)Stats.total_bits;

	Stats.chaos_compression_ratio = Stats.chaos_count * PLANES * 8 / (double)Stats.chaos_bits;
#endif
}

#ifdef CAT_COLLECT_STATS

bool ImageCMWriter::dumpStats() {
	CAT_INANE("stats") << "(CM Compress) Spatial Filter Table Size : " <<  Stats.filter_table_bits[0] << " bits (" << Stats.filter_table_bits[0]/8 << " bytes)";
	CAT_INANE("stats") << "(CM Compress) Spatial Filter Compressed Size : " <<  Stats.filter_compressed_bits[0] << " bits (" << Stats.filter_compressed_bits[0]/8 << " bytes)";

	CAT_INANE("stats") << "(CM Compress) Color Filter Table Size : " <<  Stats.filter_table_bits[1] << " bits (" << Stats.filter_table_bits[1]/8 << " bytes)";
	CAT_INANE("stats") << "(CM Compress) Color Filter Compressed Size : " <<  Stats.filter_compressed_bits[1] << " bits (" << Stats.filter_compressed_bits[1]/8 << " bytes)";

	CAT_INANE("stats") << "(CM Compress) Y-Channel Compressed Size : " <<  Stats.rgb_bits[0] << " bits (" << Stats.rgb_bits[0]/8 << " bytes)";
	CAT_INANE("stats") << "(CM Compress) U-Channel Compressed Size : " <<  Stats.rgb_bits[1] << " bits (" << Stats.rgb_bits[1]/8 << " bytes)";
	CAT_INANE("stats") << "(CM Compress) V-Channel Compressed Size : " <<  Stats.rgb_bits[2] << " bits (" << Stats.rgb_bits[2]/8 << " bytes)";
	CAT_INANE("stats") << "(CM Compress) A-Channel Compressed Size : " <<  Stats.rgb_bits[3] << " bits (" << Stats.rgb_bits[3]/8 << " bytes)";

	CAT_INANE("stats") << "(CM Compress) YUVA Overhead Size : " << Stats.chaos_overhead_bits << " bits (" << Stats.chaos_overhead_bits/8 << " bytes)";
	CAT_INANE("stats") << "(CM Compress) Chaos pixel count : " << Stats.chaos_count << " pixels";
	CAT_INANE("stats") << "(CM Compress) Chaos compression ratio : " << Stats.chaos_compression_ratio << ":1";
	CAT_INANE("stats") << "(CM Compress) Overall size : " << Stats.total_bits << " bits (" << Stats.total_bits/8 << " bytes)";
	CAT_INANE("stats") << "(CM Compress) Overall compression ratio : " << Stats.overall_compression_ratio << ":1";

	return true;
}

#endif

