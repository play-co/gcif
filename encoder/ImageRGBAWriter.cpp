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
#include "FilterScorer.hpp"

#include "../decoder/lz4.h"
#include "lz4hc.h"
#include "Log.hpp"
#include "HuffmanEncoder.hpp"
#include "lodepng.h"

using namespace cat;
using namespace std;

#ifdef CAT_DESYNCH_CHECKS
#define DESYNC_TABLE() writer.writeWord(1234567);
#define DESYNC(x, y) writer.writeBits(x ^ 12345, 16); writer.writeBits(y ^ 54321, 16);
#else
#define DESYNC_TABLE()
#define DESYNC(x, y)
#endif

//#define CAT_DUMP_RESIDUALS


//// ImageRGBAWriter

void ImageRGBAWriter::priceResiduals() {
	CAT_INANE("RGBA") << "Assigning approximate bit costs to residuals...";

	_encoders->chaos.start();

	for (int ii = 0, iiend = _encoders->chaos.getBinCount(); ii < iiend; ++ii) {
		_encoders->y[ii].reset();
		_encoders->u[ii].reset();
		_encoders->v[ii].reset();
	}

	// For each pixel of residuals,
	u8 *residuals = _residuals.get();
	for (u16 y = 0; y < _ysize; ++y) {
		for (u16 x = 0; x < _xsize; ++x, residuals += 4) {
			if (_mask->masked(x, y)) {
				_encoders->chaos.zero(x);
				residuals[3] = 0;
			} else {
				// Get chaos bin
				u8 cy, cu, cv;
				_encoders->chaos.get(x, cy, cu, cv);

				// Update chaos
				_encoders->chaos.store(x, residuals);

				int bits = _encoders->y[cy].price(residuals[0]);
				bits += _encoders->u[cu].price(residuals[1]);
				bits += _encoders->v[cv].price(residuals[2]);

				CAT_DEBUG_ENFORCE(bits < 256);
				residuals[3] = static_cast<u8>( bits );
			}
		}
	}
}

void ImageRGBAWriter::designLZ() {
	CAT_INANE("RGBA") << "Finding LZ77 matches...";

	// Find LZ matches
	const u32 *rgba = reinterpret_cast<const u32 *>( _rgba );
	_lz.init(rgba, _residuals.get() + 3, _xsize, _ysize, _mask);

	_lz_enabled = true;
}

void ImageRGBAWriter::maskTiles() {
	const int tiles_size = _tiles_x * _tiles_y;
	_sf_tiles.resizeZero(tiles_size);
	_cf_tiles.resizeZero(tiles_size);

	const u16 tile_xsize = _tile_xsize, tile_ysize = _tile_ysize;
	const u16 xsize = _xsize, ysize = _ysize;
	u8 *cf = _cf_tiles.get();

	// For each tile,
	for (u16 y = 0; y < ysize; y += tile_ysize) {
		for (u16 x = 0; x < xsize; x += tile_xsize, ++cf) {

			// For each element in the tile,
			u16 py = y, cy = tile_ysize;
			while (cy-- > 0 && py < ysize) {
				u16 px = x, cx = tile_xsize;
				while (cx-- > 0 && px < xsize) {
					// If it is not masked,
					if (!IsMasked(px, py)) {
						// We need to do this tile
						//*cf = TODO_TILE; (already 0)
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
	const int SF_USED = _lz_enabled ? SF_COUNT : SF_BASIC_COUNT;

	FilterScorer scores, awards;
	scores.init(SF_USED);
	awards.init(SF_USED);
	awards.reset();
	u8 FPT[3];
	u32 total_score = 0;

	CAT_INANE("RGBA") << "Designing spatial filters (LZ=" << _lz_enabled << ")...";

	const u16 tile_xsize = _tile_xsize, tile_ysize = _tile_ysize;
	const u16 xsize = _xsize, ysize = _ysize;

	u8 *sf = _sf_tiles.get();
	u8 *cf = _cf_tiles.get();
	const u8 *topleft_row = _rgba;

	for (int y = 0; y < _ysize; y += _tile_ysize) {
		const u8 *topleft = topleft_row;

		for (int x = 0; x < _xsize; x += _tile_xsize, ++sf, ++cf, topleft += _tile_xsize * 4) {
			if (*cf == MASK_TILE) {
				continue;
			}

			scores.reset();

			// For each element in the tile,
			const u8 *row = topleft;
			u16 py = y, cy = tile_ysize;
			while (cy-- > 0 && py < ysize) {
				const u8 *data = row;
				u16 px = x, cx = tile_xsize;
				while (cx-- > 0 && px < xsize) {
					// If element is not masked,
					if (!IsMasked(px, py)) {
						const u8 r = data[0], g = data[1], b = data[2];

						for (int f = 0; f < SF_USED; ++f) {
							const u8 *pred = RGBA_FILTERS[f].safe(data, FPT, px, py, xsize);

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
				row += xsize * 4;
			}

			FilterScorer::Score *top = scores.getLow(4, true);
			total_score += 5 + 3 + 1 + 1;
			awards.add(top[0].index, 5);
			awards.add(top[1].index, 3);
			awards.add(top[2].index, 1);
			awards.add(top[3].index, 1);
		}

		topleft_row += _xsize * 4 * _tile_ysize;
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

void ImageRGBAWriter::designTilesFast() {
	CAT_INANE("RGBA") << "Designing SF/CF tiles (fast, low quality) for " << _tiles_x << "x" << _tiles_y << "...";
	const u16 tile_xsize = _tile_xsize, tile_ysize = _tile_ysize;
	const u16 xsize = _xsize, ysize = _ysize;
	u8 FPT[3];

	FilterScorer scores;
	scores.init(_sf_count * CF_COUNT);

	const u8 *topleft_row = _rgba;
	int ty = 0;
	u8 *sf = _sf_tiles.get();
	u8 *cf = _cf_tiles.get();

	// For each tile,
	for (u16 y = 0; y < ysize; y += tile_ysize, ++ty) {
		const u8 *topleft = topleft_row;
		int tx = 0;

		for (u16 x = 0; x < xsize; x += tile_xsize, ++sf, ++cf, topleft += tile_xsize * 4, ++tx) {
			u8 ocf = *cf;

			// If tile is masked,
			if (ocf == MASK_TILE) {
				continue;
			}

			int code_count = 0;

			scores.reset();

			// For each element in the tile,
			const u8 *row = topleft;
			u16 py = y, cy = tile_ysize;
			while (cy-- > 0 && py < ysize) {
				const u8 *data = row;
				u16 px = x, cx = tile_xsize;
				while (cx-- > 0 && px < xsize) {
					// If element is not masked,
					if (!IsMasked(px, py)) {
						// For each spatial filter,
						int index = 0;
						for (int sfi = 0, sfi_end = _sf_count; sfi < sfi_end; ++sfi, index += CF_COUNT) {
							const u8 *pred = _sf[sfi].safe(data, FPT, px, py, xsize);
							u8 residual_rgb[3] = {
								data[0] - pred[0],
								data[1] - pred[1],
								data[2] - pred[2]
							};

							// For each color filter,
							for (int cfi = 0; cfi < CF_COUNT; ++cfi) {
								u8 yuv[3];
								RGB2YUV_FILTERS[cfi](residual_rgb, yuv);

								// Score this combination of SF/CF
								int score = RGBChaos::ResidualScore(yuv[0]) + RGBChaos::ResidualScore(yuv[1]) + RGBChaos::ResidualScore(yuv[2]);
								scores.add(cfi + index, score);
							}
						}

						++code_count;
					}
					++px;
					data += 4;
				}
				++py;
				row += xsize * 4;
			}

			FilterScorer::Score *top = scores.getLowest();
			int best_sf = top->index / CF_COUNT;
			int best_cf = top->index % CF_COUNT;

			*sf = best_sf;
			*cf = best_cf;
		}

		topleft_row += _xsize * 4 * _tile_ysize;
	}
}

void ImageRGBAWriter::designTiles() {
	CAT_INANE("RGBA") << "Designing SF/CF tiles for " << _tiles_x << "x" << _tiles_y << "...";

	const u16 tile_xsize = _tile_xsize, tile_ysize = _tile_ysize;
	const u16 xsize = _xsize, ysize = _ysize;
	u8 FPT[3];

	EntropyEstimator ee[3];
	ee[0].init();
	ee[1].init();
	ee[2].init();

	// Allocate temporary space for entropy analysis
	const u32 code_stride = _tile_xsize * _tile_ysize;
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
		for (u16 y = 0; y < ysize; y += tile_ysize, ++ty) {
			const u8 *topleft = topleft_row;
			int tx = 0;

			for (u16 x = 0; x < xsize; x += tile_xsize, ++sf, ++cf, topleft += tile_xsize * 4, ++tx) {
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
					u16 py = y, cy = tile_ysize;
					while (cy-- > 0 && py < ysize) {
						const u8 *data = row;
						u16 px = x, cx = tile_xsize;
						while (cx-- > 0 && px < xsize) {
							// If element is not masked,
							if (!IsMasked(px, py)) {
								const u8 *pred = _sf[osf].safe(data, FPT, px, py, xsize);
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
						row += xsize * 4;
					}

					ee[0].subtract(codes[0], code_count);
					ee[1].subtract(codes[1], code_count);
					ee[2].subtract(codes[2], code_count);
				}

				int code_count = 0;

				// For each element in the tile,
				const u8 *row = topleft;
				u16 py = y, cy = tile_ysize;
				while (cy-- > 0 && py < ysize) {
					const u8 *data = row;
					u16 px = x, cx = tile_xsize;
					while (cx-- > 0 && px < xsize) {
						// If element is not masked,
						if (!IsMasked(px, py)) {
							u8 *dest_y = codes[0] + code_count;
							u8 *dest_u = codes[1] + code_count;
							u8 *dest_v = codes[2] + code_count;

							// For each spatial filter,
							for (int sfi = 0, sfi_end = _sf_count; sfi < sfi_end; ++sfi) {
								const u8 *pred = _sf[sfi].safe(data, FPT, px, py, xsize);
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
					row += xsize * 4;
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

			topleft_row += _xsize * 4 * _tile_ysize;
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
	const int alpha_size = _xsize * _ysize;
	_alpha.resize(alpha_size);

	u8 *a = _alpha.get();
	const u8 *rgba = _rgba;
	for (int y = 0; y < _ysize; ++y) {
		for (int x = 0; x < _xsize; ++x) {
			*a++ = ~rgba[3]; // Same as 255 - a: Good default filter
			rgba += 4;
		}
	}

	MonoWriter::Parameters params;
	params.knobs = _knobs;
	params.data = _alpha.get();
	params.num_syms = 256;
	params.xsize = _xsize;
	params.ysize = _ysize;
	params.max_filters = 32;
	params.min_bits = 2;
	params.max_bits = 5;
	params.sympal_thresh = 0.1f;
	params.filter_cover_thresh = 0.6f;
	params.filter_inc_thresh = 0.05f;
	params.mask.SetMember<ImageRGBAWriter, &ImageRGBAWriter::IsMasked>(this);
	params.AWARDS[0] = 5;
	params.AWARDS[1] = 3;
	params.AWARDS[2] = 1;
	params.AWARDS[3] = 1;
	params.award_count = 4;
	params.write_order = 0;
	params.lz_enable = false;

	// Right now the match finder for LZ is terrible for this sort of data (way too slow)
	// and besides we have already run LZ77 on the full RGBA dataset.
	//params.lz_mask_color = _mask->enabled() ? static_cast<u8>( ~(getLE(_mask->getColor()) >> 24) ) : 65535;

	_a_encoder.init(params);

	return true;
}

void ImageRGBAWriter::computeResiduals() {
	CAT_INANE("RGBA") << "Executing tiles to generate residual matrix...";

	const u16 tile_xsize = _tile_xsize, tile_ysize = _tile_ysize;
	const u16 xsize = _xsize, ysize = _ysize;
	u8 FPT[3];

	const u8 *sf = _sf_tiles.get();
	const u8 *cf = _cf_tiles.get();

	_residuals.resize(_xsize * _ysize * 4);

	// For each tile,
	const u8 *topleft_row = _rgba;
	size_t residual_delta = (size_t)(_residuals.get() - topleft_row);
	for (u16 y = 0; y < ysize; y += tile_ysize) {
		const u8 *topleft = topleft_row;

		for (u16 x = 0; x < xsize; x += tile_xsize, ++sf, ++cf, topleft += tile_xsize*4) {
			const u8 cfi = *cf;

			if (cfi == MASK_TILE) {
				continue;
			}

			const u8 sfi = *sf;

			// For each element in the tile,
			const u8 *row = topleft;
			u16 py = y, cy = tile_ysize;
			while (cy-- > 0 && py < ysize) {
				const u8 *data = row;
				u16 px = x, cx = tile_xsize;
				while (cx-- > 0 && px < xsize) {
					// If element is not masked,
					if (!IsMasked(px, py)) {
						const u8 *pred = _sf[sfi].safe(data, FPT, px, py, xsize);
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
				row += xsize*4;
			}
		}

		topleft_row += _xsize * 4 * _tile_ysize;
	}
}

void ImageRGBAWriter::designChaos() {
	CAT_INANE("RGBA") << "Designing chaos...";

	u32 best_entropy = 0x7fffffff;

	Encoders *best = 0;
	Encoders *encoders = new Encoders;

	// For each chaos level,
	for (int chaos_levels = 1; chaos_levels < MAX_CHAOS_LEVELS; ++chaos_levels) {
		encoders->chaos.init(chaos_levels, _xsize);
		encoders->chaos.start();

		// For each chaos level,
		for (int ii = 0; ii < chaos_levels; ++ii) {
			encoders->y[ii].init(ImageRGBAReader::NUM_Y_SYMS, ImageRGBAReader::NUM_ZRLE_SYMS);
			encoders->u[ii].init(ImageRGBAReader::NUM_U_SYMS, ImageRGBAReader::NUM_ZRLE_SYMS);
			encoders->v[ii].init(ImageRGBAReader::NUM_V_SYMS, ImageRGBAReader::NUM_ZRLE_SYMS);
		}

		// Reset LZ
		u32 offset = 0;
		if (_lz_enabled) {
			_lz.reset();
		}

		// For each row,
		const u8 *residuals = _residuals.get();
		for (int y = 0; y < _ysize; ++y) {
			// For each column,
			for (int x = 0; x < _xsize; ++x, ++offset) {
				// If we just hit the start of the next LZ copy region,
				if (_lz_enabled && offset == _lz.peekOffset()) {
					// Get chaos bin
					u8 cy, cu, cv;
					encoders->chaos.get(x, cy, cu, cv);

					_lz.train(encoders->y[cy]);
				}

				if (IsMasked(x, y)) {
					// Will eat LZ pixels too
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
			entropy += encoders->y[ii].finalize();
			entropy += encoders->u[ii].finalize();
			entropy += encoders->v[ii].finalize();
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

		// If we have not found a better one in 2 moves,
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

	MonoWriter::generateWriteOrder(_xsize, _ysize,
		MonoWriter::MaskDelegate::FromMember<ImageRGBAWriter, &ImageRGBAWriter::IsMasked>(this),
		_tile_bits_x, _filter_order);
}

bool ImageRGBAWriter::compressSF() {
	MonoWriter::Parameters params;
	params.knobs = _knobs;
	params.data = _sf_tiles.get();
	params.num_syms = _sf_count;
	params.xsize = _tiles_x;
	params.ysize = _tiles_y;
	params.max_filters = 32;
	params.min_bits = 2;
	params.max_bits = 5;
	params.sympal_thresh = 0.1f;
	params.filter_cover_thresh = 0.6f;
	params.filter_inc_thresh = 0.05f;
	params.mask.SetMember<ImageRGBAWriter, &ImageRGBAWriter::IsSFMasked>(this);
	params.AWARDS[0] = 5;
	params.AWARDS[1] = 3;
	params.AWARDS[2] = 1;
	params.AWARDS[3] = 1;
	params.award_count = 4;
	params.write_order = &_filter_order[0];
	params.lz_enable = false;

	CAT_INANE("RGBA") << "Compressing spatial filter matrix...";

	_sf_encoder.init(params);

	return true;
}

bool ImageRGBAWriter::compressCF() {
	MonoWriter::Parameters params;
	params.knobs = _knobs;
	params.data = _cf_tiles.get();
	params.num_syms = CF_COUNT;
	params.xsize = _tiles_x;
	params.ysize = _tiles_y;
	params.max_filters = 32;
	params.min_bits = 2;
	params.max_bits = 5;
	params.sympal_thresh = 0.1f;
	params.filter_cover_thresh = 0.6f;
	params.filter_inc_thresh = 0.05f;
	params.mask.SetMember<ImageRGBAWriter, &ImageRGBAWriter::IsSFMasked>(this);
	params.AWARDS[0] = 5;
	params.AWARDS[1] = 3;
	params.AWARDS[2] = 1;
	params.AWARDS[3] = 1;
	params.award_count = 4;
	params.write_order = &_filter_order[0];
	params.lz_enable = false;

	CAT_INANE("RGBA") << "Compressing color filter matrix...";

	_cf_encoder.init(params);

	return true;
}

bool ImageRGBAWriter::IsMasked(u16 x, u16 y) {
	CAT_DEBUG_ENFORCE(x < _xsize && y < _ysize);

	return _mask->masked(x, y) || (_lz_enabled && _lz.masked(x, y));
}

bool ImageRGBAWriter::IsSFMasked(u16 x, u16 y) {
	CAT_DEBUG_ENFORCE(x < _tiles_x && y < _tiles_y);

	return _cf_tiles[x + _tiles_x * y] == MASK_TILE;
}

int ImageRGBAWriter::init(const u8 *rgba, int xsize, int ysize, ImageMaskWriter &mask, const GCIFKnobs *knobs) {
	_knobs = knobs;
	_rgba = rgba;
	_mask = &mask;
	_lz_enabled = false;

	if (xsize < 0 || ysize < 0) {
		return GCIF_WE_BAD_DIMS;
	}

	if ((!knobs->cm_disableEntropy && knobs->cm_filterSelectFuzz <= 0)) {
		return GCIF_WE_BAD_PARAMS;
	}

	_xsize = xsize;
	_ysize = ysize;

	// Use constant tile size of 4x4 for now
	_tile_bits_x = 2;
	_tile_bits_y = 2;
	_tile_xsize = 1 << _tile_bits_x;
	_tile_ysize = 1 << _tile_bits_y;
	_tiles_x = (_xsize + _tile_xsize - 1) >> _tile_bits_x;
	_tiles_y = (_ysize + _tile_ysize - 1) >> _tile_bits_y;

	// Do a fast first pass at natural compression to better inform LZ decisions
	maskTiles();
	designFilters();
	designTilesFast();
	sortFilters();
	computeResiduals();
	designChaos();
	priceResiduals();

	// Now do informed LZ77 compression
	designLZ();

	// Perform natural image compression post-LZ
	maskTiles();
	designFilters();
	designTiles();
	sortFilters();
	computeResiduals();

	// Compress alpha channel separately like a monochrome image
	compressAlpha();

	// Decide how many chaos levels to use
	designChaos();

	// Generate a write order matrix used for compressing SF/CF information
	generateWriteOrder();

	// Compress SF/CF subresolution tiles like a monochrome image
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

	int lz_table_bits = _lz.writeTables(writer);

	DESYNC_TABLE();

#ifdef CAT_COLLECT_STATS
	Stats.basic_overhead_bits = basic_bits;
	Stats.sf_choice_bits = choice_bits;
	Stats.sf_table_bits = sf_table_bits;
	Stats.cf_table_bits = cf_table_bits;
	Stats.a_table_bits = a_table_bits;
	Stats.lz_table_bits = lz_table_bits;
#endif // CAT_COLLECT_STATS

	return GCIF_WE_OK;
}

bool ImageRGBAWriter::writePixels(ImageWriter &writer) {
	CAT_INANE("RGBA") << "Writing interleaved pixel/filter data...";

#ifdef CAT_COLLECT_STATS
	int sf_bits = 0, cf_bits = 0, y_bits = 0, u_bits = 0, v_bits = 0, a_bits = 0, rgba_count = 0, lz_count = 0, lz_bits = 0;
#endif

	_seen_filter.resize(_tiles_x);

	_encoders->chaos.start();
	for (int ii = 0, iiend = _encoders->chaos.getBinCount(); ii < iiend; ++ii) {
		_encoders->y[ii].reset();
		_encoders->u[ii].reset();
		_encoders->v[ii].reset();
	}

	const u8 *residuals = _residuals.get();
	const u16 tile_mask_y = _tile_ysize - 1;

	// Reset LZ
	u32 offset = 0;
	_lz.reset();

	// For each scanline,
	for (u16 y = 0; y < _ysize; ++y) {
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
		for (u16 x = 0, xsize = _xsize; x < xsize; ++x, ++offset) {
			DESYNC(x, y);

			// If we just hit the start of the next LZ copy region,
			if (offset == _lz.peekOffset()) {
				// Get chaos bin
				u8 cy, cu, cv;
				_encoders->chaos.get(x, cy, cu, cv);

				lz_bits += _lz.write(_encoders->y[cy], writer);
			}

			// If masked,
			if (IsMasked(x, y)) {
				_encoders->chaos.zero(x);
				_a_encoder.zero(x);

				if (_lz.masked(x, y)) {
					++lz_count;
				}
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
	Stats.lz_bits = lz_bits;
	Stats.lz_count = lz_count;
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

	int total = rgba_total + Stats.lz_bits;
	total += _mask->Stats.compressedDataBits;
	Stats.total_bits = total;

	Stats.lz_compression_ratio = Stats.lz_count * 32 / (double)Stats.lz_bits;
	Stats.rgba_compression_ratio = Stats.rgba_count * 32 / (double)Stats.rgba_bits;
	Stats.overall_compression_ratio = _xsize * _ysize * 32 / (double)Stats.total_bits;
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
	CAT_INANE("stats") << "(RGBA Compress)  LZ Table Overhead : " << Stats.lz_table_bits << " bits (" << Stats.lz_table_bits/8 << " bytes, " << Stats.lz_table_bits * 100.f / Stats.total_bits << "% of Total)";
	CAT_INANE("stats") << "(RGBA Compress)      SF Compressed : " << Stats.sf_bits << " bits (" << Stats.sf_bits/8 << " bytes, " << Stats.sf_bits * 100.f / Stats.rgba_bits << "% of RGBA)";
	CAT_INANE("stats") << "(RGBA Compress)      CF Compressed : " << Stats.cf_bits << " bits (" << Stats.cf_bits/8 << " bytes, " << Stats.cf_bits * 100.f / Stats.rgba_bits << "% of RGBA)";
	CAT_INANE("stats") << "(RGBA Compress)       Y Compressed : " << Stats.y_bits << " bits (" << Stats.y_bits/8 << " bytes, " << Stats.y_bits * 100.f / Stats.rgba_bits << "% of RGBA)";
	CAT_INANE("stats") << "(RGBA Compress)       U Compressed : " << Stats.u_bits << " bits (" << Stats.u_bits/8 << " bytes, " << Stats.u_bits * 100.f / Stats.rgba_bits << "% of RGBA)";
	CAT_INANE("stats") << "(RGBA Compress)       V Compressed : " << Stats.v_bits << " bits (" << Stats.v_bits/8 << " bytes, " << Stats.v_bits * 100.f / Stats.rgba_bits << "% of RGBA)";
	CAT_INANE("stats") << "(RGBA Compress)       A Compressed : " << Stats.a_bits << " bits (" << Stats.a_bits/8 << " bytes, " << Stats.a_bits * 100.f / Stats.rgba_bits << "% of RGBA)";
	CAT_INANE("stats") << "(RGBA Compress)          RGBA Data : " << Stats.rgba_bits << " bits (" << Stats.rgba_bits/8 << " bytes, " << Stats.rgba_bits * 100.f / Stats.total_bits << "% of total)";
	CAT_INANE("stats") << "(RGBA Compress)         RGBA Count : " << Stats.rgba_count << " pixels for " << _xsize << "x" << _ysize << " pixel image (" << Stats.rgba_count * 100.f / (_xsize * _ysize) << " % of total)";
	CAT_INANE("stats") << "(RGBA Compress)   RGBA Compression : " << Stats.rgba_compression_ratio << ":1 compression ratio";
	CAT_INANE("stats") << "(RGBA Compress)            LZ Data : " << Stats.lz_bits << " bits (" << Stats.lz_bits/8 << " bytes, " << Stats.lz_bits * 100.f / Stats.total_bits << "% of total)";
	CAT_INANE("stats") << "(RGBA Compress)           LZ Count : " << Stats.lz_count << " pixels for " << _xsize << "x" << _ysize << " pixel image (" << Stats.lz_count * 100.f / (_xsize * _ysize) << " % of total)";
	CAT_INANE("stats") << "(RGBA Compress)     LZ Compression : " << Stats.lz_compression_ratio << ":1 compression ratio";
	CAT_INANE("stats") << "(RGBA Compress)              Overall Size : " << Stats.total_bits << " bits (" << Stats.total_bits/8 << " bytes)";
	CAT_INANE("stats") << "(RGBA Compress) Overall Compression Ratio : " << Stats.overall_compression_ratio << ":1";

	return true;
}

#endif

