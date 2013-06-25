/*
	Copyright (c) 2013 Game Closure.  All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	* Redistributions of source code must retain the above copyright notice,
	  this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	* Neither the name of GCIF nor the names of its contributors may be used
	  to endorse or promote products derived from this software without
	  specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#include "ImageRGBAWriter.hpp"
#include "../decoder/BitMath.hpp"
#include "../decoder/Filters.hpp"
#include "EntropyEstimator.hpp"
#include "Log.hpp"
#include "ImageLZWriter.hpp"
using namespace cat;

#include <vector>
using namespace std;

#include "../decoder/lz4.h"
#include "lz4hc.h"
#include "Log.hpp"
#include "HuffmanEncoder.hpp"
#include "lodepng.h"

#ifdef CAT_DESYNCH_CHECKS
#define DESYNC_TABLE() writer.writeWord(1234567);
#define DESYNC(x, y) writer.writeBits(x ^ 12345, 16); writer.writeBits(y ^ 54321, 16);
#else
#define DESYNC_TABLE()
#define DESYNC(x, y)
#endif

//#define CAT_DUMP_RESIDUALS


//// ImageRGBAWriter

void ImageRGBAWriter::maskTiles() {
	// SF tiles are filled with zeroes for masked tiles
	_sf_tiles.fill_00();

	const u16 tile_size_x = _tile_size_x, tile_size_y = _tile_size_y;
	const u16 size_x = _size_x, size_y = _size_y;
	u8 *cf = _cf_tiles.get();

	// For each tile,
	for (u16 y = 0; y < size_y; y += tile_size_y) {
		for (u16 x = 0; x < size_x; x += tile_size_x, ++cf) {

			// For each element in the tile,
			u16 py = y, cy = tile_size_y;
			while (cy-- > 0 && py < size_y) {
				u16 px = x, cx = tile_size_x;
				while (cx-- > 0 && px < size_x) {
					// If it is not masked,
					if (!IsMasked(px, py)) {
						// We need to do this tile
						*cf = TODO_TILE;
						goto next_tile;
					}
					++px;
				}
				++py;
			}

			// Tile is masked out entirely
			*cf = MASK_TILE;
next_tile:;
		}
	}
}

void ImageRGBAWriter::designFilters() {
	FilterScorer scores, awards;
	scores.init(SF_COUNT);
	awards.init(SF_COUNT);
	awards.reset();
	u8 FPT[3];
	u32 total_score = 0;

	CAT_INANE("RGBA") << "Designing spatial filters...";

	const u16 tile_size_x = _tile_size_x, tile_size_y = _tile_size_y;
	const u16 size_x = _size_x, size_y = _size_y;

	u8 *sf = _sf_tiles.get();
	u8 *cf = _cf_tiles.get();
	const u8 *topleft_row = _rgba;

	for (int y = 0; y < _size_y; y += _tile_size_y) {
		const u8 *topleft = topleft_row;

		for (int x = 0; x < _size_x; x += _tile_size_x, ++sf, ++cf, topleft += _tile_size_x * 4) {
			if (*cf == MASK_TILE) {
				continue;
			}

			scores.reset();

			// For each element in the tile,
			const u8 *row = topleft;
			u16 py = y, cy = tile_size_y;
			while (cy-- > 0 && py < size_y) {
				const u8 *data = row;
				u16 px = x, cx = tile_size_x;
				while (cx-- > 0 && px < size_x) {
					// If element is not masked,
					if (!IsMasked(px, py)) {
						const u8 r = data[0];
						const u8 g = data[1];
						const u8 b = data[2];

						for (int f = 0; f < SF_COUNT; ++f) {
							const u8 *pred = RGBA_FILTERS[f].safe(data, FPT, px, py, size_x);

							int score = RGBChaos::ResidualScore(r - pred[0]);
							score += RGBChaos::ResidualScore(g - pred[1]);
							score += RGBChaos::ResidualScore(b - pred[2]);

							scores.add(f, score);
						}
					}
					++px;
					data += 4;
				}
				++py;
				row += size_x * 4;
			}

			FilterScorer::Score *top = scores.getLow(4, true);
			total_score += 5 + 3 + 1 + 1;
			awards.add(top[0].index, 5);
			awards.add(top[1].index, 3);
			awards.add(top[2].index, 1);
			awards.add(top[3].index, 1);
		}

		topleft_row += _size_x * 4 * _tile_size_y;
	}

	// Sort the best awards
	int count = MAX_FILTERS;
	FilterScorer::Score *top = awards.getHigh(count, true);

	// Initialize coverage
	int sf_count = 0;

	// Design remaining filter functions
	u32 coverage = 0;
	while (count-- > 0) {
		int index = top->index;
		int score = top->score;
		++top;

		coverage += score;

		_sf_indices[sf_count] = index;
		_sf[sf_count] = RGBA_FILTERS[index];
		++sf_count;

#ifdef CAT_DUMP_FILTERS
		CAT_INANE("RGBA") << " - Added filter " << index << " with score " << score;
#endif

		float coverage_ratio = coverage / (float)total_score;

		// If coverage is sufficient,
		if (coverage_ratio >= 0.8) {
			if (score / (float)total_score < 0.05) {
				break;
			}
		}
	}

	_sf_count = sf_count;
}

void ImageRGBAWriter::designTiles() {
	CAT_INANE("RGBA") << "Designing SF/CF tiles for " << _tiles_x << "x" << _tiles_y << "...";

	const u16 tile_size_x = _tile_size_x, tile_size_y = _tile_size_y;
	const u16 size_x = _size_x, size_y = _size_y;
	u8 FPT[3];

	EntropyEstimator ee[3];
	ee[0].init();
	ee[1].init();
	ee[2].init();

	// Allocate temporary space for entropy analysis
	const u32 code_stride = _tile_size_x * _tile_size_y;
	const u32 codes_size = code_stride * _sf_count * CF_COUNT;
	_ecodes[0].resize(codes_size);
	_ecodes[1].resize(codes_size);
	_ecodes[2].resize(codes_size);
	u8 *codes[3] = {
		_ecodes[0].get(),
		_ecodes[1].get(),
		_ecodes[2].get()
	};

	// Until revisits are done,
	int passes = 0;
	int revisitCount = _knobs->cm_revisitCount;
	while (passes < MAX_PASSES) {
		const u8 *topleft_row = _rgba;
		int ty = 0;
		u8 *sf = _sf_tiles.get();
		u8 *cf = _cf_tiles.get();

		// For each tile,
		for (u16 y = 0; y < size_y; y += tile_size_y, ++ty) {
			const u8 *topleft = topleft_row;
			int tx = 0;

			for (u16 x = 0; x < size_x; x += tile_size_x, ++sf, ++cf, topleft += tile_size_x * 4, ++tx) {
				u8 ocf = *cf;

				// If tile is masked,
				if (ocf == MASK_TILE) {
					continue;
				}

				u8 osf = *sf;

				// If we are on the second or later pass,
				if (passes > 0) {
					// If just finished revisiting old zones,
					if (--revisitCount < 0) {
						// Done!
						return;
					}

					int code_count = 0;

					// For each element in the tile,
					const u8 *row = topleft;
					u16 py = y, cy = tile_size_y;
					while (cy-- > 0 && py < size_y) {
						const u8 *data = row;
						u16 px = x, cx = tile_size_x;
						while (cx-- > 0 && px < size_x) {
							// If element is not masked,
							if (!IsMasked(px, py)) {
								const u8 *pred = _sf[osf].safe(data, FPT, px, py, size_x);
								u8 residual_rgb[3] = {
									data[0] - pred[0],
									data[1] - pred[1],
									data[2] - pred[2]
								};

								u8 yuv[3];
								RGB2YUV_FILTERS[ocf](residual_rgb, yuv);

								codes[0][code_count] = yuv[0];
								codes[1][code_count] = yuv[1];
								codes[2][code_count] = yuv[2];
								++code_count;
							}
							++px;
							data += 4;
						}
						++py;
						row += size_x * 4;
					}

					ee[0].subtract(codes[0], code_count);
					ee[1].subtract(codes[1], code_count);
					ee[2].subtract(codes[2], code_count);
				}

				int code_count = 0;

				// For each element in the tile,
				const u8 *row = topleft;
				u16 py = y, cy = tile_size_y;
				while (cy-- > 0 && py < size_y) {
					const u8 *data = row;
					u16 px = x, cx = tile_size_x;
					while (cx-- > 0 && px < size_x) {
						// If element is not masked,
						if (!IsMasked(px, py)) {
							u8 *dest_y = codes[0] + code_count;
							u8 *dest_u = codes[1] + code_count;
							u8 *dest_v = codes[2] + code_count;

							// For each spatial filter,
							for (int sfi = 0, sfi_end = _sf_count; sfi < sfi_end; ++sfi) {
								const u8 *pred = _sf[sfi].safe(data, FPT, px, py, size_x);
								u8 residual_rgb[3] = {
									data[0] - pred[0],
									data[1] - pred[1],
									data[2] - pred[2]
								};

								// For each color filter,
								for (int cfi = 0; cfi < CF_COUNT; ++cfi) {
									u8 yuv[3];
									RGB2YUV_FILTERS[cfi](residual_rgb, yuv);

									*dest_y = yuv[0];
									*dest_u = yuv[1];
									*dest_v = yuv[2];
									dest_y += code_stride;
									dest_u += code_stride;
									dest_v += code_stride;
								}
							}

							++code_count;
						}
						++px;
						data += 4;
					}
					++py;
					row += size_x * 4;
				}

				// Evaluate entropy of codes
				u8 *src_y = codes[0];
				u8 *src_u = codes[1];
				u8 *src_v = codes[2];
				int lowest_entropy = 0x7fffffff;
				int best_sf = 0, best_cf = 0;
				u8 *src_best_y = src_y;
				u8 *src_best_u = src_u;
				u8 *src_best_v = src_v;

				for (int sfi = 0, sfi_end = _sf_count; sfi < sfi_end; ++sfi) {
					for (int cfi = 0; cfi < CF_COUNT; ++cfi) {
						int entropy = ee[0].entropy(src_y, code_count);
						entropy += ee[1].entropy(src_u, code_count);
						entropy += ee[2].entropy(src_v, code_count);

						if (lowest_entropy > entropy) {
							lowest_entropy = entropy;
							best_sf = sfi;
							best_cf = cfi;
							src_best_y = src_y;
							src_best_u = src_u;
							src_best_v = src_v;
						}

						src_y += code_stride;
						src_u += code_stride;
						src_v += code_stride;
					}
				}

				// Update entropy histogram
				ee[0].add(src_best_y, code_count);
				ee[1].add(src_best_u, code_count);
				ee[2].add(src_best_v, code_count);

				*sf = best_sf;
				*cf = best_cf;
			}

			topleft_row += _size_x * 4 * _tile_size_y;
		}

		++passes;

		CAT_INANE("RGBA") << "Revisiting filter selections from the top... " << revisitCount << " left";
	}
}

void ImageRGBAWriter::sortFilters() {
	CAT_INANE("RGBA") << "Sorting spatial filters...";

	_optimizer.process(_sf_tiles.get(), _tiles_x, _tiles_y, _sf_count,
		PaletteOptimizer::MaskDelegate::FromMember<ImageRGBAWriter, &ImageRGBAWriter::IsSFMasked>(this));

	// Overwrite original tiles with optimized tiles
	const u8 *src = _optimizer.getOptimizedImage();
	memcpy(_sf_tiles.get(), src, _tiles_x * _tiles_y);

	// Update filter indices
	u16 filter_indices[MAX_FILTERS];
	for (int ii = 0, iiend = _sf_count; ii < iiend; ++ii) {
		filter_indices[_optimizer.forward(ii)] = _sf_indices[ii];
	}
	memcpy(_sf_indices, filter_indices, sizeof(_sf_indices));

	// Update filter functions
	for (int ii = 0, iiend = _sf_count; ii < iiend; ++ii) {
		_sf[ii] = RGBA_FILTERS[_sf_indices[ii]];
	}
}

bool ImageRGBAWriter::compressAlpha() {
	CAT_INANE("RGBA") << "Compressing alpha channel...";

	// Generate alpha matrix
	const int alpha_size = _size_x * _size_y;
	_alpha.resize(alpha_size);

	u8 *a = _alpha.get();
	const u8 *rgba = _rgba;
	for (int y = 0; y < _size_y; ++y) {
		for (int x = 0; x < _size_x; ++x) {
			*a++ = ~rgba[3]; // Same as 255 - a: Good default filter
			rgba += 4;
		}
	}

	MonoWriter::Parameters params;
	params.knobs = _knobs;
	params.data = _alpha.get();
	params.num_syms = 256;
	params.size_x = _size_x;
	params.size_y = _size_y;
	params.max_filters = 32;
	params.min_bits = 2;
	params.max_bits = 5;
	params.sympal_thresh = 0.1;
	params.filter_cover_thresh = 0.6;
	params.filter_inc_thresh = 0.05;
	params.mask.SetMember<ImageRGBAWriter, &ImageRGBAWriter::IsMasked>(this);
	params.AWARDS[0] = 5;
	params.AWARDS[1] = 3;
	params.AWARDS[2] = 1;
	params.AWARDS[3] = 1;
	params.award_count = 4;
	params.write_order = 0;

	_a_encoder.init(params);

	return true;
}

void ImageRGBAWriter::computeResiduals() {
	CAT_INANE("RGBA") << "Executing tiles to generate residual matrix...";

	const u16 tile_size_x = _tile_size_x, tile_size_y = _tile_size_y;
	const u16 size_x = _size_x, size_y = _size_y;
	u8 FPT[3];

	const u8 *sf = _sf_tiles.get();
	const u8 *cf = _cf_tiles.get();

	_residuals.resize(_size_x * _size_y * 4);

	// For each tile,
	const u8 *topleft_row = _rgba;
	size_t residual_delta = (size_t)(_residuals.get() - topleft_row);
	for (u16 y = 0; y < size_y; y += tile_size_y) {
		const u8 *topleft = topleft_row;

		for (u16 x = 0; x < size_x; x += tile_size_x, ++sf, ++cf, topleft += tile_size_x*4) {
			const u8 cfi = *cf;

			if (cfi == MASK_TILE) {
				continue;
			}

			const u8 sfi = *sf;

			// For each element in the tile,
			const u8 *row = topleft;
			u16 py = y, cy = tile_size_y;
			while (cy-- > 0 && py < size_y) {
				const u8 *data = row;
				u16 px = x, cx = tile_size_x;
				while (cx-- > 0 && px < size_x) {
					// If element is not masked,
					if (!IsMasked(px, py)) {
						const u8 *pred = _sf[sfi].safe(data, FPT, px, py, size_x);
						u8 residual_rgb[3] = {
							data[0] - pred[0],
							data[1] - pred[1],
							data[2] - pred[2]
						};

						u8 yuv[3];
						RGB2YUV_FILTERS[cfi](residual_rgb, yuv);

						u8 *residual_data = (u8*)data + residual_delta;

						residual_data[0] = yuv[0];
						residual_data[1] = yuv[1];
						residual_data[2] = yuv[2];
					}
					++px;
					data += 4;
				}
				++py;
				row += size_x*4;
			}
		}

		topleft_row += _size_x * 4 * _tile_size_y;
	}

#ifdef CAT_DUMP_RESIDUALS
	SmartArray<u8> a, b, c;
	a.resize(_size_x * _size_y);
	b.resize(_size_x * _size_y);
	c.resize(_size_x * _size_y);

	for (u16 y = 0; y < size_y; ++y) {
		for (u16 x = 0; x < size_x; ++x) {
			int off = x + y * size_x;

			a[off] = _residuals.get()[off * 4 + 0];
			b[off] = _residuals.get()[off * 4 + 1];
			c[off] = _residuals.get()[off * 4 + 2];
		}
	}

	lodepng_encode_file("R.png", a.get(), size_x, size_y, LCT_GREY, 8);
	lodepng_encode_file("G.png", b.get(), size_x, size_y, LCT_GREY, 8);
	lodepng_encode_file("B.png", c.get(), size_x, size_y, LCT_GREY, 8);

	const int size = size_x * size_y;
	SmartArray<u8> la, lb, lc;
	la.resize(LZ4_compressBound(size));
	lb.resize(LZ4_compressBound(size));
	lc.resize(LZ4_compressBound(size));

	int las = LZ4_compressHC((char*)a.get(), (char*)la.get(), size);
	int lbs = LZ4_compressHC((char*)b.get(), (char*)lb.get(), size);
	int lcs = LZ4_compressHC((char*)c.get(), (char*)lc.get(), size);

	CAT_WARN("TEST") << "R LZ = " << las << " bytes";
	CAT_WARN("TEST") << "G LZ = " << lbs << " bytes";
	CAT_WARN("TEST") << "B LZ = " << lcs << " bytes";

	FreqHistogram<256> ha;
	FreqHistogram<256> hb;
	FreqHistogram<256> hc;

	ha.init();
	hb.init();
	hc.init();

	for (int ii = 0; ii < las; ++ii) {
		ha.add(la[ii]);
	}

	for (int ii = 0; ii < lbs; ++ii) {
		hb.add(lb[ii]);
	}

	for (int ii = 0; ii < lcs; ++ii) {
		hc.add(lc[ii]);
	}

	HuffmanEncoder<256> ea;
	HuffmanEncoder<256> eb;
	HuffmanEncoder<256> ec;

	ea.init(ha);
	eb.init(hb);
	ec.init(hc);

	int bas = 0, bbs = 0, bcs = 0;
	for (int ii = 0; ii < las; ++ii) {
		bas += ea.simulateWrite(la[ii]);
	}
	for (int ii = 0; ii < lbs; ++ii) {
		bbs += eb.simulateWrite(lb[ii]);
	}
	for (int ii = 0; ii < lcs; ++ii) {
		bcs += ec.simulateWrite(lc[ii]);
	}

	CAT_WARN("TEST") << "R = " << bas/8 << " by";
	CAT_WARN("TEST") << "G = " << bbs/8 << " by";
	CAT_WARN("TEST") << "B = " << bcs/8 << " by";
#endif
}

void ImageRGBAWriter::designChaos() {
	CAT_INANE("RGBA") << "Designing chaos...";

	u32 best_entropy = 0x7fffffff;

	Encoders *best = 0;
	Encoders *encoders = new Encoders;

	// For each chaos level,
	for (int chaos_levels = 1; chaos_levels < MAX_CHAOS_LEVELS; ++chaos_levels) {
		encoders->chaos.init(chaos_levels, _size_x);
		encoders->chaos.start();

		// For each chaos level,
		for (int ii = 0; ii < chaos_levels; ++ii) {
			encoders->y[ii].init();
			encoders->u[ii].init();
			encoders->v[ii].init();
		}

		// For each row,
		const u8 *residuals = _residuals.get();
		for (int y = 0; y < _size_y; ++y) {
			// For each column,
			for (int x = 0; x < _size_x; ++x) {
				// If masked,
				if (IsMasked(x, y)) {
					encoders->chaos.zero(x);
				} else {
					// Get chaos bin
					u8 cy, cu, cv;
					encoders->chaos.get(x, cy, cu, cv);

					// Update chaos
					encoders->chaos.store(x, residuals);

					// Add to histogram for this chaos bin
					encoders->y[cy].add(residuals[0]);
					encoders->u[cu].add(residuals[1]);
					encoders->v[cv].add(residuals[2]);
				}

				residuals += 4;
			}
		}

		// For each chaos level,
		u32 entropy = 0;
		for (int ii = 0; ii < chaos_levels; ++ii) {
			encoders->y[ii].finalize();
			entropy += encoders->y[ii].simulateAll();
			encoders->u[ii].finalize();
			entropy += encoders->u[ii].simulateAll();
			encoders->v[ii].finalize();
			entropy += encoders->v[ii].simulateAll();
		}

		// If this is the best chaos levels so far,
		if (best_entropy > entropy + 128) {
			best_entropy = entropy;
			Encoders *temp = best;
			best = encoders;
			if (temp) {
				encoders = temp;
			} else {
				encoders = new Encoders;
			}
		}

		// If we have not found a better one in 4 moves,
		if (chaos_levels - best->chaos.getBinCount() >= 2) {
			// Stop early to save time
			break;
		}
	}

	// Record the best option found
	_encoders = best;

	if (encoders) {
		delete encoders;
	}
}

void ImageRGBAWriter::generateWriteOrder() {
	CAT_DEBUG_ENFORCE(_tile_bits_x == _tile_bits_y);

	MonoWriter::generateWriteOrder(_size_x, _size_y,
		MonoWriter::MaskDelegate::FromMember<ImageRGBAWriter, &ImageRGBAWriter::IsMasked>(this),
		_tile_bits_x, _filter_order);
}

bool ImageRGBAWriter::compressSF() {
	MonoWriter::Parameters params;
	params.knobs = _knobs;
	params.data = _sf_tiles.get();
	params.num_syms = _sf_count;
	params.size_x = _tiles_x;
	params.size_y = _tiles_y;
	params.max_filters = 32;
	params.min_bits = 2;
	params.max_bits = 5;
	params.sympal_thresh = 0.1;
	params.filter_cover_thresh = 0.6;
	params.filter_inc_thresh = 0.05;
	params.mask.SetMember<ImageRGBAWriter, &ImageRGBAWriter::IsSFMasked>(this);
	params.AWARDS[0] = 5;
	params.AWARDS[1] = 3;
	params.AWARDS[2] = 1;
	params.AWARDS[3] = 1;
	params.award_count = 4;
	params.write_order = &_filter_order[0];

	CAT_INANE("RGBA") << "Compressing spatial filter matrix...";

	_sf_encoder.init(params);

	return true;
}

bool ImageRGBAWriter::compressCF() {
	MonoWriter::Parameters params;
	params.knobs = _knobs;
	params.data = _cf_tiles.get();
	params.num_syms = CF_COUNT;
	params.size_x = _tiles_x;
	params.size_y = _tiles_y;
	params.max_filters = 32;
	params.min_bits = 2;
	params.max_bits = 5;
	params.sympal_thresh = 0.1;
	params.filter_cover_thresh = 0.6;
	params.filter_inc_thresh = 0.05;
	params.mask.SetMember<ImageRGBAWriter, &ImageRGBAWriter::IsSFMasked>(this);
	params.AWARDS[0] = 5;
	params.AWARDS[1] = 3;
	params.AWARDS[2] = 1;
	params.AWARDS[3] = 1;
	params.award_count = 4;
	params.write_order = &_filter_order[0];

	CAT_INANE("RGBA") << "Compressing color filter matrix...";

	_cf_encoder.init(params);

	return true;
}

bool ImageRGBAWriter::IsMasked(u16 x, u16 y) {
	CAT_DEBUG_ENFORCE(x < _size_x && y < _size_y);

	return _mask->masked(x, y) || _lz->visited(x, y);
}

bool ImageRGBAWriter::IsSFMasked(u16 x, u16 y) {
	CAT_DEBUG_ENFORCE(x < _tiles_x && y < _tiles_y);

	return _cf_tiles[x + _tiles_x * y] == MASK_TILE;
}

int ImageRGBAWriter::init(const u8 *rgba, int size_x, int size_y, ImageMaskWriter &mask, ImageLZWriter &lz, const GCIFKnobs *knobs) {
	_knobs = knobs;
	_rgba = rgba;
	_mask = &mask;
	_lz = &lz;

	if (size_x < 0 || size_y < 0) {
		return GCIF_WE_BAD_DIMS;
	}

	if ((!knobs->cm_disableEntropy && knobs->cm_filterSelectFuzz <= 0)) {
		return GCIF_WE_BAD_PARAMS;
	}

	_size_x = size_x;
	_size_y = size_y;

	// Use constant tile size of 4x4 for now
	_tile_bits_x = 2;
	_tile_bits_y = 2;
	_tile_size_x = 1 << _tile_bits_x;
	_tile_size_y = 1 << _tile_bits_y;
	_tiles_x = (_size_x + _tile_size_x - 1) >> _tile_bits_x;
	_tiles_y = (_size_y + _tile_size_y - 1) >> _tile_bits_y;

	const int tiles_size = _tiles_x * _tiles_y;
	_sf_tiles.resize(tiles_size);
	_cf_tiles.resize(tiles_size);

	maskTiles();
	designFilters();
	designTiles();
	sortFilters();
	computeResiduals();
	compressAlpha();
	designChaos();
	generateWriteOrder();
	compressSF();
	compressCF();

	return GCIF_WE_OK;
}

int ImageRGBAWriter::writeTables(ImageWriter &writer) {
	CAT_INANE("RGBA") << "Writing tables...";

	CAT_DEBUG_ENFORCE(MAX_FILTERS <= 32);
	CAT_DEBUG_ENFORCE(SF_COUNT <= 128);
	CAT_DEBUG_ENFORCE(_tile_bits_x <= 8);

	writer.writeBits(_tile_bits_x - 1, 3);
	int basic_bits = 3;

	DESYNC_TABLE();

	CAT_DEBUG_ENFORCE(_sf_count > 0);

	// Write filter choices
	writer.writeBits(_sf_count - 1, 5);
	int choice_bits = 5;

	for (int ii = 0; ii < _sf_count; ++ii) {
		u16 sf = _sf_indices[ii];

		writer.writeBits(sf, 7);
		choice_bits += 7;
	}

	DESYNC_TABLE();

	int sf_table_bits = _sf_encoder.writeTables(writer);

	DESYNC_TABLE();

	int cf_table_bits = _cf_encoder.writeTables(writer);

	DESYNC_TABLE();

	int a_table_bits = _a_encoder.writeTables(writer);

	DESYNC_TABLE();

#ifdef CAT_COLLECT_STATS
	Stats.y_table_bits = 0;
	Stats.u_table_bits = 0;
	Stats.v_table_bits = 0;
#endif // CAT_COLLECT_STATS

	writer.writeBits(_encoders->chaos.getBinCount() - 1, 4);
	basic_bits += 4;

	for (int jj = 0; jj < _encoders->chaos.getBinCount(); ++jj) {
		int y_table_bits = _encoders->y[jj].writeTables(writer);
		DESYNC_TABLE();
		int u_table_bits = _encoders->u[jj].writeTables(writer);
		DESYNC_TABLE();
		int v_table_bits = _encoders->v[jj].writeTables(writer);
		DESYNC_TABLE();

#ifdef CAT_COLLECT_STATS
		Stats.y_table_bits += y_table_bits;
		Stats.u_table_bits += u_table_bits;
		Stats.v_table_bits += v_table_bits;
#endif // CAT_COLLECT_STATS
	}

#ifdef CAT_COLLECT_STATS
	Stats.basic_overhead_bits = basic_bits;
	Stats.sf_choice_bits = choice_bits;
	Stats.sf_table_bits = sf_table_bits;
	Stats.cf_table_bits = cf_table_bits;
	Stats.a_table_bits = a_table_bits;
#endif // CAT_COLLECT_STATS

	return GCIF_WE_OK;
}

bool ImageRGBAWriter::writePixels(ImageWriter &writer) {
	CAT_INANE("RGBA") << "Writing interleaved pixel/filter data...";

#ifdef CAT_COLLECT_STATS
	int sf_bits = 0, cf_bits = 0, y_bits = 0, u_bits = 0, v_bits = 0, a_bits = 0, rgba_count = 0;
#endif

	_seen_filter.resize(_tiles_x);

	_encoders->chaos.start();
	for (int ii = 0, iiend = _encoders->chaos.getBinCount(); ii < iiend; ++ii) {
		_encoders->y[ii].reset();
		_encoders->u[ii].reset();
		_encoders->v[ii].reset();
	}

	const u8 *residuals = _residuals.get();
	const u16 tile_mask_y = _tile_size_y - 1;

	// For each scanline,
	for (u16 y = 0; y < _size_y; ++y) {
		const u16 ty = y >> _tile_bits_y;

		// If at the start of a tile row,
		if ((y & tile_mask_y) == 0) {
			// After the first row,
			if (y > 0) {
				for (u16 tx = 0; tx < _tiles_x; ++tx) {
					if (_seen_filter[tx] == 0) {
						CAT_DEBUG_ENFORCE(IsSFMasked(tx, ty - 1));
						_sf_encoder.zero(tx);
						_cf_encoder.zero(tx);
					}
				}
			}

			_seen_filter.fill_00();

			sf_bits += _sf_encoder.writeRowHeader(ty, writer);
			cf_bits += _cf_encoder.writeRowHeader(ty, writer);
		}

		_a_encoder.writeRowHeader(y, writer);

		// For each pixel,
		for (u16 x = 0, size_x = _size_x; x < size_x; ++x) {
			DESYNC(x, y);

			// If masked,
			if (IsMasked(x, y)) {
				_encoders->chaos.zero(x);
				_a_encoder.zero(x);
			} else {
				// If filter needs to be written,
				u16 tx = x >> _tile_bits_x;
				if (_seen_filter[tx] == 0) {
					_seen_filter[tx] = 1;

					CAT_DEBUG_ENFORCE(!IsSFMasked(tx, ty));

					cf_bits += _cf_encoder.write(tx, ty, writer);
					sf_bits += _sf_encoder.write(tx, ty, writer);
				}

				// Get chaos bin
				u8 cy, cu, cv;
				_encoders->chaos.get(x, cy, cu, cv);

				// Update chaos
				_encoders->chaos.store(x, residuals);

				// Write pixel
				y_bits += _encoders->y[cy].write(residuals[0], writer);
				u_bits += _encoders->u[cu].write(residuals[1], writer);
				v_bits += _encoders->v[cv].write(residuals[2], writer);
				a_bits += _a_encoder.write(x, y, writer);

#ifdef CAT_COLLECT_STATS
				// Increment RGBA pixel count
				++rgba_count;
#endif
			}

			residuals += 4;
		}
	}

#ifdef CAT_COLLECT_STATS
	Stats.rgba_count = rgba_count;
	Stats.sf_bits = sf_bits;
	Stats.cf_bits = cf_bits;
	Stats.y_bits = y_bits;
	Stats.u_bits = u_bits;
	Stats.v_bits = v_bits;
	Stats.a_bits = a_bits;
#endif

	return true;
}

void ImageRGBAWriter::write(ImageWriter &writer) {
	writeTables(writer);

	writePixels(writer);

#ifdef CAT_COLLECT_STATS
	Stats.chaos_bins = _encoders->chaos.getBinCount();

	int rgba_total = 0;
	rgba_total += Stats.basic_overhead_bits;
	rgba_total += Stats.sf_choice_bits;
	rgba_total += Stats.cf_table_bits;
	rgba_total += Stats.y_table_bits;
	rgba_total += Stats.u_table_bits;
	rgba_total += Stats.v_table_bits;
	rgba_total += Stats.a_table_bits;
	rgba_total += Stats.sf_bits;
	rgba_total += Stats.cf_bits;
	rgba_total += Stats.y_bits;
	rgba_total += Stats.u_bits;
	rgba_total += Stats.v_bits;
	rgba_total += Stats.a_bits;
	Stats.rgba_bits = rgba_total;

	int total = rgba_total;
	total += _lz->Stats.huff_bits;
	total += _mask->Stats.compressedDataBits;
	Stats.total_bits = total;

	Stats.rgba_compression_ratio = Stats.rgba_count * 32 / (double)Stats.rgba_bits;
	Stats.overall_compression_ratio = _size_x * _size_y * 32 / (double)Stats.total_bits;
#endif
}

#ifdef CAT_COLLECT_STATS

bool ImageRGBAWriter::dumpStats() {
	CAT_INANE("stats") << "(RGBA Compress) Alpha channel encoder:";
	_a_encoder.dumpStats();
	CAT_INANE("stats") << "(RGBA Compress) Spatial filter encoder:";
	_sf_encoder.dumpStats();
	CAT_INANE("stats") << "(RGBA Compress) Color filter encoder:";
	_cf_encoder.dumpStats();

	CAT_INANE("stats") << "(RGBA Compress)     Basic Overhead : " <<  Stats.basic_overhead_bits << " bits (" << Stats.basic_overhead_bits/8 << " bytes, " << Stats.basic_overhead_bits * 100.f / Stats.rgba_bits << "% of RGBA) with " << _encoders->chaos.getBinCount() << " chaos bins";
	CAT_INANE("stats") << "(RGBA Compress) SF Choice Overhead : " << Stats.sf_choice_bits << " bits (" << Stats.sf_choice_bits/8 << " bytes, " << Stats.sf_choice_bits * 100.f / Stats.rgba_bits << "% of RGBA)";
	CAT_INANE("stats") << "(RGBA Compress)  SF Table Overhead : " << Stats.sf_table_bits << " bits (" << Stats.sf_table_bits/8 << " bytes, " << Stats.sf_table_bits * 100.f / Stats.rgba_bits << "% of RGBA)";
	CAT_INANE("stats") << "(RGBA Compress)  CF Table Overhead : " << Stats.cf_table_bits << " bits (" << Stats.cf_table_bits/8 << " bytes, " << Stats.cf_table_bits * 100.f / Stats.rgba_bits << "% of RGBA)";
	CAT_INANE("stats") << "(RGBA Compress)   Y Table Overhead : " << Stats.y_table_bits << " bits (" << Stats.y_table_bits/8 << " bytes, " << Stats.y_table_bits * 100.f / Stats.rgba_bits << "% of RGBA)";
	CAT_INANE("stats") << "(RGBA Compress)   U Table Overhead : " << Stats.u_table_bits << " bits (" << Stats.u_table_bits/8 << " bytes, " << Stats.u_table_bits * 100.f / Stats.rgba_bits << "% of RGBA)";
	CAT_INANE("stats") << "(RGBA Compress)   V Table Overhead : " << Stats.v_table_bits << " bits (" << Stats.v_table_bits/8 << " bytes, " << Stats.v_table_bits * 100.f / Stats.rgba_bits << "% of RGBA)";
	CAT_INANE("stats") << "(RGBA Compress)   A Table Overhead : " << Stats.a_table_bits << " bits (" << Stats.a_table_bits/8 << " bytes, " << Stats.a_table_bits * 100.f / Stats.rgba_bits << "% of RGBA)";
	CAT_INANE("stats") << "(RGBA Compress)      SF Compressed : " << Stats.sf_bits << " bits (" << Stats.sf_bits/8 << " bytes, " << Stats.sf_bits * 100.f / Stats.rgba_bits << "% of RGBA)";
	CAT_INANE("stats") << "(RGBA Compress)      CF Compressed : " << Stats.cf_bits << " bits (" << Stats.cf_bits/8 << " bytes, " << Stats.cf_bits * 100.f / Stats.rgba_bits << "% of RGBA)";
	CAT_INANE("stats") << "(RGBA Compress)       Y Compressed : " << Stats.y_bits << " bits (" << Stats.y_bits/8 << " bytes, " << Stats.y_bits * 100.f / Stats.rgba_bits << "% of RGBA)";
	CAT_INANE("stats") << "(RGBA Compress)       U Compressed : " << Stats.u_bits << " bits (" << Stats.u_bits/8 << " bytes, " << Stats.u_bits * 100.f / Stats.rgba_bits << "% of RGBA)";
	CAT_INANE("stats") << "(RGBA Compress)       V Compressed : " << Stats.v_bits << " bits (" << Stats.v_bits/8 << " bytes, " << Stats.v_bits * 100.f / Stats.rgba_bits << "% of RGBA)";
	CAT_INANE("stats") << "(RGBA Compress)       A Compressed : " << Stats.a_bits << " bits (" << Stats.a_bits/8 << " bytes, " << Stats.a_bits * 100.f / Stats.rgba_bits << "% of RGBA)";
	CAT_INANE("stats") << "(RGBA Compress)  Overall RGBA Data : " << Stats.rgba_bits << " bits (" << Stats.rgba_bits/8 << " bytes, " << Stats.rgba_bits * 100.f / Stats.total_bits << "% of total)";
	CAT_INANE("stats") << "(RGBA Compress)   RGBA write count : " << Stats.rgba_count << " pixels for " << _size_x << "x" << _size_y << " pixel image (" << Stats.rgba_count * 100.f / (_size_x * _size_y) << " % of total)";
	CAT_INANE("stats") << "(RGBA Compress)    RGBA Compression Ratio : " << Stats.rgba_compression_ratio << ":1 compression ratio";
	CAT_INANE("stats") << "(RGBA Compress)              Overall Size : " << Stats.total_bits << " bits (" << Stats.total_bits/8 << " bytes)";
	CAT_INANE("stats") << "(RGBA Compress) Overall Compression Ratio : " << Stats.overall_compression_ratio << ":1";

	return true;
}

#endif

