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



void convertRGBtoYUV(int cf, const u8 rgb[3], u8 out[3]) {
	const u8 R = rgb[0];
	const u8 G = rgb[1];
	const u8 B = rgb[2];
	u8 Y, U, V;

	switch (cf) {
		case CF_YUVr:	// YUVr from JPEG2000
			{
				U = B - G;
				V = R - G;
				Y = G + (((char)U + (char)V) >> 2);
			}
			break;


		case CF_YCgCo_R:	// Malvar's YCgCo-R
			{
				char Co = R - B;
				int t = B + (Co >> 1);
				char Cg = G - t;
				Y = t + (Cg >> 1);

				U = Cg;
				V = Co;
			}
			break;


		case CF_E2:		// from the Strutz paper
			{
				Y = ((char)G >> 1) + (((char)R + (char)B) >> 2);
				U = B - (((char)R + (char)G) >> 1);
				V = R - G;
			}
			break;

		case CF_E1:		// from the Strutz paper
			{
				Y = (G >> 1) + ((R + B) >> 2);
				U = B - ((R + G*3) >> 2);
				V = R - G;
			}
			break;

		case CF_E4:		// from the Strutz paper
			{
				Y = (G >> 1) + ((R + B) >> 2);
				U = R - ((B + G*3) >> 2);
				V = B - G;
			}
			break;


		case CF_D8:		// from the Strutz paper
			{
				Y = R;
				U = B - ((R + G) >> 1);
				V = G - R;
			}
			break;

		case CF_D9:		// from the Strutz paper
			{
				Y = R;
				U = B - ((R + G*3) >> 2);
				V = G - R;
			}
			break;

		case CF_D14:		// from the Strutz paper
			{
				Y = R;
				U = G - ((R + B) >> 1);
				V = B - R;
			}
			break;


		case CF_D10:		// from the Strutz paper
			{
				Y = B;
				U = G - ((R + B*3) >> 2);
				V = R - B;
			}
			break;

		case CF_D11:		// from the Strutz paper
			{
				Y = B;
				U = G - ((R + B) >> 1);
				V = R - B;
			}
			break;

		case CF_D12:		// from the Strutz paper
			{
				Y = B;
				U = G - ((R*3 + B) >> 2);
				V = R - B;
			}
			break;

		case CF_D18:		// from the Strutz paper
			{
				Y = B;
				U = R - ((G*3 + B) >> 2);
				V = G - B;
			}
			break;


		case CF_A3:		// from the Strutz paper
			{
				Y = (R + G + B) / 3;
				U = B - G;
				V = R - G;
			}
			break;


		case CF_GB_RG:
			{
				Y = B;
				U = G - B;
				V = G - R;
			}
			break;

		case CF_GB_RB:
			{
				Y = B;
				U = G - B;
				V = R - B;
			}
			break;

		case CF_GR_BR:
			{
				Y = B - R;
				U = G - R;
				V = R;
			}
			break;

		case CF_GR_BG:
			{
				Y = G - B;
				U = G - R;
				V = R;
			}
			break;

		case CF_BG_RG:
			{
				Y = G - B;
				U = G;
				V = G - R;
			}
			break;


		default:
		case CF_BGR:		// RGB -> BGR
			{
				Y = B;
				U = G;
				V = R;
			}
			break;


		case CF_C7:		// from the Strutz paper
			{
				Y = B;
				U = B - ((R + G) >> 1);
				V = R - G;
			}
			break;

		case CF_E5:		// from the Strutz paper
			{
				Y = (G >> 1) + ((R + B) >> 2);
				U = R - ((G + B) >> 1);
				V = G - B;
			}
			break;

		case CF_E8:		// from the Strutz paper
			{
				Y = (R >> 1) + ((G + B) >> 2);
				U = B - ((R + G) >> 1);
				V = G - R;
			}
			break;

		case CF_E11:		// from the Strutz paper
			{
				Y = (B >> 1) + ((R + G) >> 2);
				U = G - ((R + B) >> 1);
				V = R - B;
			}
			break;

		case CF_F1:		// from the Strutz paper
			{
				Y = (R + G + B) / 3;
				U = B - ((R + 3*G) >> 2);
				V = R - G;
			}
			break;

		case CF_F2:		// from the Strutz paper
			{
				Y = (R + G + B) / 3;
				U = R - ((B + 3*G) >> 2);
				V = B - G;
			}
			break;
	}

	out[0] = Y;
	out[1] = U;
	out[2] = V;
}

void convertYUVtoRGB(int cf, const u8 yuv[3], u8 out[3]) {
	const u8 Y = yuv[0];
	const u8 U = yuv[1];
	const u8 V = yuv[2];
	u8 R, G, B;

	// 0.625 = 5/8
	// 0.375 = 3/8
	// 0.0625 = 1/16

	switch (cf) {
		case CF_YUVr:	// YUVr from JPEG2000
			{
				G = Y - (((char)U + (char)V) >> 2);
				R = V + G;
				B = U + G;
			}
			break;


		case CF_YCgCo_R:	// Malvar's YCgCo-R
			{
				char Co = V;
				char Cg = U;

				const int t = Y - (Cg >> 1);
				G = Cg + t;
				B = t - (Co >> 1);
				R = Co + B;

			}
			break;


		case CF_E2:		// from the Strutz paper
			{
/*				Y = (G >> 1) + ((R + B) >> 2);
				U = B - ((R + G) >> 1);
				V = R - G;*/

				/*
				 * PLU decomposition:
				 *
				 * 0 0 1
				 * 0 1 0 = P
				 * 1 0 0
				 *
				 *     1     0 0
				 *  -0.5     1 0 = L
				 *  0.25 -0.75 1
				 *
				 *  1 -1 0
				 *  0 -1 1 = U
				 *  0  0 1
				 *
				 * Inv(E2) = Inv(U) * Inv(L) * Trans(P)
				 *
				 * P = Trans(P) => swap Y an V
				 *
				 *     1    0 0
				 *   0.5    1 0 = Inv(L)
				 * 0.125 0.75 1
				 *
				 * 1 -1 1
				 * 0 -1 1 = Inv(U)
				 * 0  0 1
				 */

				// TODO

				//cout << (int)Y << ", " << (int)U << ", " << (int)V << endl;

				u8 ly = V;
				u8 lu = (((char)V) >> 1) + U;
				u8 lv = (((char)V) >> 3) + ((3 * (char)U + 3) >> 2) + Y;

				//cout << (int)ly << ", " << (int)lu << ", " << (int)lv << endl;

				R = ly - lu + lv;
				G = lv - lu;
				B = lv;
			}
			break;

		case CF_E1:		// from the Strutz paper
			{
				/*
				 * YUV = E1 * RGB
				 * RGB = Inv(E1) * YUV
				 *
				 *  0.25   0.5  0.25
				 * -0.25 -0.75     1 = E1
				 *     1    -1     0
				 *
				 * E1 = PLU (LU Decomposition)
				 *
				 * 0 0 1
				 * 0 1 0 = P
				 * 1 0 0
				 *
				 * 1         0   0
				 * -0.25     1   0 = L
				 *  0.25 -0.75   1
				 *
				 * 1 -1 0
				 * 0 -1 1 = U
				 * 0 0  1
				 *
				 * Inv(E1) = Inv(U) * Inv(L) * Trans(P)
				 *
				 * 1 -1 1
				 * 0 -1 1 = Inv(U)
				 * 0 0  1
				 *
				 * 1          0 0
				 * 0.25       1 0 = Inv(L)
				 * -0.0625 0.75 1
				 *
				 * Trans(P) = P
				 *
				 * RGB = Inv(U) * Inv(L) * Trans(P) * YUV
				 */

				// TODO

				// x P
				int py = V;
				int pu = U;
				int pv = Y;

				// x Inv(L)
				int ly = py;
				int lu = py/4 + pu;
				int lv = pv + pu*3/4 - py/16;

				// x Inv(U)
				int uy = ly - lu + lv;
				int uu = lv - lu;
				int uv = lv;

				R = uy;
				G = uu;
				B = uv;
			}
			break;

		case CF_E4:		// from the Strutz paper
			{
/*				Y = (G >> 1) + ((R + B) >> 2);
				U = R - ((B + G*3) >> 2);
				V = B - G;*/
			}
			break;


		case CF_D8:		// from the Strutz paper
			{
				R = Y;
				G = V + R;
				B = U + (((u8)R + (u8)G) >> 1);
			}
			break;

		case CF_D9:		// from the Strutz paper
			{
				R = Y;
				G = V + R;
				B = U + (((u8)R + (u8)G*3) >> 2);
			}
			break;

		case CF_D14:		// from the Strutz paper
			{
				R = Y;
				B = V + R;
				G = U + (((u8)R + (u8)B) >> 1);
			}
			break;


		case CF_D10:		// from the Strutz paper
			{
				B = Y;
				R = V + B;
				G = U + (((u8)R + (u8)B*3) >> 2);
			}
			break;

		case CF_D11:		// from the Strutz paper
			{
				B = Y;
				R = V + B;
				G = U + (((u8)R + (u8)B) >> 1);
			}
			break;

		case CF_D12:		// from the Strutz paper
			{
				B = Y;
				R = B + V;
				G = U + (((u8)R*3 + (u8)B) >> 2);
			}
			break;

		case CF_D18:		// from the Strutz paper
			{
				B = Y;
				G = V + B;
				R = U + (((u8)G*3 + (u8)B) >> 2);
			}
			break;


		case CF_A3:		// from the Strutz paper
			{
				G = (Y * 3 - U - V) / 3;
				R = V + G;
				B = U + G;
			}
			break;


		case CF_GB_RG:
			{
				B = Y;
				G = U + B;
				R = V + G;
			}
			break;

		case CF_GB_RB:
			{
				B = Y;
				G = U + B;
				R = V + B;
			}
			break;

		case CF_GR_BR:
			{
				R = V;
				B = Y + R;
				G = U + R;
			}
			break;

		case CF_GR_BG:
			{
				R = V;
				G = U + R;
				B = Y + G;
			}
			break;

		case CF_BG_RG:
			{
				G = U;
				B = Y + G;
				R = V + G;
			}
			break;


		default:
		case CF_BGR:		// Original RGB
			{
				R = V;
				G = U;
				B = Y;
			}
			break;


		case CF_C7:		// from the Strutz paper
			{
/*				Y = B;
				U = B - ((R + G) >> 1);
				V = R - G;*/

				B = Y;
				const char s = (B - U) << 1;
				R = (s + V + 1) >> 1; 
				G = R - V;
			}
			break;

		case CF_E5:		// from the Strutz paper
			{
/*				Y = (G >> 1) + ((R + B) >> 2);
				U = R - ((G + B) >> 1);
				V = G - B;*/
			}
			break;

		case CF_E8:		// from the Strutz paper
			{
/*				Y = (R >> 1) + ((G + B) >> 2);
				U = B - ((R + G) >> 1);
				V = G - R;*/
			}
			break;

		case CF_E11:		// from the Strutz paper
			{
/*				Y = (B >> 1) + ((R + G) >> 2);
				U = G - ((R + B) >> 1);
				V = R - B;*/
			}
			break;

		case CF_F1:		// from the Strutz paper
			{
/*				Y = (R + G + B) / 3;
				U = B - ((R + 3*G) >> 2);
				V = R - G;*/
			}
			break;

		case CF_F2:		// from the Strutz paper
			{
/*				Y = (R + G + B) / 3;
				U = R - ((B + 3*G) >> 2);
				V = B - G;*/
			}
			break;
	}

	out[0] = R;
	out[1] = G;
	out[2] = B;
}



static CAT_INLINE void filterColor(int cf, const u8 *p, const u8 *pred, u8 *out) {
	u8 r = p[0] - pred[0];
	u8 g = p[1] - pred[1];
	u8 b = p[2] - pred[2];

	u8 temp[3] = {r, g, b};

	convertRGBtoYUV(cf, temp, out);
}



const char *GetColorFilterString(int cf) {
	switch (cf) {
		case CF_YUVr:	// YUVr from JPEG2000
			return "YUVr";

		case CF_E2:		// from the Strutz paper
			 return "E2";
		case CF_E1:		// from the Strutz paper
			 return "E1";
		case CF_E4:		// from the Strutz paper
			 return "E4";

		case CF_D8:		// from the Strutz paper
			 return "D8";
		case CF_D9:		// from the Strutz paper
			 return "D9";
		case CF_D14:		// from the Strutz paper
			 return "D14";

		case CF_D10:		// from the Strutz paper
			 return "D10";
		case CF_D11:		// from the Strutz paper
			 return "D11";
		case CF_D12:		// from the Strutz paper
			 return "D12";
		case CF_D18:		// from the Strutz paper
			 return "D18";

		case CF_YCgCo_R:	// Malvar's YCgCo-R
			 return "YCgCo-R";

		case CF_A3:		// from the Strutz paper
			 return "A3";

		case CF_GB_RG:	// from BCIF
			 return "BCIF-GB-RG";
		case CF_GB_RB:	// from BCIF
			 return "BCIF-GB-RB";
		case CF_GR_BR:	// from BCIF
			 return "BCIF-GR-BR";
		case CF_GR_BG:	// from BCIF
			 return "BCIF-GR-BG";
		case CF_BG_RG:	// from BCIF (recommendation from LOCO-I paper)
			 return "BCIF-LOCO-I";

		case CF_BGR:		// RGB -> BGR
			 return "BGR";

		case CF_C7:		// from the Strutz paper
			 return "C7";
		case CF_E5:		// from the Strutz paper
			 return "E5";
		case CF_E8:		// from the Strutz paper
			 return "E8";
		case CF_E11:		// from the Strutz paper
			 return "E11";
		case CF_F1:		// from the Strutz paper
			 return "F1";
		case CF_F2:		// from the Strutz paper
			 return "F2";
	}

	return "Uknown";
}


void testColorFilters() {
	for (int cf = 0; cf < CF_COUNT; ++cf) {
retry:
		for (int r = 0; r < 256; ++r) {
			for (int g = 0; g < 256; ++g) {
				for (int b = 0; b < 256; ++b) {
					u8 yuv[3];
					u8 rgb[3] = {r, g, b};
					convertRGBtoYUV(cf, rgb, yuv);
					u8 rgb2[3];
					convertYUVtoRGB(cf, yuv, rgb2);

					if (rgb2[0] != r || rgb2[1] != g || rgb2[2] != b) {
						cout << "Color filter " << GetColorFilterString(cf) << " is lossy for " << r << "," << g << "," << b << " -> " << (int)rgb2[0] << "," << (int)rgb2[1] << "," << (int)rgb2[2] << endl;
						goto nextcf;
					}
				}
			}
		}

		cout << "Color filter " << GetColorFilterString(cf) << " is reversible with YUV888!" << endl;
nextcf:
		;
	}
}






#include <cmath>

template<class LType> class EntropyEstimator {
	int _num_syms;

	u32 *_global;
	u32 _globalTotal;

	LType *_best;
	u32 _bestTotal;

	LType *_local;
	u32 _localTotal;

	void cleanup() {
		if (_global) {
			delete []_global;
			_global = 0;
		}
		if (_best) {
			delete []_best;
			_best = 0;
		}
		if (_local) {
			delete []_local;
			_local = 0;
		}
	}

public:
	CAT_INLINE EntropyEstimator() {
		_global = 0;
		_best = 0;
		_local = 0;
	}
	CAT_INLINE virtual ~EntropyEstimator() {
		cleanup();
	}

	void clear(int num_syms) {
		cleanup();

		_num_syms = num_syms;
		_global = new u32[num_syms];
		_best = new LType[num_syms];
		_local = new LType[num_syms];

		_globalTotal = 0;
		CAT_CLR(_global, _num_syms * sizeof(u32));
	}

	void setup() {
		_localTotal = 0;
		CAT_CLR(_local, _num_syms * sizeof(_local[0]));
	}

	void push(s16 symbol) {
		_local[symbol]++;
		++_localTotal;
	}

	double entropy() {
		double total = _globalTotal + _localTotal;
		double e = 0;
		static const double log2 = log(2.);

		for (int ii = 0, num_syms = _num_syms; ii < num_syms; ++ii) {
			u32 count = _global[ii] + _local[ii];
			if (count > 0) {
				double freq = count / total;
				e -= freq * log(freq) / log2;
			}
		}

		return e;
	}

	void drawHistogram(u8 *rgba, int width) {
		double total = _globalTotal + _localTotal;
		double e = 0;
		static const double log2 = log(2.);

		for (int ii = 0, num_syms = _num_syms; ii < num_syms; ++ii) {
			u32 count = _global[ii] + _local[ii];
			double freq;
			if (count > 0) {
				freq = count / total;
				e -= freq * log(freq) / log2;
			} else {
				freq = 0;
			}

			int r, g, b;
			r = 255;
			g = 0;
			b = 0;

			if (ii > 127) g = 255;
			if (ii > 255) b = 255;

			int bar = 200 * freq;
			for (int jj = 0; jj < bar; ++jj) {
				rgba[ii * width * 4 + jj * 4] = r;
				rgba[ii * width * 4 + jj * 4 + 1] = g;
				rgba[ii * width * 4 + jj * 4 + 2] = b;
			}
			for (int jj = bar; jj < 200; ++jj) {
				rgba[ii * width * 4 + jj * 4] = 0;
				rgba[ii * width * 4 + jj * 4 + 1] = 0;
				rgba[ii * width * 4 + jj * 4 + 2] = 0;
			}
		}
	}

	void save() {
		memcpy(_best, _local, _num_syms * sizeof(_best[0]));
		_bestTotal = _localTotal;
	}

	void commit() {
		for (int ii = 0, num_syms = _num_syms; ii < num_syms; ++ii) {
			_global[ii] += _best[ii];
		}
		_globalTotal += _bestTotal;
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





void ImageFilterWriter::decideFilters() {
	u16 *filterWriter = _matrix;

	static const int FSZ = FILTER_ZONE_SIZE;

	EntropyEstimator<u8> ee[3];
	ee[0].clear(256);
	ee[1].clear(256);
	ee[2].clear(256);

	FilterScorer scores;
	scores.init(SF_COUNT * CF_COUNT);

	int compressLevel = 1;
	const int width = _width;

	for (int y = 0; y < _height; y += FSZ) {
		for (int x = 0; x < width; x += FSZ) {

			// Determine best filter combination to use
			int bestSF = 0, bestCF = 0;

			// Lower compression level that is a lot faster:
			if (compressLevel == 0) {
				scores.reset();

				// For each pixel in the 8x8 zone,
				for (int yy = 0; yy < FSZ; ++yy) {
					for (int xx = 0; xx < FSZ; ++xx) {
						int px = x + xx, py = y + yy;
						if (_mask->hasRGB(px, py) ||
							!hasPixel(px, py)) {
							continue;
						}

						// TODO: Try setting pixels to zero that are masked out

						u8 *p = _rgba + (px + py * width) * 4;

						for (int ii = 0; ii < SF_COUNT; ++ii) {
							const u8 *pred = filterPixel(p, ii, px, py, width);

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
				for (int yy = 0; yy < FSZ; ++yy) {
					for (int xx = 0; xx < FSZ; ++xx) {
						int px = x + xx, py = y + yy;
						if (_mask->hasRGB(px, py) ||
							!hasPixel(px, py)) {
							continue;
						}

						u8 *p = _rgba + (px + py * width) * 4;

						for (int ii = 0; ii < SF_COUNT; ++ii) {
							const u8 *pred = filterPixel(p, ii, px, py, width);

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

						for (int yy = 0; yy < FSZ; ++yy) {
							for (int xx = 0; xx < FSZ; ++xx) {
								int px = x + xx, py = y + yy;
								if (_mask->hasRGB(px, py) ||
									!hasPixel(px, py)) {
									continue;
								}

								u8 *p = _rgba + (px + py * width) * 4;
								const u8 *pred = filterPixel(p, sf, px, py, width);

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

			u16 filter = ((u16)bestSF << 8) | bestCF;
			setFilter(x, y, filter);
		}
	}
}

void ImageFilterWriter::applyFilters() {
	u16 *filterWriter = _matrix;
	const int width = _width;

	static const int FSZ = FILTER_ZONE_SIZE;

	// For each zone,
	for (int y = _height - 1; y >= 0; --y) {
		for (int x = width - 1; x >= 0; --x) {
			u16 filter = getFilter(x, y);
			u8 cf = (u8)filter;
			u8 sf = (u8)(filter >> 8);
			if (_mask->hasRGB(x, y) ||
				!hasPixel(x, y)) {
				continue;
			}

			u8 *p = _rgba + (x + y * width) * 4;
			const u8 *pred = filterPixel(p, sf, x, y, width);

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






void ImageFilterWriter::chaosEncode() {
#ifdef GENERATE_CHAOS_TABLE
	GenerateChaosTable();
#endif

	int bitcount[3] = {0};
	const int width = _width;

	u8 *last_chaos = _chaos;
	CAT_CLR(last_chaos, width * 3 + 3);

	u8 *last = _rgba;
	u8 *now = _rgba;

	vector<u8> test;

	EntropyEncoder encoder[3][16];

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
				test.push_back(chaos[1] * 256/8);
				test.push_back(chaos[1] * 256/8);
				test.push_back(chaos[1] * 256/8);

				if (!_mask->hasRGB(x, y)) {
					for (int ii = 0; ii < 3; ++ii) {
						if (_lz_mask[(x + y * _width) * 3 + ii]) {
							encoder[ii][chaos[ii]].push(now[ii]);
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
			encoder[ii][jj].finalize();
		}
	}

	CAT_CLR(last_chaos, width * 3 + 3);

	last = _rgba;
	now = _rgba;

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
						if (_lz_mask[(x + y * _width) * 3 + ii]) {
							bitcount[ii] += encoder[ii][chaos[ii]].encode(now[ii]);
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

			lodepng_encode_file("chaos.png", (const unsigned char*)&test[0], _width, _height, LCT_RGB, 8);
		}
#endif

}




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

		if (cf == CF_YCgCo_R) {
			ee[0].drawHistogram(rgba, width);
			ee[1].drawHistogram(rgba + 800, width);
			ee[2].drawHistogram(rgba + 1600, width);
			return;
		}

		double e[3], score = 0;
		for (int ii = 0; ii < 3; ++ii) {
			e[ii] = ee[ii].entropy();
			score += e[ii];
		}

		cout << "YUV888 Entropy for " << GetColorFilterString(cf) << " = { " << e[0] << ", " << e[1] << ", " << e[2] << " } : SCORE=" << score << endl;
	}
}



bool nextPixel(int &x, int &y, int &c, int width, int height) {
	++c;
	if (c >= 3) {
		c = 0;
		++x;
		if (x >= width) {
			x = 0;
			++y;
			if (y >= height) {
				return true;
			}
		}
	}

	return false;
}



void ImageFilterWriter::makeLZmask() {
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

	_lz.resize(LZ4_compressHC((char*)&lz_input[0], (char*)&_lz[0], lz_input.size()));

	cout << "LZ bytes = " << _lz.size() << endl;

	vector<u8> test;
	test.resize(lz_input.size());

	int result = LZ4_uncompress((char*)&_lz[0], (char*)&test[0], test.size());
	cout << result << endl;

	for (int ii = 0; ii < test.size(); ++ii) {
		if (test[ii] != lz_input[ii]) {
			cout << "FAIL at " << ii << endl;
			break;
		}
	}

	const int size = _lz.size();

	int moffset = 0;

	u8 *lzmask = _lz_mask;

	vector<u8> lz_overhead;

	for (int ii = 0; ii < size;) {
		// Read token
		u8 token = _lz[ii++];
		lz_overhead.push_back(token);

		// Read Literal Length
		int literalLength = token >> 4;
		if (literalLength == 15) {
			int s;
			do {
				s = _lz[ii++];
				lz_overhead.push_back(s);
				literalLength += s;
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

		// Read match length
		int matchLength = token & 15;
		if (matchLength == 15) {
			int s;
			do {
				s = _lz[ii++];
				lz_overhead.push_back(s);
				matchLength += s;
			} while (s == 255 && ii < size);
		}
		matchLength += 8;

		for (int jj = 0; jj < matchLength; ++jj) {
			while (_mask->hasRGB((moffset/3) % _width, (moffset/3) / _width)) {
				moffset += 3;
			}

			lzmask[moffset++] = 0;
		}
	}

	cout << endl;

	// NOTE: Do not need this after it works
	while (moffset < lz_mask_size &&
		   _mask->hasRGB((moffset/3) % _width, (moffset/3) / _width)) {
		moffset += 3;
	}

	if (moffset != 1024 * 1024 * 3) {
		cout << ">>> Offset does not match expectation.  There is no way this just worked!" << endl;
	}

	u16 freq[256];

	collectFreqs(lz_overhead, freq);

	u16 codes[256];
	u8 lens[256];
	generateHuffmanCodes(256, freq, codes, lens);

	int bits_overhead = calcBits(lz_overhead, lens);

	cout << "LZ overhead = " << bits_overhead/8 << endl;
}


int ImageFilterWriter::initFromRGBA(u8 *rgba, int width, int height, ImageMaskWriter &mask) {
	if (!init(width, height)) {
		return WE_BAD_DIMS;
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

	makeLZmask();
	decideFilters();
	applyFilters();
	chaosEncode();

	return WE_OK;
}

