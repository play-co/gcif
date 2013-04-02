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

	if (width < FILTER_ZONE_SIZE || height < FILTER_ZONE_SIZE) {
		return false;
	}

	if ((width % FILTER_ZONE_SIZE) != 0 || (height % FILTER_ZONE_SIZE) != 0) {
		return false;
	}

	_w = width / FILTER_ZONE_SIZE;
	_h = height / FILTER_ZONE_SIZE;
	_matrix = new u16[_w * _h];
	_chaos = new u8[width * 3 + 3];

	return true;
}

void ImageFilterWriter::decideFilters(u8 *rgba, int width, int height, ImageMaskWriter &mask) {
	u16 *filterWriter = _matrix;

	static const int FSZ = FILTER_ZONE_SIZE;

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

	static const int FSZ = FILTER_ZONE_SIZE;

	// For each zone,
	for (int y = height - 1; y >= 0; --y) {
		for (int x = width - 1; x >= 0; --x) {
			u16 filter = getFilter(x, y);
			u8 cf = (u8)filter;
			u8 sf = (u8)(filter >> 8);
			if (mask.hasRGB(x, y)) {
				continue;
			}

			u8 fp[3];

			for (int plane = 0; plane < 3; ++plane) {
				int a, c, b, d;

				// Grab ABCD
				if (x > 0) {
					if (y > 0) {
						a = rgba[((x-1) + y * width)*4 + plane];
						c = rgba[((x-1) + (y-1) * width)*4 + plane];
						b = rgba[(x + (y-1) * width)*4 + plane];
						if (x < width-1) {
							d = rgba[((x+1) + (y-1) * width)*4 + plane];
						} else {
							d = 0;
						}
					} else {
						a = rgba[((x-1) + y * width)*4 + plane];
						c = 0;
						b = 0;
						d = 0;
					}
				} else {
					if (y > 0) {
						a = 0;
						c = 0;
						b = rgba[(x + (y-1) * width)*4 + plane];
						if (x < width-1) {
							d = rgba[((x+1) + (y-1) * width)*4 + plane];
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

				fp[plane] = rgba[(x + y * width)*4 + plane] - pred;
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

			for (int ii = 0; ii < 3; ++ii) {
#if 1
				rgba[(x + y * width)*4 + ii] = fp[ii];
#else
				rgba[(x + y * width)*4 + ii] = score(fp[ii]) * 16;
#endif
			}

			if ((y % FSZ) == 0 && (x % FSZ) == 0) {
				if (sf == SF_B) {
					rgba[(x + y * width) * 4] = 255;
				}
			}
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



//#define ADAPTIVE_ZRLE


class EntropyEncoder {
	static const int BZ_SYMS = 256 + FILTER_RLE_SYMS;
	static const int AZ_SYMS = 256;

	u32 histBZ[BZ_SYMS], histAZ[AZ_SYMS];
	u32 zeroRun;
	u32 maxBZ, maxAZ;

#ifdef ADAPTIVE_ZRLE
	u32 zeros, total;
	bool usingZ;
#endif

	u16 codesBZ[BZ_SYMS];
	u8 codelensBZ[BZ_SYMS];

	u16 codesAZ[AZ_SYMS];
	u8 codelensAZ[AZ_SYMS];

	void endSymbols() {
		if (zeroRun > 0) {
			if (zeroRun < FILTER_RLE_SYMS) {
				histBZ[255 + zeroRun]++;
			} else {
				histBZ[BZ_SYMS - 1]++;
			}

			zeroRun = 0;
		}
	}

	void normalizeFreqs(u32 max_freq, int num_syms, u32 hist[], u16 freqs[]) {
		static const int MAX_FREQ = 0xffff;

		// Scale to fit in 16-bit frequency counter
		while (max_freq > MAX_FREQ) {
			// For each symbol,
			for (int ii = 0; ii < num_syms; ++ii) {
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
		for (int ii = 0; ii < num_syms; ++ii) {
			freqs[ii] = static_cast<u16>( hist[ii] );
		}
	}

public:
	CAT_INLINE EntropyEncoder() {
		reset();
	}
	CAT_INLINE virtual ~EntropyEncoder() {
	}

	void reset() {
		CAT_OBJCLR(histBZ);
		CAT_OBJCLR(histAZ);
		zeroRun = 0;
		maxBZ = 0;
		maxAZ = 0;
#ifdef ADAPTIVE_ZRLE
		zeros = 0;
		total = 0;
#endif
	}

	void push(u8 symbol) {
#ifdef ADAPTIVE_ZRLE
		++total;
#endif
		if (symbol == 0) {
			++zeroRun;
#ifdef ADAPTIVE_ZRLE
			++zeros;
#endif
		} else {
			if (zeroRun > 0) {
				if (zeroRun < FILTER_RLE_SYMS) {
					histBZ[255 + zeroRun]++;
				} else {
					histBZ[BZ_SYMS - 1]++;
				}

				zeroRun = 0;
				histAZ[symbol]++;
			} else {
				histBZ[symbol]++;
			}
		}
	}

	void finalize() {
#ifdef ADAPTIVE_ZRLE
		if (total == 0) {
			return;
		}
#endif

		endSymbols();

		u16 freqBZ[BZ_SYMS], freqAZ[AZ_SYMS];

#ifdef ADAPTIVE_ZRLE
		if (zeros * 100 / total >= 0) {
			usingZ = true;
#endif

			normalizeFreqs(maxBZ, BZ_SYMS, histBZ, freqBZ);
			generateHuffmanCodes(BZ_SYMS, freqBZ, codesBZ, codelensBZ);
#ifdef ADAPTIVE_ZRLE
		} else {
			usingZ = false;

			histAZ[0] = zeros;
			u32 maxAZ = 0;
			for (int ii = 1; ii < AZ_SYMS; ++ii) {
				histAZ[ii] += histBZ[ii];
			}
		}
#endif

		normalizeFreqs(maxAZ, AZ_SYMS, histAZ, freqAZ);
		generateHuffmanCodes(AZ_SYMS, freqAZ, codesAZ, codelensAZ);
	}

	u32 encode(u8 symbol) {
		u32 bits = 0;

#ifdef ADAPTIVE_ZRLE
		if (usingZ) {
#endif
			if (symbol == 0) {
				++zeroRun;
			} else {
				if (zeroRun > 0) {
					if (zeroRun < FILTER_RLE_SYMS) {
						bits = codelensBZ[255 + zeroRun];
					} else {
						bits = codelensBZ[BZ_SYMS - 1] + 4; // estimated
					}

					zeroRun = 0;
					bits += codelensAZ[symbol];
				} else {
					bits += codelensBZ[symbol];
				}
			}
#ifdef ADAPTIVE_ZRLE
		} else {
			bits += codelensAZ[symbol];
		}
#endif

		return bits;
	}

	u32 encodeFinalize() {
		u32 bits = 0;

#ifdef ADAPTIVE_ZRLE
		if (usingZ) {
#endif
			if (zeroRun > 0) {
				if (zeroRun < FILTER_RLE_SYMS) {
					bits = codelensBZ[255 + zeroRun];
				} else {
					bits = codelensBZ[BZ_SYMS - 1] + 4; // estimated
				}
			}
#ifdef ADAPTIVE_ZRLE
		}
#endif

		return bits;
	}
};








void ImageFilterWriter::chaosEncode(u8 *rgba, int width, int height, ImageMaskWriter &mask) {
#ifdef GENERATE_CHAOS_TABLE
	GenerateChaosTable();
#endif

	u8 *last_chaos = _chaos;
	CAT_CLR(last_chaos, width * 3 + 3);

	u8 *last = rgba;
	u8 *now = rgba;

	vector<u8> test;

	EntropyEncoder encoder[3][16];

	for (int y = 0; y < height; ++y) {
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
				test.push_back(chaos[0] * 256/8);
				test.push_back(chaos[0] * 256/8);
				test.push_back(chaos[0] * 256/8);

				if (!mask.hasRGB(x, y)) {
					for (int ii = 0; ii < 3; ++ii) {
						encoder[ii][chaos[ii]].push(now[ii]);
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

	CAT_CLR(last_chaos, width * 3 + 3);

	for (int ii = 0; ii < 3; ++ii) {
		for (int jj = 0; jj < 16; ++jj) {
			encoder[ii][jj].finalize();
		}
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
				if (!mask.hasRGB(x, y)) {
					for (int ii = 0; ii < 3; ++ii) {
						bitcount[ii] += encoder[ii][chaos[ii]].encode(now[ii]);
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
			bitcount[ii] += encoder[ii][jj].encodeFinalize();
		}
	}

	CAT_WARN("main") << "Chaos metric R bytes: " << bitcount[0] / 8;
	CAT_WARN("main") << "Chaos metric G bytes: " << bitcount[1] / 8;
	CAT_WARN("main") << "Chaos metric B bytes: " << bitcount[2] / 8;

	CAT_WARN("main") << "Estimated file size bytes: " << (bitcount[0] + bitcount[1] + bitcount[2]) / 8 + (3*8*100);

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
/*
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
*/
	return WE_OK;
}

