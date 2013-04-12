#include "Filters.hpp"
using namespace cat;


//// Spatial Filters

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
	int pa = AbsVal(pabc - a);
	int pb = AbsVal(pabc - b);
	int pc = AbsVal(pabc - c);

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

	int pa = AbsVal(pabc - a);
	int pb = AbsVal(pabc - b);
	int pc = AbsVal(pabc - c);

	if (pa <= pb && pa <= pc) {
		return (u8)a;
	} else if (pb <= pc) {
		return (u8)b;
	} else {
		return (u8)c;
	}
}

u8 leftSel(int f, int c, int a) {
	if (AbsVal(f - c) < AbsVal(f - a)) {
		return c;
	} else {
		return a;
	}
}

u8 threeSel(int f, int c, int b, int d) {
	int dc = AbsVal(f - c);
	int db = AbsVal(f - b);
	int dd = AbsVal(f - d);

	if (dc < db) {
		if (dc < dd) {
			return c;
		}
	} else {
		if (db < dd) {
			return b;
		}
	}

	return d;
}

const u8 *cat::spatialFilterPixel(const u8 *p, int sf, int x, int y, int width) {
	static const u8 FPZ[3] = {0};
	static u8 fpt[3]; // not thread-safe

	const u8 *fp = FPZ;

	switch (sf) {
		default:
		case SF_Z:			// 0
			break;

		case SF_PICK_LEFT:
			if CAT_LIKELY(x > 1 && y > 1 && x < width - 2) {
				const u8 *a = p - 4;
				const u8 *c = a - width*4;
				const u8 *f = c - 4;
				fp = fpt;

				fpt[0] = leftSel(f[0], c[0], a[0]);
				fpt[1] = leftSel(f[1], c[1], a[1]);
				fpt[2] = leftSel(f[2], c[2], a[2]);
			} else {
				if CAT_LIKELY(x > 0) {
					fp = p - 4; // A
				} else if (y > 0) {
					fp = p - width*4; // B
				}
			}
			break;

		case SF_PRED_UR:
			if CAT_LIKELY(y > 1 && x < width - 2) {
				const u8 *d = p + 4 - width*4;
				const u8 *e = d + 4 - width*4;
				fp = fpt;

				fpt[0] = d[0] * 2 - e[0];
				fpt[1] = d[1] * 2 - e[1];
				fpt[2] = d[2] * 2 - e[2];
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



//// Color Filters

void cat::convertRGBtoYUV(int cf, const u8 rgb[3], u8 out[3]) {
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


		case CF_E2_R:		// derived from E2 and YCgCo-R
			{
				char Co = R - G;
				int t = G + (Co >> 1);
				char Cg = B - t;

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
		case CF_B_GR_R:		// A decent default filter
			{
				Y = B;
				U = G - R;
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

void cat::convertYUVtoRGB(int cf, const u8 yuv[3], u8 out[3]) {
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

		case CF_E2_R:		// Derived from E2 and YCgCo-R
			{
				char Co = V;
				char Cg = U;
				const int t = Y - (Cg >> 1);

				B = Cg + t;
				G = t - (Co >> 1);
				R = Co + G;
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
				R = G - V;
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
				B = G - Y;
			}
			break;

		case CF_BG_RG:
			{
				G = U;
				B = G - Y;
				R = G - V;
			}
			break;


		default:
		case CF_B_GR_R:		// A decent default filter
			{
				R = V;
				G = U + R;
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

const char *GetColorFilterString(int cf) {
	switch (cf) {
		case CF_YUVr:	// YUVr from JPEG2000
			return "YUVr";

		case CF_E2_R:	// derived from E2 and YCgCo-R
			 return "E2-R";

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
			 return "BCIF-BG-RG";

		case CF_B_GR_R:		// RGB -> B, G-R, R
			 return "B_GR_R";

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

	return "Unknown";
}


#if TEST_COLOR_FILTERS

#include <iostream>
using namespace std;

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

#endif // TEST_COLOR_FILTERS


//// Chaos

const u8 cat::CHAOS_TABLE[512] = {
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

