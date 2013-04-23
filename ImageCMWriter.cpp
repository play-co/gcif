#include "ImageCMWriter.hpp"
#include "BitMath.hpp"
#include "Filters.hpp"
#include "EntropyEstimator.hpp"
#include "Log.hpp"
#include "ImageLZWriter.hpp"
using namespace cat;

#include <vector>
using namespace std;

#include "lz4.h"
#include "lz4hc.h"
#include "Log.hpp"
#include "HuffmanEncoder.hpp"
#include "lodepng.h"


static CAT_INLINE int scoreYUV(u8 *yuv) {
	return chaosScore(yuv[0]) + chaosScore(yuv[1]) + chaosScore(yuv[2]);
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

	_filter_replacements.clear();
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
	_chaos_size = (width + 1) * COLOR_PLANES;
	_chaos = new u8[_chaos_size];

	return WE_OK;
}

void ImageCMWriter::designFilters() {
	// Initialize spatial filter subsystem
	ResetSpatialFilters();

	// If disabled,
	if (!_knobs->cm_designFilters) {
		CAT_INANE("CM") << "Skipping filter design";
		return;
	}

	/* Inputs: A, B, C, D
	 *
	 * aA + bB + cC + dD
	 * a,b,c,d = {-2, -1, -1/2, 0, 1/2, 1, 2}
	 */

	const int width = _width;

	FilterScorer scores;
	scores.init(SF_COUNT + TAPPED_COUNT);

	int bestHist[SF_COUNT + TAPPED_COUNT] = {0};

	CAT_INANE("CM") << "Designing filters...";

	for (int y = 0; y < _height; y += FILTER_ZONE_SIZE) {
		for (int x = 0; x < width; x += FILTER_ZONE_SIZE) {
			// If this zone is skipped,
			if (getFilter(x, y) == UNUSED_FILTER) {
				continue;
			}

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

					int A[3] = {0};
					int B[3] = {0};
					int C[3] = {0};
					int D[3] = {0};

					for (int cc = 0; cc < 3; ++cc) {
						if (px > 0) {
							A[cc] = p[(-1) * 4 + cc];
						}
						if (py > 0 ) {
							B[cc] = p[(-width) * 4 + cc];
							if (px > 0) {
								C[cc] = p[(-width - 1) * 4 + cc];
							}
							if (px < width - 1) {
								D[cc] = p[(-width + 1) * 4 + cc];
							}
						}
					}

					for (int ii = 0; ii < SF_COUNT; ++ii) {
						const u8 *pred = SPATIAL_FILTERS[ii](p, px, py, width);

						int sum = 0;

						for (int cc = 0; cc < 3; ++cc) {
							int err = p[cc] - (int)pred[cc];
							if (err < 0) err = -err;
							sum += err;
						}
						scores.add(ii, sum);
					}

					for (int ii = 0; ii < TAPPED_COUNT; ++ii) {
						const int a = FILTER_TAPS[ii][0];
						const int b = FILTER_TAPS[ii][1];
						const int c = FILTER_TAPS[ii][2];
						const int d = FILTER_TAPS[ii][3];

						int sum = 0;
						for (int cc = 0; cc < 3; ++cc) {
							const int pred = (u8)((a * A[cc] + b * B[cc] + c * C[cc] + d * D[cc]) / 2);
							int err = p[cc] - pred;
							if (err < 0) err = -err;
							sum += err;
						}

						scores.add(ii + SF_COUNT, sum);
					}
				}
			}

			// Super Mario Kart scoring
			FilterScorer::Score *top = scores.getLowest();
			bestHist[top[0].index] += 4;

			top = scores.getTop(4);
			bestHist[top[0].index] += 1;
			bestHist[top[1].index] += 1;
			bestHist[top[2].index] += 1;
			bestHist[top[3].index] += 1;
		}
	}

	// Replace filters
	for (int jj = 0; jj < SF_COUNT; ++jj) {
		// Find worst default filter
		int lowest_sf = 0x7fffffffUL, lowest_index = 0;

		for (int ii = 0; ii < SF_COUNT; ++ii) {
			if (bestHist[ii] < lowest_sf) {
				lowest_sf = bestHist[ii];
				lowest_index = ii;
			}
		}

		// Find best custom filter
		int best_tap = -1, highest_index = -1;

		for (int ii = 0; ii < TAPPED_COUNT; ++ii) {
			int score = bestHist[ii + SF_COUNT];

			if (score > best_tap) {
				best_tap = score;
				highest_index = ii;
			}
		}

		// If it not an improvement,
		if (best_tap <= lowest_sf) {
			break;
		}

		// Verify it is good enough to bother with
		double ratio = best_tap / (double)lowest_sf;
		if (ratio < _knobs->cm_minTapQuality) {
			break;
		}

		// Insert it at this location
		const int a = FILTER_TAPS[highest_index][0];
		const int b = FILTER_TAPS[highest_index][1];
		const int c = FILTER_TAPS[highest_index][2];
		const int d = FILTER_TAPS[highest_index][3];

		CAT_INANE("CM") << "Replacing default filter " << lowest_index << " with tapped filter " << highest_index << " that is " << ratio << "x more preferable : PRED = (" << a << "A + " << b << "B + " << c << "C + " << d << "D) / 2";

		_filter_replacements.push_back((lowest_index << 16) | highest_index);

		SetSpatialFilter(lowest_index, highest_index);

		// Install grave markers
		bestHist[lowest_index] = 0x7fffffffUL;
		bestHist[highest_index + SF_COUNT] = 0;
	}
}

void ImageCMWriter::decideFilters() {
	EntropyEstimator<u8> ee[3];
	for (int ii = 0; ii < 3; ++ii) {
		ee[ii].clear(256);
	}

	FilterScorer scores;
	scores.init(SF_COUNT * CF_COUNT);

	const int width = _width;

	if (!_knobs->cm_disableEntropy) {
		CAT_INANE("CM") << "Scoring filters using entropy metric " << _knobs->cm_filterSelectFuzz << "... (may take a while)";
	} else {
		CAT_INANE("CM") << "Scoring filters...";
	}

	for (int y = 0; y < _height; y += FILTER_ZONE_SIZE) {
		for (int x = 0; x < width; x += FILTER_ZONE_SIZE) {
			// If this zone is skipped,
			if (getFilter(x, y) == UNUSED_FILTER) {
				continue;
			}

			// Determine best filter combination to use
			int bestSF = 0, bestCF = 0;

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

			if (_knobs->cm_disableEntropy ||
				lowest->score <= _knobs->cm_maxEntropySkip) {
				bestSF = lowest->index % SF_COUNT;
				bestCF = lowest->index / SF_COUNT;

				if (!_knobs->cm_disableEntropy) {
					for (int jj = 0; jj < 3; ++jj) {
						ee[jj].setup();
					}

					// Record this choice
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
							const u8 *pred = SPATIAL_FILTERS[bestSF](p, px, py, width);
							u8 temp[3];
							for (int jj = 0; jj < 3; ++jj) {
								temp[jj] = p[jj] - pred[jj];
							}

							u8 yuv[3];
							RGB2YUV_FILTERS[bestCF](temp, yuv);

							ee[0].push(yuv[0]);
							ee[1].push(yuv[1]);
							ee[2].push(yuv[2]);
						}
					}

					for (int jj = 0; jj < 3; ++jj) {
						ee[jj].save();
						ee[jj].commit();
					}
				}
			} else {
				const int TOP_COUNT = _knobs->cm_filterSelectFuzz;

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

			// Set filter for this zone
			u16 filter = ((u16)bestSF << 8) | bestCF;
			setFilter(x, y, filter);
		}
	}
}

void ImageCMWriter::maskFilters() {
	// For each zone,
	for (int y = 0, height = _height; y < height; y += FILTER_ZONE_SIZE) {
		for (int x = 0, width = _width; x < width; x += FILTER_ZONE_SIZE) {
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

			if (on) {
				setFilter(x, y, UNUSED_FILTER);
			} else {
				setFilter(x, y, TODO_FILTER);
			}
		}
	}
}

bool ImageCMWriter::applyFilters() {
	FreqHistogram<SF_COUNT> sf_hist;
	FreqHistogram<CF_COUNT> cf_hist;

	// For each zone,
	for (int y = 0, height = _height; y < height; y += FILTER_ZONE_SIZE) {
		for (int x = 0, width = _width; x < width; x += FILTER_ZONE_SIZE) {
			// Read filter
			u16 filter = getFilter(x, y);
			if (filter != UNUSED_FILTER) {
				u8 cf = (u8)filter;
				u8 sf = (u8)(filter >> 8);

				sf_hist.add(sf);
				cf_hist.add(cf);
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

	return true;
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
	if (chaos_count >= _knobs->cm_chaosThresh) {
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
	u8 *lastStart = _chaos + COLOR_PLANES;
	CAT_CLR(_chaos, _chaos_size);

	const u8 *CHAOS_TABLE = _chaos_table;

	for (int y = 0; y < _height; ++y) {
		u8 *last = lastStart;

		// For each pixel,
		for (int x = 0; x < width; ++x) {
			// If not masked out,
			if (!_lz->visited(x, y) && !_mask->hasRGB(x, y)) {
				u16 filter = getFilter(x, y);
				// Get filter for this pixel
				u8 cf = (u8)filter;
				u8 sf = (u8)(filter >> 8);

				// Apply spatial filter
				const u8 *pred = SPATIAL_FILTERS[sf](p, x, y, width);
				u8 temp[3];
				for (int jj = 0; jj < 3; ++jj) {
					temp[jj] = p[jj] - pred[jj];
				}

				// Apply color filter
				u8 yuv[COLOR_PLANES];
				RGB2YUV_FILTERS[cf](temp, yuv);
				if (x > 0) {
					yuv[3] = p[-1] - p[3];
				} else {
					yuv[3] = 255 - p[3];
				}

				u8 chaos = CHAOS_TABLE[chaosScore(last[0 - 4]) + chaosScore(last[0])];
				_y_encoder[chaos].add(yuv[0]);
				chaos = CHAOS_TABLE[chaosScore(last[1 - 4]) + chaosScore(last[1])];
				_u_encoder[chaos].add(yuv[1]);
				chaos = CHAOS_TABLE[chaosScore(last[2 - 4]) + chaosScore(last[2])];
				_v_encoder[chaos].add(yuv[2]);
				chaos = CHAOS_TABLE[chaosScore(last[3 - 4]) + chaosScore(last[3])];
				_a_encoder[chaos].add(yuv[3]);

				for (int c = 0; c < COLOR_PLANES; ++c) {
					last[c] = yuv[c];
				}
			} else {
				for (int c = 0; c < COLOR_PLANES; ++c) {
					last[c] = 0;
				}
			}

			// Next pixel
			last += COLOR_PLANES;
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

int ImageCMWriter::initFromRGBA(const u8 *rgba, int width, int height, ImageMaskWriter &mask, ImageLZWriter &lz, const GCIFKnobs *knobs) {
	int err;

	if ((err = init(width, height))) {
		return err;
	}

	if ((!knobs->cm_disableEntropy && knobs->cm_filterSelectFuzz <= 0)) {
		return WE_BAD_PARAMS;
	}

	_knobs = knobs;
	_rgba = rgba;
	_mask = &mask;
	_lz = &lz;

#ifdef TEST_COLOR_FILTERS
	testColorFilters();
	return -1;
#endif

	maskFilters();

	designFilters();

	decideFilters();

	if (!applyFilters()) {
		return WE_BUG;
	}

	chaosStats();

	return WE_OK;
}

bool ImageCMWriter::writeFilters(ImageWriter &writer) {
	const int rep_count = _filter_replacements.size();

	CAT_DEBUG_ENFORCE(SF_COUNT < 32);
	CAT_DEBUG_ENFORCE(TAPPED_COUNT < 128);

	writer.writeBits(rep_count, 5);
	int bits = 5;

	for (int ii = 0; ii < rep_count; ++ii) {
		u32 filter = _filter_replacements[ii];

		u16 def = (u16)(filter >> 16);
		u16 cust = (u16)filter;

		writer.writeBits(def, 5);
		writer.writeBits(cust, 7);
		bits += 12;
	}

	// Write out filter huffman tables
	int sf_table_bits = _sf_encoder.writeTable(writer);
	int cf_table_bits = _cf_encoder.writeTable(writer);

#ifdef CAT_COLLECT_STATS
	Stats.filter_table_bits[0] = sf_table_bits + bits;
	Stats.filter_table_bits[1] = cf_table_bits;
#endif // CAT_COLLECT_STATS

	return true;
}

bool ImageCMWriter::writeChaos(ImageWriter &writer) {
#ifdef CAT_COLLECT_STATS
	int overhead_bits = 0;
	int bitcount[COLOR_PLANES] = {0};
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
	u8 *lastStart = _chaos + COLOR_PLANES;
	CAT_CLR(_chaos, _chaos_size);

	const u8 *CHAOS_TABLE = _chaos_table;

	for (int y = 0; y < _height; ++y) {
		u8 *last = lastStart;

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
				}
			}

			// If not masked out,
			if (!_lz->visited(x, y) && !_mask->hasRGB(x, y)) {
				// Get filter for this pixel
				u16 filter = getFilter(x, y);
				CAT_DEBUG_ENFORCE(filter != UNUSED_FILTER);
				u8 cf = (u8)filter;
				u8 sf = (u8)(filter >> 8);


				// Apply spatial filter
				const u8 *pred = SPATIAL_FILTERS[sf](p, x, y, width);
				u8 temp[3];
				for (int jj = 0; jj < 3; ++jj) {
					temp[jj] = p[jj] - pred[jj];
				}

				// Apply color filter
				u8 YUVA[COLOR_PLANES];
				RGB2YUV_FILTERS[cf](temp, YUVA);
				if (x > 0) {
					YUVA[3] = p[-1] - p[3];
				} else {
					YUVA[3] = 255 - p[3];
				}

				u8 chaos = CHAOS_TABLE[chaosScore(last[0 - 4]) + chaosScore(last[0])];
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
				bits = _a_encoder[chaos].write(YUVA[3], writer);
#ifdef CAT_COLLECT_STATS
				bitcount[3] += bits;
#endif

				for (int c = 0; c < COLOR_PLANES; ++c) {
					last[c] = YUVA[c];
				}
			} else {
				for (int c = 0; c < COLOR_PLANES; ++c) {
					last[c] = 0;
				}
			}

			// Next pixel
			last += COLOR_PLANES;
			p += 4;
		}
	}

#ifdef CAT_COLLECT_STATS
	for (int ii = 0; ii < COLOR_PLANES; ++ii) {
		Stats.rgb_bits[ii] = bitcount[ii];
	}
	Stats.chaos_overhead_bits = overhead_bits;
	Stats.filter_compressed_bits[0] = filter_table_bits[0];
	Stats.filter_compressed_bits[1] = filter_table_bits[1];
#endif

	return true;
}

void ImageCMWriter::write(ImageWriter &writer) {
	CAT_INANE("CM") << "Writing encoded pixel data...";

	writeFilters(writer);

	writeChaos(writer);

#ifdef CAT_COLLECT_STATS
	int total = 0;
	for (int ii = 0; ii < 2; ++ii) {
		total += Stats.filter_table_bits[ii];
		total += Stats.filter_compressed_bits[ii];
	}
	for (int ii = 0; ii < COLOR_PLANES; ++ii) {
		total += Stats.rgb_bits[ii];
	}
	total += Stats.chaos_overhead_bits;
	Stats.chaos_bits = total;
	total += _lz->Stats.huff_bits;
	total += _mask->Stats.compressedDataBits;
	Stats.total_bits = total;

	Stats.overall_compression_ratio = _width * _height * 4 * 8 / (double)Stats.total_bits;

	Stats.chaos_compression_ratio = Stats.chaos_count * COLOR_PLANES * 8 / (double)Stats.chaos_bits;
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

