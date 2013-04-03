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

static CAT_INLINE int scoreYUV(YUV899 *yuv) {
	return (int)yuv->y + abs(yuv->u) + abs(yuv->v);
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


static CAT_INLINE u8 predLevel(int a, int b, int c) {
	if (c >= a && c >= b) {
		if (a > b) {
			return b;
		} else {
			return a;
		}
	} else if (c <= a && c <= b) {
		if (a > b) {
			return a;
		} else {
			return b;
		}
	} else {
		return b + a - c;
	}
}

static CAT_INLINE u8 abcClamp(int a, int b, int c) {
	int sum = a + b - c;
	if (sum < 0) {
		return 0;
	} else if (sum > 255) {
		return 255;
	} else {
		return sum;
	}
}

static CAT_INLINE u8 predABC(int a, int b, int c) {
	int abc = a + b - c;
	if (abc > 255) abc = 255;
	else if (abc < 0) abc = 0;
	return abc;
}

static CAT_INLINE u8 paeth(int a, int b, int c) {
	// Paeth filter
	int pabc = a + b - c;
	int pa = abs(pabc - a);
	int pb = abs(pabc - b);
	int pc = abs(pabc - c);

	if (pa <= pb && pa <= pc) {
		return (u8)a;
	} else if (pb <= pc) {
		return (u8)b;
	} else {
		return (u8)c;
	}
}

static CAT_INLINE u8 abc_paeth(int a, int b, int c) {
	// Paeth filter with modifications from BCIF
	int pabc = a + b - c;
	if (a <= c && c <= b) {
		return (u8)pabc;
	}

	int pa = abs(pabc - a);
	int pb = abs(pabc - b);
	int pc = abs(pabc - c);

	if (pa <= pb && pa <= pc) {
		return (u8)a;
	} else if (pb <= pc) {
		return (u8)b;
	} else {
		return (u8)c;
	}
}

static CAT_INLINE u8 predTest(int e, int c, int a) {
	return c + (c - e);
}

static const u8 *filterPixel(u8 *p, int sf, int x, int y, int width) {
	static const u8 FPZ[3] = {0};
	static u8 fpt[3]; // not thread-safe

	const u8 *fp = FPZ;

	switch (sf) {
		default:
		case SF_Z:			// 0
			break;

		case SF_TEST:
			if CAT_LIKELY(x > 1 && y > 1) {
				const u8 *c = p - 4 - width*4;
				const u8 *e = c - 4 - width*4;
				const u8 *a = p - 4;
				fp = fpt;

				fpt[0] = predTest(e[0], c[0], a[0]);
				fpt[1] = predTest(e[1], c[1], a[1]);
				fpt[2] = predTest(e[2], c[2], a[2]);
			} else {
				if CAT_LIKELY(x > 0) {
					fp = p - 4; // A
				} else if (y > 0) {
					fp = p - width*4; // B
				}
			}
			break;

		case SF_A:			// A
			if CAT_LIKELY(x > 0) {
				fp = p - 4; // A
			} else if (y > 0) {
				fp = p - width*4; // B
			}
			break;

		case SF_B:			// B
			if CAT_LIKELY(y > 0) {
				fp = p - width*4; // B
			} else if (x > 0) {
				fp = p - 4; // A
			}
			break;

		case SF_C:			// C
			if CAT_LIKELY(x > 0) {
				if CAT_LIKELY(y > 0) {
					fp = p - width*4 - 4; // C
				} else {
					fp = p - 4; // A
				}
			} else if (y > 0) {
				fp = p - width*4; // B
			}
			break;

		case SF_D:			// D
			if CAT_LIKELY(y > 0) {
				fp = p - width*4; // B
				if CAT_LIKELY(x < width-1) {
					fp += 4; // D
				}
			} else if (x > 0) {
				fp = p - 4; // A
			}
			break;

		case SF_AB:			// (A + B)/2
			if CAT_LIKELY(x > 0) {
				const u8 *a = p - 4; // A

				if CAT_LIKELY(y > 0) {
					fp = fpt;
					const u8 *b = p - width*4; // B

					fpt[0] = (a[0] + (u16)b[0]) >> 1;
					fpt[1] = (a[1] + (u16)b[1]) >> 1;
					fpt[2] = (a[2] + (u16)b[2]) >> 1;
				} else {
					fp = a;
				}
			} else if (y > 0) {
				fp = p - width*4; // B
			}
			break;

		case SF_AD:			// (A + D)/2
			if CAT_LIKELY(y > 0) {
				if CAT_LIKELY(x > 0) {
					const u8 *a = p - 4; // A

					fp = fpt;
					const u8 *src = p - width*4; // B
					if CAT_LIKELY(x < width-1) {
						src += 4; // D
					}

					fpt[0] = (a[0] + (u16)src[0]) >> 1;
					fpt[1] = (a[1] + (u16)src[1]) >> 1;
					fpt[2] = (a[2] + (u16)src[2]) >> 1;
				} else {
					// Assume image is not really narrow
					fp = p - width*4 + 4; // D
				}
			} else if (x > 0) {
				fp = p - 4; // A
			}
			break;

		case SF_BD:			// (B + D)/2
			if CAT_LIKELY(y > 0) {
				fp = fpt;
				const u8 *b = p - width*4; // B
				const u8 *src = b; // B
				if CAT_LIKELY(x < width-1) {
					src += 4; // D
				}

				fpt[0] = (b[0] + (u16)src[0]) >> 1;
				fpt[1] = (b[1] + (u16)src[1]) >> 1;
				fpt[2] = (b[2] + (u16)src[2]) >> 1;
			} else if (x > 0) {
				fp = p - 4; // A
			}
			break;

		case SF_A_BC:		// A + (B - C)/2
			if CAT_LIKELY(x > 0) {
				const u8 *a = p - 4; // A

				if CAT_LIKELY(y > 0) {
					fp = fpt;
					const u8 *b = p - width*4; // B
					const u8 *c = b - 4; // C

					fpt[0] = a[0] + (b[0] - (int)c[0]) >> 1;
					fpt[1] = a[1] + (b[1] - (int)c[1]) >> 1;
					fpt[2] = a[2] + (b[2] - (int)c[2]) >> 1;
				} else {
					fp = a;
				}
			} else if (y > 0) {
				fp = p - width*4; // B
			}
			break;

		case SF_B_AC:		// B + (A - C)/2
			if CAT_LIKELY(x > 0) {
				const u8 *a = p - 4; // A

				if CAT_LIKELY(y > 0) {
					fp = fpt;
					const u8 *b = p - width*4; // B
					const u8 *c = b - 4; // C

					fpt[0] = b[0] + (a[0] - (int)c[0]) >> 1;
					fpt[1] = b[1] + (a[1] - (int)c[1]) >> 1;
					fpt[2] = b[2] + (a[2] - (int)c[2]) >> 1;
				} else {
					fp = a;
				}
			} else if (y > 0) {
				fp = p - width*4; // B
			}
			break;

		case SF_ABCD:		// (A + B + C + D + 1)/4
			if CAT_LIKELY(x > 0) {
				const u8 *a = p - 4; // A

				if CAT_LIKELY(y > 0) {
					fp = fpt;
					const u8 *b = p - width*4; // B
					const u8 *c = b - 4; // C

					const u8 *src = b; // B
					if CAT_LIKELY(x < width-1) {
						src += 4; // D
					}

					fpt[0] = (a[0] + (int)b[0] + c[0] + (int)src[0] + 1) >> 2;
					fpt[1] = (a[1] + (int)b[1] + c[1] + (int)src[1] + 1) >> 2;
					fpt[2] = (a[2] + (int)b[2] + c[2] + (int)src[2] + 1) >> 2;
				} else {
					fp = a;
				}
			} else if (y > 0) {
				// Assumes image is not really narrow
				fp = fpt;
				const u8 *b = p - width*4; // B
				const u8 *d = b + 4; // D

				fpt[0] = (b[0] + (u16)d[0]) >> 1;
				fpt[1] = (b[1] + (u16)d[1]) >> 1;
				fpt[2] = (b[2] + (u16)d[2]) >> 1;
			}
			break;

		case SF_ABC_CLAMP:	// A + B - C clamped to [0, 255]
			if CAT_LIKELY(x > 0) {
				const u8 *a = p - 4; // A

				if CAT_LIKELY(y > 0) {
					fp = fpt;
					const u8 *b = p - width*4; // B
					const u8 *c = b - 4; // C

					fpt[0] = abcClamp(a[0], b[0], c[0]);
					fpt[1] = abcClamp(a[1], b[1], c[1]);
					fpt[2] = abcClamp(a[2], b[2], c[2]);
				} else {
					fp = a;
				}
			} else if (y > 0) {
				fp = p - width*4; // B
			}
			break;

		case SF_PAETH:		// Paeth filter
			if CAT_LIKELY(x > 0) {
				const u8 *a = p - 4; // A

				if CAT_LIKELY(y > 0) {
					fp = fpt;
					const u8 *b = p - width*4; // B
					const u8 *c = b - 4; // C

					fpt[0] = paeth(a[0], b[0], c[0]);
					fpt[1] = paeth(a[1], b[1], c[1]);
					fpt[2] = paeth(a[2], b[2], c[2]);
				} else {
					fp = a;
				}
			} else if (y > 0) {
				fp = p - width*4; // B
			}
			break;

		case SF_ABC_PAETH:	// If A <= C <= B, A + B - C, else Paeth filter
			if CAT_LIKELY(x > 0) {
				const u8 *a = p - 4; // A

				if CAT_LIKELY(y > 0) {
					fp = fpt;
					const u8 *b = p - width*4; // B
					const u8 *c = b - 4; // C

					fpt[0] = abc_paeth(a[0], b[0], c[0]);
					fpt[1] = abc_paeth(a[1], b[1], c[1]);
					fpt[2] = abc_paeth(a[2], b[2], c[2]);
				} else {
					fp = a;
				}
			} else if (y > 0) {
				fp = p - width*4; // B
			}
			break;

		case SF_PL:			// Use ABC to determine if increasing or decreasing
			if CAT_LIKELY(x > 0) {
				const u8 *a = p - 4; // A

				if CAT_LIKELY(y > 0) {
					fp = fpt;
					const u8 *b = p - width*4; // B
					const u8 *c = b - 4; // C

					fpt[0] = predLevel(a[0], b[0], c[0]);
					fpt[1] = predLevel(a[1], b[1], c[1]);
					fpt[2] = predLevel(a[2], b[2], c[2]);
				} else {
					fp = a;
				}
			} else if (y > 0) {
				fp = p - width*4; // B
			}
			break;

		case SF_PLO:		// Offset PL
			if CAT_LIKELY(x > 0) {
				const u8 *a = p - 4; // A

				if CAT_LIKELY(y > 0) {
					fp = fpt;
					const u8 *b = p - width*4; // B

					const u8 *src = b; // B
					if CAT_LIKELY(x < width-1) {
						src += 4; // D
					}

					fpt[0] = predLevel(a[0], src[0], b[0]);
					fpt[1] = predLevel(a[1], src[1], b[1]);
					fpt[2] = predLevel(a[2], src[2], b[2]);
				} else {
					fp = a;
				}
			} else if (y > 0) {
				fp = p - width*4; // B
			}
			break;
	}

	return fp;
}



void convertRGBtoYUV(int cf, const u8 *rgb, YUV899 *out) {
	const int R = rgb[0];
	const int G = rgb[1];
	const int B = rgb[2];

	switch (cf) {
		case CF_E2:		// from the Strutz paper
			{
				out->y = static_cast<u8>( (G >> 1) + ((R + B) >> 2) );
				out->u = static_cast<s16>( B - ((R + G) >> 1) );
				out->v = static_cast<s16>( R - G );
			}
			break;

		case CF_E1:		// from the Strutz paper
			{
				out->y = static_cast<u8>( (G >> 1) + ((R + B) >> 2) );
				out->u = static_cast<s16>( B - ((R + G*3) >> 2) );
				out->v = static_cast<s16>( R - G );
			}
			break;

		case CF_E4:		// from the Strutz paper
			{
				out->y = static_cast<u8>( (G >> 1) + ((R + B) >> 2) );
				out->u = static_cast<s16>( R - ((B + G*3) >> 2) );
				out->v = static_cast<s16>( B - G );
			}
			break;


		case CF_YUVr:	// YUVr from JPEG2000
			{
				int Ur = B - G;
				int Vr = R - G;

				out->y = static_cast<u8>( G + ((Ur + Vr) >> 2) );
				out->u = static_cast<s16>( Ur );
				out->v = static_cast<s16>( Vr );
			}
			break;


		case CF_D8:		// from the Strutz paper
			{
				out->y = static_cast<u8>( R );
				out->u = static_cast<s16>( B - ((R + G) >> 1) );
				out->v = static_cast<s16>( G - R );
			}
			break;

		case CF_D9:		// from the Strutz paper
			{
				out->y = static_cast<u8>( R );
				out->u = static_cast<s16>( B - ((R + G*3) >> 2) );
				out->v = static_cast<s16>( G - R );
			}
			break;

		case CF_D14:		// from the Strutz paper
			{
				out->y = static_cast<u8>( R );
				out->u = static_cast<s16>( G - ((R + B) >> 1) );
				out->v = static_cast<s16>( B - R );
			}
			break;


		case CF_D10:		// from the Strutz paper
			{
				out->y = static_cast<u8>( B );
				out->u = static_cast<s16>( G - ((R + B*3) >> 2) );
				out->v = static_cast<s16>( R - B );
			}
			break;

		case CF_D11:		// from the Strutz paper
			{
				out->y = static_cast<u8>( B );
				out->u = static_cast<s16>( G - ((R + B) >> 1) );
				out->v = static_cast<s16>( R - B );
			}
			break;

		case CF_D12:		// from the Strutz paper
			{
				out->y = static_cast<u8>( B );
				out->u = static_cast<s16>( G - ((R*3 + B) >> 2) );
				out->v = static_cast<s16>( R - B );
			}
			break;

		case CF_D18:		// from the Strutz paper
			{
				out->y = static_cast<u8>( B );
				out->u = static_cast<s16>( R - ((G*3 + B) >> 2) );
				out->v = static_cast<s16>( G - B );
			}
			break;


		case CF_YCgCo_R:	// Malvar's YCgCo-R
			{
				int Co = R - B;
				int t = B + (Co >> 1);
				int Cg = G - t;

				out->y = static_cast<u8>( t + (Cg >> 1) );
				out->u = static_cast<s16>( Co );
				out->v = static_cast<s16>( Cg );
			}
			break;


		case CF_A3:		// from the Strutz paper
			{
				out->y = static_cast<u8>( (R + G + B) / 3 );
				out->u = static_cast<s16>( B - G );
				out->v = static_cast<s16>( R - G );
			}
			break;


		case CF_LOCO_I:		// recommendation from LOCO-I paper (also used in BCIF)
			{
				out->y = static_cast<u8>( G );
				out->u = static_cast<s16>( B - G );
				out->v = static_cast<s16>( R - G );
			}
			break;



		default:
		case CF_RGB:		// Original RGB
			{
				out->y = G;
				out->u = B;
				out->v = R;
			}
			break;


		case CF_C7:		// from the Strutz paper
			{
				out->y = static_cast<u8>( B );
				out->u = static_cast<s16>( B - ((R + G) >> 1) );
				out->v = static_cast<s16>( R - G );
			}
			break;

		case CF_E5:		// from the Strutz paper
			{
				out->y = static_cast<u8>( (G >> 1) + ((R + B) >> 2) );
				out->u = static_cast<s16>( R - ((G + B) >> 1) );
				out->v = static_cast<s16>( G - B );
			}
			break;

		case CF_E8:		// from the Strutz paper
			{
				out->y = static_cast<u8>( (R >> 1) + ((G + B) >> 2) );
				out->u = static_cast<s16>( B - ((R + G) >> 1) );
				out->v = static_cast<s16>( G - R );
			}
			break;

		case CF_E11:		// from the Strutz paper
			{
				out->y = static_cast<u8>( (B >> 1) + ((R + G) >> 2) );
				out->u = static_cast<s16>( G - ((R + B) >> 1) );
				out->v = static_cast<s16>( R - B );
			}
			break;

		case CF_F1:		// from the Strutz paper
			{
				out->y = static_cast<u8>( (R + G + B) / 3 );
				out->u = static_cast<s16>( B - ((R + 3*G) >> 2) );
				out->v = static_cast<s16>( R - G );
			}
			break;

		case CF_F2:		// from the Strutz paper
			{
				out->y = static_cast<u8>( (R + G + B) / 3 );
				out->u = static_cast<s16>( R - ((B + 3*G) >> 2) );
				out->v = static_cast<s16>( B - G );
			}
			break;
	}
}



#include <cmath>

class EntropyEstimator {
	static const int NUM_SYMS = 512;

	u32 global[NUM_SYMS];
	u32 globalTotal;

	u8 best[NUM_SYMS];
	u32 bestTotal;

	u8 local[NUM_SYMS];
	u32 localTotal;

public:
	CAT_INLINE EntropyEstimator() {
	}
	CAT_INLINE virtual ~EntropyEstimator() {
	}

	void clear() {
		globalTotal = 0;
		CAT_OBJCLR(global);
	}

	void setup() {
		localTotal = 0;
		CAT_OBJCLR(local);
	}

	void push(s16 symbol) {
		local[symbol]++;
		++localTotal;
	}

	double entropy() {
		double total = globalTotal + localTotal;
		double e = 0;
		double log2 = log(2.);

		for (int ii = 0; ii < NUM_SYMS; ++ii) {
			u32 count = global[ii] + local[ii];
			if (count > 0) {
				double freq = count / total;
				e -= freq * log(freq) / log2;
			}
		}

		return e;
	}

	void save() {
		memcpy(best, local, sizeof(best));
		bestTotal = localTotal;
	}

	void commit() {
		for (int ii = 0; ii < NUM_SYMS; ++ii) {
			global[ii] += best[ii];
		}
		globalTotal += bestTotal;
	}
};


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




class FilterScorer {
public:
	struct Score {
		int score;
		int index;
	};

protected:
	Score *_list;
	int _count;

	CAT_INLINE void swap(int a, int b) {
		Score temp = _list[a];
		_list[a] = _list[b];
		_list[b] = temp;
	}

	int partitionTop(int left, int right, int pivotIndex) {
		int pivotValue = _list[pivotIndex].score;

		// Move pivot to end
		swap(pivotIndex, right);

		int storeIndex = left;

		for (int ii = left; ii < right; ++ii) {
			if (_list[ii].score < pivotValue) {
				swap(storeIndex, ii);

				++storeIndex;
			}
		}

		// Move pivot to its final place
		swap(right, storeIndex);

		return storeIndex;
	}

	void clear() {
		if (_list) {
			delete []_list;
			_list = 0;
		}
	}

public:
	CAT_INLINE FilterScorer() {
	}
	CAT_INLINE virtual ~FilterScorer() {
		clear();
	}

	void init(int count) {
		clear();

		_list = new Score[count];
		_count = count;
	}

	void reset() {
		for (int ii = 0, count = _count; ii < count; ++ii) {
			_list[ii].score = 0;
			_list[ii].index = ii;
		}
	}

	CAT_INLINE void add(int index, int error) {
		_list[index].score += error;
	}

	Score *getLowest() {
		Score *lowest = _list;
		int lowestScore = lowest->score;

		for (int ii = 1; ii < _count; ++ii) {
			int score = _list[ii].score;

			if (lowestScore > score) {
				lowestScore = score;
				lowest = _list + ii;
			}
		}

		return lowest;
	}

	Score *getTop(int k) {
		if (k > _count) {
			k = _count;
		}

		int pivotIndex = k;
		int left = 0;
		int right = _count - 1;

		for (;;) {
			int pivotNewIndex = partitionTop(left, right, pivotIndex);

			int pivotDist = pivotNewIndex - left + 1;
			if (pivotDist == k) {
				return _list;
			} else if (k < pivotDist) {
				right = pivotNewIndex - 1;
			} else {
				k -= pivotDist;
				left = pivotNewIndex + 1;
			}
		}
	}
};





void ImageFilterWriter::decideFilters(u8 *rgba, int width, int height, ImageMaskWriter &mask) {
	u16 *filterWriter = _matrix;

	static const int FSZ = FILTER_ZONE_SIZE;

	EntropyEstimator ee[3];
	ee[0].clear();
	ee[1].clear();
	ee[2].clear();

	FilterScorer scores;
	scores.init(SF_COUNT * CF_COUNT);

	int compressLevel = 1;

	for (int y = 0; y < height; y += FSZ) {
		for (int x = 0; x < width; x += FSZ) {

			// Determine best filter combination to use
			int bestSF = 0, bestCF = 0;

			// Lower compression level that is a lot faster:
			if (compressLevel == 0) {
				int predErrors[SF_COUNT*CF_COUNT] = {0};

				// For each pixel in the 8x8 zone,
				for (int yy = 0; yy < FSZ; ++yy) {
					for (int xx = 0; xx < FSZ; ++xx) {
						int px = x + xx, py = y + yy;
						if (mask.hasRGB(px, py)) {
							continue;
						}

						u8 *p = rgba + (px + py * width) * 4;

						for (int ii = 0; ii < SF_COUNT; ++ii) {
							const u8 *pred = filterPixel(p, ii, px, py, width);

							for (int jj = 0; jj < CF_COUNT; ++jj) {
								YUV899 yuv;
								convertRGBtoYUV(jj, pred, &yuv);

								predErrors[ii + SF_COUNT*jj] += scoreYUV(&yuv);
							}
						}
					}
				}

				// Find lowest error filter
				int lowestSum = predErrors[0];
				int best = 0;

				for (int ii = 1; ii < SF_COUNT*CF_COUNT; ++ii) {
					if (predErrors[ii] < lowestSum) {
						lowestSum = predErrors[ii];
						best = ii;
					}
				}

				// Write it out
				bestSF = best % SF_COUNT;
				bestCF = best / SF_COUNT;

			} else { // Higher compression level that uses entropy estimate:

				scores.reset();

				// For each pixel in the 8x8 zone,
				for (int yy = 0; yy < FSZ; ++yy) {
					for (int xx = 0; xx < FSZ; ++xx) {
						int px = x + xx, py = y + yy;
						if (mask.hasRGB(px, py)) {
							continue;
						}

						u8 *p = rgba + (px + py * width) * 4;

						for (int ii = 0; ii < SF_COUNT; ++ii) {
							const u8 *pred = filterPixel(p, ii, px, py, width);

							for (int jj = 0; jj < CF_COUNT; ++jj) {
								YUV899 yuv;
								convertRGBtoYUV(jj, pred, &yuv);

								int error = scoreYUV(&yuv);

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
					const int TOP_COUNT = 16;

					FilterScorer::Score *top = scores.getTop(TOP_COUNT);

					double bestScore = 0;

					for (int ii = 0; ii < TOP_COUNT; ++ii) {
						// Write it out
						u8 sf = top[ii].index % SF_COUNT;
						u8 cf = top[ii].index / SF_COUNT;

						ee[0].setup();
						ee[1].setup();
						ee[2].setup();

						for (int yy = 0; yy < FSZ; ++yy) {
							for (int xx = 0; xx < FSZ; ++xx) {
								int px = x + xx, py = y + yy;
								if (mask.hasRGB(px, py)) {
									continue;
								}

								u8 *p = rgba + (px + py * width) * 4;
								const u8 *pred = filterPixel(p, sf, px, py, width);

								YUV899 yuv;
								convertRGBtoYUV(cf, pred, &yuv);

								ee[0].push(yuv.y);
								ee[1].push(yuv.u + 255); // translate from [-255..255] -> [0..510]
								ee[2].push(yuv.v + 255); // "
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

			u16 filter = ((u16)bestSF << 8) | bestCF;
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

	//		sf = SF_Z;
	//		cf = CF_NOOP;

			u8 *p = rgba + (x + y * width) * 4;
			const u8 *pred = filterPixel(p, sf, x, y, width);

			p[0] = pred[0];
			p[1] = pred[1];
			p[2] = pred[2];

			// Color filtering is currently done during encoding so we do not have to allocate YUV memory

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
			if ((y % FSZ) == 0 && (x % FSZ) == 0) {
				rgba[(x + y * width) * 4] = 255;
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



//#define ADAPTIVE_ZRLE
#define USE_AZ


class EntropyEncoder {
	static const int BZ_SYMS = 256 + FILTER_RLE_SYMS;
#ifdef USE_AZ
	static const int AZ_SYMS = 256;
#endif

	u32 histBZ[BZ_SYMS], maxBZ;
#ifdef USE_AZ
	u32 histAZ[AZ_SYMS], maxAZ;
#endif
	u32 zeroRun;

#ifdef ADAPTIVE_ZRLE
	u32 zeros, total;
	bool usingZ;
#endif

	u16 codesBZ[BZ_SYMS];
	u8 codelensBZ[BZ_SYMS];

#ifdef USE_AZ
	u16 codesAZ[AZ_SYMS];
	u8 codelensAZ[AZ_SYMS];
#endif

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
		maxBZ = 0;
#ifdef USE_AZ
		CAT_OBJCLR(histAZ);
		maxAZ = 0;
#endif
		zeroRun = 0;
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
#ifdef USE_AZ
				histAZ[symbol]++;
#else
				histBZ[symbol]++;
#endif
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

		u16 freqBZ[BZ_SYMS];
#ifdef USE_AZ
		u16 freqAZ[AZ_SYMS];
#endif

#ifdef ADAPTIVE_ZRLE
		if (zeros * 100 / total >= 15) {
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

#ifdef USE_AZ
		normalizeFreqs(maxAZ, AZ_SYMS, histAZ, freqAZ);
		generateHuffmanCodes(AZ_SYMS, freqAZ, codesAZ, codelensAZ);
#endif
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
#ifdef USE_AZ
					bits += codelensAZ[symbol];
#else
					bits += codelensBZ[symbol];
#endif
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






bool convert(int r, int g, int b) {
	char Co = r - b;
	int t = b + (Co >> 1);
	char Cg = g - t;
	char Y = t + (Cg >> 1);

	int s = Y - (Cg >> 1);
	u8 G = Cg + s;
	u8 B = s - (Co >> 1);
	u8 R = B + Co;

	//cout << "(" << r << "," << g << "," << b << ") -> " << (int)Co << ", " << (int)Cg << ", " << (int) Y << endl;

	if (r == R && g == G && b == B) {
		return true;
	} else {
		cout << "Fails for " << r << " , " << g << " , " << b << endl;
		return false;
	}
}



void ImageFilterWriter::chaosEncode(u8 *rgba, int width, int height, ImageMaskWriter &mask) {
#ifdef GENERATE_CHAOS_TABLE
	GenerateChaosTable();
#endif

#if 1
	for (int r = 0; r < 256; ++r) {
		for (int g = 0; g < 256; ++g) {
			for (int b = 0; b < 256; ++b) {
				if (!convert(r, g, b))
					break;
			}
		}
	}
#endif

	int bitcount[3] = {0};

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
				test.push_back(chaos[1] * 256/8);
				test.push_back(chaos[1] * 256/8);
				test.push_back(chaos[1] * 256/8);

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

	for (int ii = 0; ii < 3; ++ii) {
		for (int jj = 0; jj < 16; ++jj) {
			encoder[ii][jj].finalize();
		}
	}

	CAT_CLR(last_chaos, width * 3 + 3);

	last = rgba;
	now = rgba;

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



class EntropyEstimator2 {
	u32 global[512];
	u32 globalTotal;

	u8 best[512];
	u32 bestTotal;

	u8 local[512];
	u32 localTotal;

public:
	CAT_INLINE EntropyEstimator2() {
	}
	CAT_INLINE virtual ~EntropyEstimator2() {
	}

	void clear() {
		globalTotal = 0;
		CAT_OBJCLR(global);
	}

	void setup() {
		localTotal = 0;
		CAT_OBJCLR(local);
	}

	void push(int symbol) {
		local[symbol]++;
		++localTotal;
	}

	double entropy() {
		double total = globalTotal + localTotal;
		double e = 0;
		double log2 = log(2.);

		//cout << endl; cout << endl;
		for (int ii = 0; ii < 512; ++ii) {
			u32 count = global[ii] + local[ii];
			//cout << ii << " : " << count << endl;
			if (count > 0) {
				double freq = count / total;
				e -= freq * log(freq) / log2;
			}
		}

		return e;
	}

	void save() {
		memcpy(best, local, sizeof(best));
		bestTotal = localTotal;
	}

	void commit() {
		for (int ii = 0; ii < 512; ++ii) {
			global[ii] += best[ii];
		}
		globalTotal += bestTotal;
	}
};




void ImageFilterWriter::colorSpace(u8 *rgba, int width, int height, ImageMaskWriter &mask) {
	for (int cf = 0; cf < CF_COUNT; ++cf) {
		EntropyEstimator2 ee[3];
		for (int ii = 0; ii < 3; ++ii) {
			ee[ii].clear();
			ee[ii].setup();
		}

		u8 *p = rgba;
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				YUV899 yuv;
				convertRGBtoYUV(cf, p, &yuv);

				ee[0].push(yuv.y);
				ee[1].push((u8)yuv.u);
				ee[2].push((u8)yuv.v);

				p += 4;
			}
		}

		double e[3], score = 0;
		for (int ii = 0; ii < 3; ++ii) {
			e[ii] = ee[ii].entropy();
			score += e[ii];
		}

		cout << "Entropy YUV(" << cf << ") = { " << e[0] << ", " << e[1] << ", " << e[2] << " } : SCORE=" << score << endl;
	}
}

int ImageFilterWriter::initFromRGBA(u8 *rgba, int width, int height, ImageMaskWriter &mask) {
	if (!init(width, height)) {
		return WE_BAD_DIMS;
	}

	colorSpace(rgba, width, height, mask);
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

