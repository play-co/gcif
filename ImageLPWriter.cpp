#include "ImageLPWriter.hpp"
#include "GCIFWriter.hpp"
#include "Log.hpp"
#include "HuffmanEncoder.hpp"
#include "EntropyEstimator.hpp"
#include "Filters.hpp"
#include "BitMath.hpp"
using namespace cat;

#include <vector>
using namespace std;


//// ImageLPWriter

static bool isgood(int area, int used) {
	int required = 16 + 16 + 8 + 8 + 4 + (4 + 32) * used;

	return required <= area * 32 / 4;
}

void ImageLPWriter::clear() {
	if (_visited) {
		delete []_visited;
		_visited = 0;
	}
	_rgba = 0;
}

bool ImageLPWriter::expandMatch(int &used, u16 &x, u16 &y, u16 &w, u16 &h) {
	const u32 *rgba = reinterpret_cast<const u32*>( _rgba );

	const int width = _width;
	const int height = _height;

	// Try expanding to the right
	while (w + 1 <= MAXW && x + w < width) {
		int temp_used = used;

		for (int jj = 0; jj < h; ++jj) {
			int sx = x + w, sy = y + jj;

			if (_mask->hasRGB(sx, sy) ||
					_lz->visited(sx, sy) ||
					visited(sx, sy)) {
				goto try_down;
			}

			u32 color = rgba[sx + sy * width];

			bool found = false;
			for (int kk = 0; kk < temp_used; ++kk) {
				if (_colors[kk] == color) {
					found = true;
					break;
				}
			}
			if (!found) {
				if (!isgood(h * (w+1), temp_used)) {
					goto try_down;
				}

				if (temp_used >= ZONE_MAX_COLORS) {
					goto try_down;
				}
				_colors[temp_used++] = color;
			}
		}

		// Accept any new colors
		used = temp_used;

		// Widen width
		++w;
	}

	// Try expanding down
try_down:
	while (h + 1 <= MAXH && y + h < height) {
		int temp_used = used;

		for (int jj = 0; jj < w; ++jj) {
			int sx = x + jj, sy = y + h;

			if (_mask->hasRGB(sx, sy) ||
					_lz->visited(sx, sy) ||
					visited(sx, sy)) {
				goto try_left;
			}

			u32 color = rgba[sx + sy * width];

			bool found = false;
			for (int kk = 0; kk < temp_used; ++kk) {
				if (_colors[kk] == color) {
					found = true;
					break;
				}
			}
			if (!found) {
				if (!isgood((h+1) * w, temp_used)) {
					goto try_left;
				}

				if (temp_used >= ZONE_MAX_COLORS) {
					goto try_left;
				}
				_colors[temp_used++] = color;
			}
		}

		// Accept any new colors
		used = temp_used;

		// Heighten height
		++h;
	}

	// Try expanding left
try_left:
	while (w + 1 <= MAXW && x >= 1) {
		int temp_used = used;

		for (int jj = 0; jj < h; ++jj) {
			int sx = x - 1, sy = y + jj;

			if (_mask->hasRGB(sx, sy) ||
					_lz->visited(sx, sy) ||
					visited(sx, sy)) {
				goto try_up;
			}

			u32 color = rgba[sx + sy * width];

			bool found = false;
			for (int kk = 0; kk < temp_used; ++kk) {
				if (_colors[kk] == color) {
					found = true;
					break;
				}
			}
			if (!found) {
				if (!isgood(h * (w+1), temp_used)) {
					goto try_up;
				}

				if (temp_used >= ZONE_MAX_COLORS) {
					goto try_up;
				}
				_colors[temp_used++] = color;
			}
		}

		// Accept any new colors
		used = temp_used;

		// Widen width
		++w;
		--x;
	}

	// Try expanding up
try_up:
	while (h + 1 <= MAXH && y >= 1) {
		int temp_used = used;

		for (int jj = 0; jj < w; ++jj) {
			int sx = x + jj, sy = y - 1;

			if (_mask->hasRGB(sx, sy) ||
					_lz->visited(sx, sy) ||
					visited(sx, sy)) {
				return true;
			}

			u32 color = rgba[sx + sy * width];

			bool found = false;
			for (int kk = 0; kk < temp_used; ++kk) {
				if (_colors[kk] == color) {
					found = true;
					break;
				}
			}
			if (!found) {
				if (!isgood((h+1) * w, temp_used)) {
					return true;
				}

				if (temp_used >= ZONE_MAX_COLORS) {
					return true;
				}
				_colors[temp_used++] = color;
			}
		}

		// Accept any new colors
		used = temp_used;

		// Heighten height
		++h;
		--y;
	}

	return true;
}

void ImageLPWriter::add(int used, u16 x, u16 y, u16 w, u16 h) {
	Match m;

	for (int ii = 0; ii < w; ++ii) {
		for (int jj = 0; jj < h; ++jj) {
			visit(x + ii, y + jj, _exact_matches.size());
		}
	}

	for (int ii = 0; ii < used; ++ii) {
		m.colors[ii] = _colors[ii];
	}
	m.used = used;
	m.x = x;
	m.y = y;
	m.w = w;
	m.h = h;

	_exact_matches.push_back(m);

	//CAT_WARN("TEST") << "Match: " << x << "," << y << " [" << w << "," << h << "] : " << used;
}

int ImageLPWriter::match() {
	const u32 *rgba = reinterpret_cast<const u32*>( _rgba );

	for (int y = 0, yend = _height - ZONEH; y <= yend; ++y) {
		const u32 *p = rgba;

		for (int x = 0, xend = _width - ZONEW; x <= xend; ++x) {
			int used = 0;
			u16 mx, my, w, h;

			for (int ii = 0; ii < ZONEW; ++ii) {
				for (int jj = 0; jj < ZONEH; ++jj) {
					int sx = x + ii;
					int sy = y + jj;

					if (_mask->hasRGB(sx, sy) ||
						_lz->visited(sx, sy) ||
						visited(sx, sy)) {
						goto abort;
					}

					u32 color = rgba[ii + jj * _width];
					bool found = false;

					for (int kk = 0; kk < used; ++kk) {
						if (color == _colors[kk]) {
							found = true;
							break;
						}
					}

					if (!found) {
						if (used >= ZONE_MAX_COLORS) {
							goto abort;
						}
						_colors[used++] = color;
					}
				}
			}

			// Match found here
			mx = x, my = y, w = ZONEW, h = ZONEH;
			expandMatch(used, mx, my, w, h);
			add(used, mx, my, w, h);
abort:

			++p;
		}

		rgba += _width;
	}

	return WE_OK;
}

int ImageLPWriter::initFromRGBA(const u8 *rgba, int width, int height, ImageMaskWriter &mask, ImageLZWriter &lz) {
	clear();

	if (width < ZONEW || height < ZONEH) {
		return WE_BAD_DIMS;
	}

	_rgba = rgba;
	_width = width;
	_height = height;

	_mask = &mask;
	_lz = &lz;

	_visited = new u16[width * height];
	memset(_visited, 0, width * height * sizeof(u16));

	return match();
}

void ImageLPWriter::write(ImageWriter &writer) {
	vector<u32> colors;
	FreqHistogram<256> indexHist[2];
#ifdef CAT_COLLECT_STATS
	u32 covered = 0, ccnt = 0;
#endif

	for (int ii = 0; ii < _exact_matches.size(); ++ii) {
		Match *m = &_exact_matches[ii];
#ifdef CAT_COLLECT_STATS
		ccnt += m->used;
#endif

		for (int jj = 0; jj < m->used; ++jj) {
			u32 color = m->colors[jj];
			bool found = false;
			u32 colorId = 0;

			for (int kk = 0; kk < colors.size(); ++kk) {
				if (color == colors[kk]) {
					found = true;
					colorId = kk;
					break;
				}
			}
			if (!found) {
				colorId = colors.size();
				colors.push_back(color);
			}

			m->colorIndex[jj] = colorId;

			indexHist[0].add((u8)colorId);
			indexHist[1].add((u8)(colorId >> 8));
		}

#ifdef CAT_COLLECT_STATS
		covered += m->w * m->h;
#endif
	}

	// Write color table
	writer.writeBits(colors.size(), 16);

	if (colors.size() <= 0) {
		return;
	}

	// If there are enough colors to justify the expense,
#ifdef CAT_COLLECT_STATS
	u32 color_overhead = 32;
	Stats.total_palette_entries = ccnt;
#endif
	if (colors.size() >= HUFF_COLOR_THRESH) {
		// Huffman-encode the color table
		FreqHistogram<256> hist[4];

		EntropyEstimator<u32> ee[3];
		ee[0].clear(256);
		ee[1].clear(256);
		ee[2].clear(256);

		// Select a color filter for the table
		double best = 0;
		int bestCF = 0;
		for (int cf = 0; cf < CF_COUNT; ++cf) {
			ee[0].setup();
			ee[1].setup();
			ee[2].setup();

			RGB2YUVFilterFunction cff = RGB2YUV_FILTERS[cf];

			for (int ii = 0; ii < colors.size(); ++ii) {
				u32 rgba = getLE(colors[ii]);
				u8 rgb[3] = {
					(u8)rgba,
					(u8)(rgba >> 8),
					(u8)(rgba >> 16)
				};

				u8 yuv[3];
				cff(rgb, yuv);

				ee[0].push(yuv[0]);
				ee[1].push(yuv[1]);
				ee[2].push(yuv[2]);
			}

			double e = ee[0].entropy() + ee[1].entropy() + ee[2].entropy();
			if (cf == 0 || e < best) {
				best = e;
				bestCF = cf;
			}
		}

		writer.writeBits(bestCF, 4);
		CAT_ENFORCE(CF_COUNT <= 16);

		RGB2YUVFilterFunction cff = RGB2YUV_FILTERS[bestCF];

		// Collect stats
		for (int ii = 0; ii < colors.size(); ++ii) {
			u32 rgba = getLE(colors[ii]);
			u8 rgb[3] = {
				(u8)rgba,
				(u8)(rgba >> 8),
				(u8)(rgba >> 16)
			};
			u8 a = (u8)(rgba >> 24);

			u8 yuv[3];
			cff(rgb, yuv);

			hist[0].add(yuv[0]);
			hist[1].add(yuv[1]);
			hist[2].add(yuv[2]);
			hist[3].add(a);
		}

		u16 codes[4][256];
		u8 codelens[4][256];

		// Store Huffman codelens
		for (int ii = 0; ii < 4; ++ii) {
			hist[ii].generateHuffman(codes[ii], codelens[ii]);

			int table_bits = writeHuffmanTable(256, codelens[ii], writer);

#ifdef CAT_COLLECT_STATS
			color_overhead += table_bits;
#endif
		}

		// Write encoded colors
		for (int ii = 0; ii < colors.size(); ++ii) {
			u32 rgba = getLE(colors[ii]);
			u8 rgb[3] = {
				(u8)rgba,
				(u8)(rgba >> 8),
				(u8)(rgba >> 16)
			};
			u8 a = (u8)(rgba >> 24);

			u8 yuv[3];
			cff(rgb, yuv);

			writer.writeBits(codes[0][yuv[0]], codelens[0][yuv[0]]);
			writer.writeBits(codes[1][yuv[1]], codelens[1][yuv[1]]);
			writer.writeBits(codes[2][yuv[2]], codelens[2][yuv[2]]);
			writer.writeBits(codes[3][a], codelens[3][a]);
#ifdef CAT_COLLECT_STATS
			color_overhead += codelens[0][yuv[0]];
			color_overhead += codelens[1][yuv[1]];
			color_overhead += codelens[2][yuv[2]];
			color_overhead += codelens[3][a];
#endif
		}
	} else {
		// Just dump them out
		for (int ii = 0; ii < colors.size(); ++ii) {
			writer.writeWord(colors[ii]);
#ifdef CAT_COLLECT_STATS
			color_overhead += 32;
#endif
		}
	}

	// Write zone list size
	writer.writeBits(_exact_matches.size(), 16);

#ifdef CAT_COLLECT_STATS
	u32 overhead = 32;
	u32 pixelsize = 0;
#endif

	u16 last_x = 0, last_y = 0;

	// If Huffman-encoding this is worthwhile,
	if (_exact_matches.size() >= HUFF_ZONE_THRESH) {
		// x[0] x[1] y[0] y[1] w+h used
		FreqHistogram<256> hist[6];

		// Collect stats
		for (int ii = 0; ii < _exact_matches.size(); ++ii) {
			Match *m = &_exact_matches[ii];

			// Context modeling for the offsets
			u16 ex = m->x;
			u16 ey = m->y - last_y;
			if (ey == 0) {
				ex -= last_x;
			}
			u8 ew = m->w - ZONEW;
			u8 eh = m->h - ZONEH;

			hist[0].add((u8)ex);
			hist[1].add((u8)(ex >> 8));
			hist[2].add((u8)ey);
			hist[3].add((u8)(ey >> 8));
			hist[4].add(ew);
			hist[4].add(eh);
			hist[5].add(m->used - 1);

			last_x = m->x;
			last_y = m->y;
		}

		// Generate Huffman codes
		u16 codes[6][256];
		u8 codelens[6][256];
		static const int NUM_SYMS[6] = {
			256, 256, 256, 256, 256, MAX_COLORS
		};

		for (int ii = 0; ii < 6; ++ii) {
			hist[ii].generateHuffman(codes[ii], codelens[ii]);

			int table_bits = writeHuffmanTable(NUM_SYMS[ii], codelens[ii], writer);

#ifdef CAT_COLLECT_STATS
			overhead += table_bits;
#endif
		}

		// Color index huffman tables
		u16 index_codes[2][256];
		u8 index_codelens[2][256];
		indexHist[0].generateHuffman(index_codes[0], index_codelens[0]);
		indexHist[1].generateHuffman(index_codes[1], index_codelens[1]);

		int index_huff_bits = writeHuffmanTable(256, index_codelens[0], writer);

		// If there are more colors than one byte,
		if (colors.size() > 256) {
			index_huff_bits += writeHuffmanTable(256, index_codelens[1], writer);
		}

#ifdef CAT_COLLECT_STATS
		overhead += index_huff_bits;
#endif

		// Encode zones
		for (int ii = 0; ii < _exact_matches.size(); ++ii) {
			Match *m = &_exact_matches[ii];

			// Context modeling for the offsets
			u16 ex = m->x;
			u16 ey = m->y - last_y;
			if (ey == 0) {
				ex -= last_x;
			}
			u8 ew = m->w - ZONEW;
			u8 eh = m->h - ZONEH;

			// Write em
			u8 sym = (u8)ex;
			writer.writeBits(codes[0][sym], codelens[0][sym]);
#ifdef CAT_COLLECT_STATS
			overhead += codelens[0][sym];
#endif
			sym = (u8)(ex >> 8);
			writer.writeBits(codes[1][sym], codelens[1][sym]);
#ifdef CAT_COLLECT_STATS
			overhead += codelens[1][sym];
#endif
			sym = (u8)ey;
			writer.writeBits(codes[2][sym], codelens[2][sym]);
#ifdef CAT_COLLECT_STATS
			overhead += codelens[2][sym];
#endif
			sym = (u8)(ey >> 8);
			writer.writeBits(codes[3][sym], codelens[3][sym]);
#ifdef CAT_COLLECT_STATS
			overhead += codelens[3][sym];
#endif
			writer.writeBits(codes[4][ew], codelens[4][ew]);
#ifdef CAT_COLLECT_STATS
			overhead += codelens[4][ew];
#endif
			writer.writeBits(codes[4][eh], codelens[4][eh]);
#ifdef CAT_COLLECT_STATS
			overhead += codelens[4][eh];
#endif
			sym = (u8)(m->used - 1);
			writer.writeBits(codes[5][sym], codelens[5][sym]);
#ifdef CAT_COLLECT_STATS
			overhead += codelens[5][sym];
#endif

			if (m->used == 1) {
				u16 ci = m->colorIndex[0];
				writer.writeBits(index_codes[0][(u8)ci], index_codelens[0][(u8)ci]);
#ifdef CAT_COLLECT_STATS
				overhead += index_codelens[0][(u8)ci];
#endif
				if (colors.size() > 256) {
					writer.writeBits(index_codes[1][(u8)(ci >> 8)], index_codelens[1][(u8)(ci >> 8)]);
#ifdef CAT_COLLECT_STATS
					overhead += index_codelens[1][(u8)(ci >> 8)];
#endif
				}
			} else {
				// Collect stats
				FreqHistogram<MAX_HUFF_SYMS> hist;
				for (int y = m->y, yend = m->y + m->h; y < yend; ++y) {
					for (int x = m->x, xend = m->x + m->w; x < xend; ++x) {
						u32 color = _rgba[x + y * _width];
						int colorIndex = 0;

						for (int jj = 0; jj < m->used; ++jj) {
							u32 tc = m->colors[jj];

							if (color == tc) {
								colorIndex = jj;
								break;
							}
						}

						hist.add(colorIndex);
					}
				}

				hist.generateHuffman(m->codes, m->codelens);

				int table_bits = writeHuffmanTable(m->used, m->codelens, writer);
#ifdef CAT_COLLECT_STATS
				overhead += table_bits;
#endif

				for (int jj = 0; jj < m->used; ++jj) {
					u16 ci = m->colorIndex[jj];
					writer.writeBits(index_codes[0][(u8)ci], index_codelens[0][(u8)ci]);
#ifdef CAT_COLLECT_STATS
					overhead += index_codelens[0][(u8)ci];
#endif
					if (colors.size() > 256) {
						writer.writeBits(index_codes[1][(u8)(ci >> 8)], index_codelens[1][(u8)(ci >> 8)]);
#ifdef CAT_COLLECT_STATS
						overhead += index_codelens[1][(u8)(ci >> 8)];
#endif
					}
				}

#ifdef CAT_COLLECT_STATS
				// Calculate the space taken
				for (int y = m->y, yend = m->y + m->h; y < yend; ++y) {
					for (int x = m->x, xend = m->x + m->w; x < xend; ++x) {
						u32 color = _rgba[x + y * _width];
						int colorIndex = 0;

						for (int jj = 0; jj < m->used; ++jj) {
							u32 tc = m->colors[jj];

							if (color == tc) {
								colorIndex = jj;
								break;
							}
						}

						pixelsize += m->codelens[colorIndex];
					}
				}
#endif

			}

			last_x = m->x;
			last_y = m->y;
		}
	} else {
		int colorIndexBits = (int)BSR32(colors.size() - 1) + 1;
		for (int ii = 0; ii < _exact_matches.size(); ++ii) {
			Match *m = &_exact_matches[ii];

			writer.writeBits(m->x, 16);
			writer.writeBits(m->y, 16);
			writer.writeBits(m->w - ZONEW, 8);
			writer.writeBits(m->h - ZONEH, 8);
			writer.writeBits(m->used - 1, 4);
			CAT_ENFORCE(MAX_COLORS <= 16);
#ifdef CAT_COLLECT_STATS
			overhead += 16 + 16 + 8 + 8 + 4 + m->used * colorIndexBits;
#endif

			if (m->used > 1) {
				// Collect stats
				FreqHistogram<MAX_HUFF_SYMS> hist;
				for (int y = m->y, yend = m->y + m->h; y < yend; ++y) {
					for (int x = m->x, xend = m->x + m->w; x < xend; ++x) {
						u32 color = _rgba[x + y * _width];
						int colorIndex = 0;

						for (int jj = 0; jj < m->used; ++jj) {
							u32 tc = m->colors[jj];

							if (color == tc) {
								colorIndex = jj;
								break;
							}
						}

						hist.add(colorIndex);
					}
				}

				hist.generateHuffman(m->codes, m->codelens);

				int table_bits = writeHuffmanTable(m->used, m->codelens, writer);
#ifdef CAT_COLLECT_STATS
				overhead += table_bits;
#endif
			}


			for (int jj = 0; jj < m->used; ++jj) {
				writer.writeBits(m->colorIndex[jj], colorIndexBits);
				m->codes[jj] = jj;
				m->codelens[jj] = 4;
			}
		}
	}

#ifdef CAT_COLLECT_STATS
	int overall = overhead + color_overhead + pixelsize;
	double compressionRatio = covered * 4 / (double)(overall/8);

	Stats.color_list_size = (u32)colors.size();
	Stats.color_list_overhead = color_overhead;
	Stats.zone_list_overhead = overhead;
	Stats.pixel_overhead = pixelsize;
	Stats.pixels_covered = covered;
	Stats.zone_count = (u32)_exact_matches.size();
	Stats.overall_bytes = overall / 8;
	Stats.compressionRatio = compressionRatio;
#endif
}

#ifdef CAT_COLLECT_STATS

bool ImageLPWriter::dumpStats() {
	CAT_INFO("stats") << "(LP Compress) Color palette size : " << Stats.color_list_size << " colors collected from " << Stats.total_palette_entries << " total palette entries";
	CAT_INFO("stats") << "(LP Compress) Color palette overhead : " << Stats.color_list_overhead/8 << " bytes";
	CAT_INFO("stats") << "(LP Compress) Zone count : " << Stats.zone_count;
	CAT_INFO("stats") << "(LP Compress) Zone list overhead : " << Stats.zone_list_overhead/8 << " bytes";
	CAT_INFO("stats") << "(LP Compress) Pixels covered : " << Stats.pixels_covered << " of " << _width * _height << " (" << Stats.pixels_covered * 100. / (_width * _height) << " % covered)";
	CAT_INFO("stats") << "(LP Compress) Pixel data size after encoding : " << Stats.pixel_overhead/8 << " bytes";
	CAT_INFO("stats") << "(LP Compress) Compression ratio : " << Stats.compressionRatio << ":1 (" << Stats.overall_bytes << " bytes used overall)";

	return true;
}

#endif // CAT_COLLECT_STATS

