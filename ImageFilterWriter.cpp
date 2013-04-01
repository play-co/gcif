#include "ImageFilterWriter.hpp"
#include "BitMath.hpp"
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

static CAT_INLINE int wrapNeg(u8 p) {
	if (p == 0) {
		return 0;
	} else if (p < 128) {
		return ((p - 1) << 1) | 1;
	} else {
		return (256 - p) << 1;
	}
}


static void collectFreqs(const std::vector<u8> &lz, u16 freqs[256]) {
	const int NUM_SYMS = 256;
	const int lzSize = static_cast<int>( lz.size() );
	const int MAX_FREQ = 0xffff;

	int hist[NUM_SYMS] = {0};
	int max_freq = 0;

	// Perform histogram, and find maximum symbol count
	for (int ii = 0; ii < lzSize; ++ii) {
		int count = ++hist[lz[ii]];

		if (max_freq < count) {
			max_freq = count;
		}
	}

	// Scale to fit in 16-bit frequency counter
	while (max_freq > MAX_FREQ) {
		// For each symbol,
		for (int ii = 0; ii < NUM_SYMS; ++ii) {
			int count = hist[ii];

			// If it exists,
			if (count) {
				count >>= 1;

				// Do not let it go to zero if it is actually used
				if (!count) {
					count = 1;
				}
			}
		}

		// Update max
		max_freq >>= 1;
	}

	// Store resulting scaled histogram
	for (int ii = 0; ii < NUM_SYMS; ++ii) {
		freqs[ii] = static_cast<u16>( hist[ii] );
	}
}

static void generateHuffmanCodes(int num_syms, u16 freqs[], u16 codes[], u8 codelens[]) {
	huffman::huffman_work_tables state;
	u32 max_code_size, total_freq;

	huffman::generate_huffman_codes(&state, num_syms, freqs, codelens, max_code_size, total_freq);

	if (max_code_size > HuffmanDecoder::MAX_CODE_SIZE) {
		huffman::limit_max_code_size(num_syms, codelens, HuffmanDecoder::MAX_CODE_SIZE);
	}

	huffman::generate_codes(num_syms, codelens, codes);
}

static int calcBits(vector<u8> &lz, u8 codelens[256]) {
	int bits = 0;

	for (int ii = 0; ii < lz.size(); ++ii) {
		int sym = lz[ii];
		bits += codelens[sym];
	}

	return bits;
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

bool ImageFilterWriter::init(int width, int height) {
	clear();

	if (width < 8 || height < 8) {
		return false;
	}

	if ((width & 7) | (height & 7)) {
		return false;
	}

	_w = width >> 3;
	_h = height >> 3;
	_matrix = new u16[_w * _h];
	_chaos = new u8[width * 3 + 3];

	return true;
}

void ImageFilterWriter::decideFilters(u8 *rgba, int width, int height, ImageMaskWriter &mask) {
	u16 *filterWriter = _matrix;

	static const int FSZ = 8;

	for (int y = height - FSZ; y >= 0; y -= FSZ) {
		for (int x = width - FSZ; x >= 0; x -= FSZ) {
			int predErrors[SF_COUNT*CF_COUNT] = {0};

			// Determine best filter combination to use

			// For each pixel in the 8x8 zone,
			for (int yy = FSZ-1; yy >= 0; --yy) {
				for (int xx = FSZ-1; xx >= 0; --xx) {
					int px = x + xx, py = y + yy;
					if (mask.hasRGB(px, py)) {
						continue;
					}

					u8 *p = &rgba[(px + py * width) * 4];

					// Calculate spatial filter predictions
					u8 sfPred[SF_COUNT*3];
					for (int plane = 0; plane < 3; ++plane) {
						int a, c, b, d;

						// Grab ABCD
						if (px > 0) {
							if (py > 0) {
								a = p[plane - 4];
								c = p[plane - 4 - width*4];
								b = p[plane - width*4];
								if (px < width-1) {
									d = p[plane + 4 - width*4];
								} else {
									d = 0;
								}
							} else {
								a = p[plane - 4];
								c = 0;
								b = 0;
								d = 0;
							}
						} else {
							if (py > 0) {
								a = 0;
								c = 0;
								b = p[plane - width*4];
								if (px < width-1) {
									d = p[plane + 4 - width*4];
								} else {
									d = 0;
								}
							} else {
								a = 0;
								c = 0;
								b = 0;
								d = 0;
							}
						}

						sfPred[SF_Z + plane*SF_COUNT] = 0;
						sfPred[SF_A + plane*SF_COUNT] = a;
						sfPred[SF_B + plane*SF_COUNT] = b;
						sfPred[SF_C + plane*SF_COUNT] = c;
						sfPred[SF_D + plane*SF_COUNT] = d;
						sfPred[SF_AB + plane*SF_COUNT] = (u8)((a + b) / 2);
						sfPred[SF_AD + plane*SF_COUNT] = (u8)((a + d) / 2);
						sfPred[SF_A_BC + plane*SF_COUNT] = (u8)(a + (b - c) / 2);
						sfPred[SF_B_AC + plane*SF_COUNT] = (u8)(b + (a - c) / 2);
						sfPred[SF_ABCD + plane*SF_COUNT] = (u8)((a + b + c + d + 1) / 4);
						int abc = a + b - c;
						if (abc > 255) abc = 255;
						else if (abc < 0) abc = 0;
						sfPred[SF_ABC_CLAMP + plane*SF_COUNT] = (u8)abc;

						// Paeth filter
						sfPred[SF_PAETH + plane*SF_COUNT] = paeth(a, b, c);

						// Modified Paeth
						sfPred[SF_ABC_PAETH + plane*SF_COUNT] = abc_paeth(a, b, c);
					}

					// Calculate color filter predictions
					u8 xr = p[0], xg = p[1], xb = p[2];
					for (int ii = 0; ii < SF_COUNT; ++ii) {
						// Get predicted RGB
						u8 pr = sfPred[ii];
						u8 pg = sfPred[ii + SF_COUNT];
						u8 pb = sfPred[ii + SF_COUNT*2];

						// Apply spatial filter
						u8 r = xr - pr;
						u8 g = xg - pg;
						u8 b = xb - pb;

						// Calculate color filter error
						predErrors[ii + SF_COUNT*CF_NOOP] += score(r) + score(g) + score(b);
						predErrors[ii + SF_COUNT*CF_GB_RG] += score(r-g) + score(g-b) + score(b);
						predErrors[ii + SF_COUNT*CF_GB_RB] += score(r-b) + score(g-b) + score(b);
						predErrors[ii + SF_COUNT*CF_GR_BR] += score(r) + score(g-r) + score(b-r);
						predErrors[ii + SF_COUNT*CF_GR_BG] += score(r) + score(g-r) + score(b-g);
						predErrors[ii + SF_COUNT*CF_BG_RG] += score(r-g) + score(g) + score(b-g);
					}
				}
			}

			// Find lowest error filter
			int lowestSum = predErrors[0];
			int bestSF = 0;
			for (int ii = 1; ii < SF_COUNT*CF_COUNT; ++ii) {
				if (predErrors[ii] < lowestSum) {
					lowestSum = predErrors[ii];
					bestSF = ii;
				}
			}

			// Write it out
			u8 sf = bestSF % SF_COUNT;
			u8 cf = bestSF / SF_COUNT;
			u16 filter = ((u16)sf << 8) | cf;

			setFilter(x, y, filter);
		}
	}
}

void ImageFilterWriter::applyFilters(u8 *rgba, int width, int height, ImageMaskWriter &mask) {
	u16 *filterWriter = _matrix;

	static const int FSZ = 8;

	// For each zone,
	for (int y = height - 1; y >= 0; --y) {
		for (int x = width - FSZ; x >= 0; x -= FSZ) {
			u16 filter = getFilter(x, y);
			u8 cf = (u8)filter;
			u8 sf = (u8)(filter >> 8);

			// For each zone pixel,
			for (int xx = FSZ-1; xx >= 0; --xx) {
				int px = x + xx, py = y;
				if (mask.hasRGB(px, py)) {
					continue;
				}

				u8 *p = &rgba[(px + py * width) * 4];

				u8 fp[3];

				for (int plane = 0; plane < 3; ++plane) {
					int a, c, b, d;

					// Grab ABCD
					if (px > 0) {
						if (py > 0) {
							a = p[plane - 4];
							c = p[plane - 4 - width*4];
							b = p[plane - width*4];
							if (px < width-1) {
								d = p[plane + 4 - width*4];
							} else {
								d = 0;
							}
						} else {
							a = p[plane - 4];
							c = 0;
							b = 0;
							d = 0;
						}
					} else {
						if (py > 0) {
							a = 0;
							c = 0;
							b = p[plane - width*4];
							if (px < width-1) {
								d = p[plane + 4 - width*4];
							} else {
								d = 0;
							}
						} else {
							a = 0;
							c = 0;
							b = 0;
							d = 0;
						}
					}

					u8 pred;

					switch (sf) {
						default:
						case SF_Z:			// 0
							pred = 0;
							break;
						case SF_A:			// A
							pred = a;
							break;
						case SF_B:			// B
							pred = b;
							break;
						case SF_C:			// C
							pred = c;
							break;
						case SF_D:			// D
							pred = d;
							break;
						case SF_AB:			// (A + B)/2
							pred = (u8)((a + b) / 2);
							break;
						case SF_AD:			// (A + D)/2
							pred = (u8)((a + d) / 2);
							break;
						case SF_A_BC:		// A + (B - C)/2
							pred = (u8)(a + (b - c) / 2);
							break;
						case SF_B_AC:		// B + (A - C)/2
							pred = (u8)(b + (a - c) / 2);
							break;
						case SF_ABCD:		// (A + B + C + D + 1)/4
							pred = (u8)((a + b + c + d + 1) / 4);
							break;
						case SF_ABC_CLAMP:	// A + B - C clamped to [0, 255]
							{
								int abc = a + b - c;
								if (abc > 255) abc = 255;
								else if (abc < 0) abc = 0;
								pred = (u8)abc;
							}
							break;
						case SF_PAETH:		// Paeth filter
							{
								pred = paeth(a, b, c);
							}
							break;
						case SF_ABC_PAETH:	// If A <= C <= B, A + B - C, else Paeth filter
							{
								pred = abc_paeth(a, b, c);
							}
							break;
					}

					fp[plane] = p[plane] - pred;
				}

				switch (cf) {
					default:
					case CF_NOOP:
						// No changes necessary
						break;
					case CF_GB_RG:
						fp[0] -= fp[1];
						fp[1] -= fp[2];
						break;
					case CF_GB_RB:
						fp[0] -= fp[2];
						fp[1] -= fp[2];
						break;
					case CF_GR_BR:
						fp[1] -= fp[0];
						fp[2] -= fp[0];
						break;
					case CF_GR_BG:
						fp[2] -= fp[1];
						fp[1] -= fp[0];
						break;
					case CF_BG_RG:
						fp[0] -= fp[1];
						fp[2] -= fp[1];
						break;
				}

				p[0] = fp[0];
				p[1] = fp[1];
				p[2] = fp[2];
			}

			//rgba[(x + y * width) * 4] = 255;
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

void ImageFilterWriter::chaosEncode(u8 *rgba, int width, int height, ImageMaskWriter &mask) {
#ifdef GENERATE_CHAOS_TABLE
	GenerateChaosTable();
#endif

	static const int RLE_SYMS = 7;

	u8 *last_chaos = _chaos;
	CAT_CLR(last_chaos, width * 3 + 3);

	u8 *last = rgba;
	u8 *now = rgba;

	vector<u8> test;

	u16 hist[3][16][256 + RLE_SYMS] = {0};
	int rle[3][16] = {0};

	for (int y = 0; y < height; ++y) {
		u8 left_rgb[3] = {0};
		u8 *last_chaos_read = last_chaos + 3;

		for (int x = 0; x < width; ++x) {
			u16 chaos[3] = {left_rgb[0], left_rgb[1], left_rgb[2]};
			if (y > 0) {
				chaos[0] += score(last[0]);
				chaos[1] += score(last[1]);
				chaos[2] += score(last[2]);
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
#endif
			left_rgb[0] = score(now[0]);
			left_rgb[1] = score(now[1]);
			left_rgb[2] = score(now[2]);
			{
				test.push_back(chaos[0] * 256/8);
				test.push_back(chaos[0] * 256/8);
				test.push_back(chaos[0] * 256/8);

				if (!mask.hasRGB(x, y)) {
					for (int ii = 0; ii < 3; ++ii) {
						if (now[ii]) {
							int count = rle[ii][chaos[ii]];
							if (count) {
								rle[ii][chaos[ii]] = 0;
								if (count <= RLE_SYMS) {
									if (count == 1) {
										hist[ii][chaos[ii]][0]++;
									} else {
										hist[ii][chaos[ii]][254 + count]++;
									}
								} else {
									hist[ii][chaos[ii]][255 + RLE_SYMS]++;
								}
							}
							hist[ii][chaos[ii]][now[ii]]++;
						} else {
							rle[ii][chaos[ii]]++;
						}
					}
				}
			}

			last_chaos_read[0] = (chaos[0] + 1) >> 1;
			last_chaos_read[1] = (chaos[1] + 1) >> 1;
			last_chaos_read[2] = (chaos[2] + 1) >> 1;
			last_chaos_read += 3;

			now += 4;
		}
	}

	for (int ii = 0; ii < 3; ++ii) {
		for (int jj = 0; jj < 16; ++jj) {
			int count = rle[ii][jj];

			if (count) {
				if (count <= RLE_SYMS) {
					if (count == 1) {
						hist[ii][jj][0]++;
					} else {
						hist[ii][jj][254 + count]++;
					}
				} else {
					hist[ii][jj][255 + RLE_SYMS]++;
				}
			}
		}
	}

	CAT_CLR(last_chaos, width * 3 + 3);
	CAT_OBJCLR(rle);

	u8 codelens[3][16][256 + RLE_SYMS];

	for (int ii = 0; ii < 3; ++ii) {
		for (int jj = 0; jj < 16; ++jj) {
			u16 codes[256 + RLE_SYMS];

			generateHuffmanCodes(256 + RLE_SYMS, hist[ii][jj], codes, codelens[ii][jj]);
		}
	}

	for (int ii = 0; ii < 256 + RLE_SYMS; ++ii) {
		cout << ii << ": " << (int)codelens[0][0][ii] << endl;
	}

	last = rgba;
	now = rgba;

	int bitcount[3] = {0};

	for (int y = 0; y < height; ++y) {
		u8 left_rgb[3] = {0};
		u8 *last_chaos_read = last_chaos + 3;

		for (int x = 0; x < width; ++x) {
			u16 chaos[3] = {left_rgb[0], left_rgb[1], left_rgb[2]};
			if (y > 0) {
				chaos[0] += score(last[0]);
				chaos[1] += score(last[1]);
				chaos[2] += score(last[2]);
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
#endif
			left_rgb[0] = score(now[0]);
			left_rgb[1] = score(now[1]);
			left_rgb[2] = score(now[2]);
			{
				if (!mask.hasRGB(x, y)) {
					for (int ii = 0; ii < 3; ++ii) {
						if (now[ii]) {
							int count = rle[ii][chaos[ii]];
							if (count) {
								if (count <= RLE_SYMS) {
									if (count == 1) {
										bitcount[ii] += codelens[ii][chaos[ii]][0];
									} else {
										bitcount[ii] += codelens[ii][chaos[ii]][254 + count];
									}
								} else {
									bitcount[ii] += codelens[ii][chaos[ii]][255 + RLE_SYMS];
									bitcount[ii] += 4; //approx
								}
								rle[ii][chaos[ii]] = 0;
							}
							bitcount[ii] += codelens[ii][chaos[ii]][now[ii]];
						} else {
							rle[ii][chaos[ii]]++;
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

	for (int ii = 0; ii < 3; ++ii) {
		for (int jj = 0; jj < 16; ++jj) {
			int count = rle[ii][jj];
			if (count <= RLE_SYMS) {
				if (count == 1) {
					bitcount[ii] += codelens[ii][jj][0];
				} else {
					bitcount[ii] += codelens[ii][jj][254 + count];
				}
			} else {
				bitcount[ii] += codelens[ii][jj][255 + RLE_SYMS];
				bitcount[ii] += 4; //approx
			}
		}
	}

	CAT_WARN("main") << "Chaos metric R bytes: " << bitcount[0] / 8;
	CAT_WARN("main") << "Chaos metric G bytes: " << bitcount[1] / 8;
	CAT_WARN("main") << "Chaos metric B bytes: " << bitcount[2] / 8;

#if 1
		{
			CAT_WARN("main") << "Writing delta image file";

			// Convert to image:

			lodepng_encode_file("chaos.png", (const unsigned char*)&test[0], width, height, LCT_RGB, 8);
		}
#endif

}

int ImageFilterWriter::initFromRGBA(u8 *rgba, int width, int height, ImageMaskWriter &mask) {
	if (!init(width, height)) {
		return WE_BAD_DIMS;
	}

	decideFilters(rgba, width, height, mask);
	applyFilters(rgba, width, height, mask);
	chaosEncode(rgba, width, height, mask);

	vector<u8> reds, greens, blues, alphas;

	u8 *pixel = rgba;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			if (pixel[3] != 0) {
				reds.push_back(pixel[0]);
				greens.push_back(pixel[1]);
				blues.push_back(pixel[2]);
				alphas.push_back(pixel[3]);

				//cout << (int)pixel[3] << " ";
			}
			pixel += 4;
		}
	}

#if 1
	std::vector<u8> lz_reds, lz_greens, lz_blues, lz_alphas;
	lz_reds.resize(LZ4_compressBound(static_cast<int>( reds.size() )));
	lz_greens.resize(LZ4_compressBound(static_cast<int>( greens.size() )));
	lz_blues.resize(LZ4_compressBound(static_cast<int>( blues.size() )));
	lz_alphas.resize(LZ4_compressBound(static_cast<int>( alphas.size() )));

	lz_reds.resize(LZ4_compressHC((char*)&reds[0], (char*)&lz_reds[0], reds.size()));
	lz_greens.resize(LZ4_compressHC((char*)&greens[0], (char*)&lz_greens[0], greens.size()));
	lz_blues.resize(LZ4_compressHC((char*)&blues[0], (char*)&lz_blues[0], blues.size()));
	lz_alphas.resize(LZ4_compressHC((char*)&alphas[0], (char*)&lz_alphas[0], alphas.size()));
#else
#define lz_reds reds
#define lz_greens greens
#define lz_blues blues
#define lz_alphas alphas
#endif

	CAT_WARN("test") << "R bytes: " << lz_reds.size();
	CAT_WARN("test") << "G bytes: " << lz_greens.size();
	CAT_WARN("test") << "B bytes: " << lz_blues.size();
	CAT_WARN("test") << "A bytes: " << lz_alphas.size();

	u16 freq_reds[256], freq_greens[256], freq_blues[256], freq_alphas[256];

	collectFreqs(lz_reds, freq_reds);
	collectFreqs(lz_greens, freq_greens);
	collectFreqs(lz_blues, freq_blues);
	collectFreqs(lz_alphas, freq_alphas);

	u16 c_reds[256], c_greens[256], c_blues[256], c_alphas[256];
	u8 l_reds[256], l_greens[256], l_blues[256], l_alphas[256];
	generateHuffmanCodes(256, freq_reds, c_reds, l_reds);
	generateHuffmanCodes(256, freq_greens, c_greens, l_greens);
	generateHuffmanCodes(256, freq_blues, c_blues, l_blues);
	generateHuffmanCodes(256, freq_alphas, c_alphas, l_alphas);

	int bits_reds, bits_greens, bits_blues, bits_alphas;
	bits_reds = calcBits(lz_reds, l_reds);
	bits_greens = calcBits(lz_greens, l_greens);
	bits_blues = calcBits(lz_blues, l_blues);
	bits_alphas = calcBits(lz_alphas, l_alphas);

	CAT_WARN("test") << "Huffman-encoded R bytes: " << bits_reds / 8;
	CAT_WARN("test") << "Huffman-encoded G bytes: " << bits_greens / 8;
	CAT_WARN("test") << "Huffman-encoded B bytes: " << bits_blues / 8;
	CAT_WARN("test") << "Huffman-encoded A bytes: " << bits_alphas / 8;

	CAT_WARN("test") << "Estimated file size = " << (bits_reds + bits_greens + bits_blues + bits_alphas) / 8 + 6000 + 50000;

	return WE_OK;
}

