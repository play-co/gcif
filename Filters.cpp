#include "Filters.hpp"
#include "Log.hpp"
using namespace cat;


//// Spatial Filters

static const u8 FPZ[3] = {0};
static u8 FPT[3]; // not thread-safe

static const u8 *SFF_Z(const u8 *p, int x, int y, int width) {
	return FPZ;
}

#define SFFU_Z SFF_Z

static const u8 *SFF_D(const u8 *p, int x, int y, int width) {
	if (y > 0) {
		const u8 *fp = p - width*4; // B
		if (x < width-1) {
			fp += 4; // D
		}
		return fp;
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_D(const u8 *p, int x, int y, int width) {
	CAT_DEBUG_ENFORCE(y > 0 && x < width-1);

	return p - width*4 + 4; // D
}

static const u8 *SFF_C(const u8 *p, int x, int y, int width) {
	if (x > 0) {
		if (y > 0) {
			return p - width*4 - 4; // C
		} else {
			return p - 4; // A
		}
	} else if (y > 0) {
		return p - width*4; // B
	}

	return FPZ;
}

static const u8 *SFFU_C(const u8 *p, int x, int y, int width) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0);

	return p - width*4 - 4; // C
}

static const u8 *SFF_B(const u8 *p, int x, int y, int width) {
	if (y > 0) {
		return p - width*4; // B
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_B(const u8 *p, int x, int y, int width) {
	CAT_DEBUG_ENFORCE(y > 0);

	return p - width*4; // B
}

static const u8 *SFF_A(const u8 *p, int x, int y, int width) {
	if (x > 0) {
		return p - 4; // A
	} else if (y > 0) {
		return p - width*4; // B
	}

	return FPZ;
}

static const u8 *SFFU_A(const u8 *p, int x, int y, int width) {
	CAT_DEBUG_ENFORCE(x > 0);

	return p - 4; // A
}

static const u8 *SFF_AB(const u8 *p, int x, int y, int width) {
	if (x > 0) {
		const u8 *a = p - 4; // A

		if (y > 0) {
			const u8 *b = p - width*4; // B

			FPT[0] = (a[0] + (u16)b[0]) >> 1;
			FPT[1] = (a[1] + (u16)b[1]) >> 1;
			FPT[2] = (a[2] + (u16)b[2]) >> 1;

			return FPT;
		} else {
			return a;
		}
	} else if (y > 0) {
		return p - width*4; // B
	}

	return FPZ;
}

static const u8 *SFFU_AB(const u8 *p, int x, int y, int width) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0);

	const u8 *a = p - 4; // A
	const u8 *b = p - width*4; // B

	FPT[0] = (a[0] + (u16)b[0]) >> 1;
	FPT[1] = (a[1] + (u16)b[1]) >> 1;
	FPT[2] = (a[2] + (u16)b[2]) >> 1;

	return FPT;
}

static const u8 *SFF_BD(const u8 *p, int x, int y, int width) {
	if (y > 0) {
		const u8 *b = p - width*4; // B
		const u8 *src = b; // B
		if (x < width-1) {
			src += 4; // D
		}

		FPT[0] = (b[0] + (u16)src[0]) >> 1;
		FPT[1] = (b[1] + (u16)src[1]) >> 1;
		FPT[2] = (b[2] + (u16)src[2]) >> 1;

		return FPT;
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_BD(const u8 *p, int x, int y, int width) {
	CAT_DEBUG_ENFORCE(y > 0 && x < width-1);

	const u8 *b = p - width*4; // B
	const u8 *src = b + 4; // D

	FPT[0] = (b[0] + (u16)src[0]) >> 1;
	FPT[1] = (b[1] + (u16)src[1]) >> 1;
	FPT[2] = (b[2] + (u16)src[2]) >> 1;

	return FPT;
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

static const u8 *SFF_ABC_CLAMP(const u8 *p, int x, int y, int width) {
	if (x > 0) {
		const u8 *a = p - 4; // A

		if (y > 0) {
			const u8 *b = p - width*4; // B
			const u8 *c = b - 4; // C

			FPT[0] = abcClamp(a[0], b[0], c[0]);
			FPT[1] = abcClamp(a[1], b[1], c[1]);
			FPT[2] = abcClamp(a[2], b[2], c[2]);

			return FPT;
		} else {
			return a;
		}
	} else if (y > 0) {
		return p - width*4; // B
	}

	return FPZ;
}

static const u8 *SFFU_ABC_CLAMP(const u8 *p, int x, int y, int width) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0);

	const u8 *a = p - 4; // A
	const u8 *b = p - width*4; // B
	const u8 *c = b - 4; // C

	FPT[0] = abcClamp(a[0], b[0], c[0]);
	FPT[1] = abcClamp(a[1], b[1], c[1]);
	FPT[2] = abcClamp(a[2], b[2], c[2]);

	return FPT;
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

static const u8 *SFF_PAETH(const u8 *p, int x, int y, int width) {
	if (x > 0) {
		const u8 *a = p - 4; // A

		if (y > 0) {
			const u8 *b = p - width*4; // B
			const u8 *c = b - 4; // C

			FPT[0] = paeth(a[0], b[0], c[0]);
			FPT[1] = paeth(a[1], b[1], c[1]);
			FPT[2] = paeth(a[2], b[2], c[2]);

			return FPT;
		} else {
			return a;
		}
	} else if (y > 0) {
		return p - width*4; // B
	}

	return FPZ;
}

static const u8 *SFFU_PAETH(const u8 *p, int x, int y, int width) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0);

	const u8 *a = p - 4; // A
	const u8 *b = p - width*4; // B
	const u8 *c = b - 4; // C

	FPT[0] = paeth(a[0], b[0], c[0]);
	FPT[1] = paeth(a[1], b[1], c[1]);
	FPT[2] = paeth(a[2], b[2], c[2]);

	return FPT;
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

static const u8 *SFF_ABC_PAETH(const u8 *p, int x, int y, int width) {
	if (x > 0) {
		const u8 *a = p - 4; // A

		if (y > 0) {
			const u8 *b = p - width*4; // B
			const u8 *c = b - 4; // C

			FPT[0] = abc_paeth(a[0], b[0], c[0]);
			FPT[1] = abc_paeth(a[1], b[1], c[1]);
			FPT[2] = abc_paeth(a[2], b[2], c[2]);

			return FPT;
		} else {
			return a;
		}
	} else if (y > 0) {
		return p - width*4; // B
	}

	return FPZ;
}

static const u8 *SFFU_ABC_PAETH(const u8 *p, int x, int y, int width) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0);

	const u8 *a = p - 4; // A
	const u8 *b = p - width*4; // B
	const u8 *c = b - 4; // C

	FPT[0] = abc_paeth(a[0], b[0], c[0]);
	FPT[1] = abc_paeth(a[1], b[1], c[1]);
	FPT[2] = abc_paeth(a[2], b[2], c[2]);

	return FPT;
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

static const u8 *SFF_PLO(const u8 *p, int x, int y, int width) {
	if (x > 0) {
		const u8 *a = p - 4; // A

		if (y > 0) {
			const u8 *b = p - width*4; // B

			const u8 *src = b; // B
			if (x < width-1) {
				src += 4; // D
			}

			FPT[0] = predLevel(a[0], src[0], b[0]);
			FPT[1] = predLevel(a[1], src[1], b[1]);
			FPT[2] = predLevel(a[2], src[2], b[2]);

			return FPT;
		} else {
			return a;
		}
	} else if (y > 0) {
		return p - width*4; // B
	}

	return FPZ;
}

static const u8 *SFFU_PLO(const u8 *p, int x, int y, int width) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < width-1);

	const u8 *a = p - 4; // A
	const u8 *b = p - width*4; // B
	const u8 *src = b + 4; // D

	FPT[0] = predLevel(a[0], src[0], b[0]);
	FPT[1] = predLevel(a[1], src[1], b[1]);
	FPT[2] = predLevel(a[2], src[2], b[2]);

	return FPT;
}

static const u8 *SFF_ABCD(const u8 *p, int x, int y, int width) {
	if (x > 0) {
		const u8 *a = p - 4; // A

		if (y > 0) {
			const u8 *b = p - width*4; // B
			const u8 *c = b - 4; // C

			const u8 *src = b; // B
			if (x < width-1) {
				src += 4; // D
			}

			FPT[0] = (a[0] + (int)b[0] + c[0] + (int)src[0] + 1) >> 2;
			FPT[1] = (a[1] + (int)b[1] + c[1] + (int)src[1] + 1) >> 2;
			FPT[2] = (a[2] + (int)b[2] + c[2] + (int)src[2] + 1) >> 2;

			return FPT;
		} else {
			return a;
		}
	} else if (y > 0) {
		// Assumes image is not really narrow
		const u8 *b = p - width*4; // B
		const u8 *d = b + 4; // D

		FPT[0] = (b[0] + (u16)d[0]) >> 1;
		FPT[1] = (b[1] + (u16)d[1]) >> 1;
		FPT[2] = (b[2] + (u16)d[2]) >> 1;

		return FPT;
	}

	return FPZ;
}

static const u8 *SFFU_ABCD(const u8 *p, int x, int y, int width) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < width-1);

	const u8 *a = p - 4; // A
	const u8 *b = p - width*4; // B
	const u8 *c = b - 4; // C
	const u8 *src = b + 4; // D

	FPT[0] = (a[0] + (int)b[0] + c[0] + (int)src[0] + 1) >> 2;
	FPT[1] = (a[1] + (int)b[1] + c[1] + (int)src[1] + 1) >> 2;
	FPT[2] = (a[2] + (int)b[2] + c[2] + (int)src[2] + 1) >> 2;

	return FPT;
}

static CAT_INLINE u8 leftSel(int f, int c, int a) {
	if (AbsVal(f - c) < AbsVal(f - a)) {
		return c;
	} else {
		return a;
	}
}

static const u8 *SFF_PICK_LEFT(const u8 *p, int x, int y, int width) {
	if (x > 1 && y > 0) {
		const u8 *a = p - 4;
		const u8 *c = a - width*4;
		const u8 *f = c - 4;

		FPT[0] = leftSel(f[0], c[0], a[0]);
		FPT[1] = leftSel(f[1], c[1], a[1]);
		FPT[2] = leftSel(f[2], c[2], a[2]);

		return FPT;
	}

	if (x > 0) {
		return p - 4; // A
	} else if (y > 0) {
		return p - width*4; // B
	}

	return FPZ;
}

static const u8 *SFFU_PICK_LEFT(const u8 *p, int x, int y, int width) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < width-1);

	if (x > 1) {
		const u8 *a = p - 4;
		const u8 *c = a - width*4;
		const u8 *f = c - 4;

		FPT[0] = leftSel(f[0], c[0], a[0]);
		FPT[1] = leftSel(f[1], c[1], a[1]);
		FPT[2] = leftSel(f[2], c[2], a[2]);

		return FPT;
	}

	return p - 4; // A
}

static const u8 *SFF_PRED_UR(const u8 *p, int x, int y, int width) {
	if (y > 1 && x < width - 2) {
		const u8 *d = p + 4 - width*4;
		const u8 *e = d + 4 - width*4;

		FPT[0] = d[0] * 2 - e[0];
		FPT[1] = d[1] * 2 - e[1];
		FPT[2] = d[2] * 2 - e[2];

		return FPT;
	}

	if (x > 0) {
		return p - 4; // A
	} else if (y > 0) {
		return p - width*4; // B
	}

	return FPZ;
}

#define SFFU_PRED_UR SFF_PRED_UR

static CAT_INLINE u8 clampGrad(int b, int a, int c) {
	int grad = (int)b + (int)a - (int)c;
	int lo = b;
	if (lo > a) {
		lo = a;
	}
	if (lo > c) {
		lo = c;
	}
	int hi = b;
	if (hi < a) {
		hi = a;
	}
	if (hi < c) {
		hi = c;
	}
	if (grad <= lo) {
		return lo;
	}
	if (grad >= hi) {
		return hi;
	}
	return grad;
}

static const u8 *SFF_CLAMP_GRAD(const u8 *p, int x, int y, int width) {
	if (y > 0) {
		if (x > 0) {
			const u8 *a = p - 4; // A
			const u8 *b = p - width*4; // B
			const u8 *c = b - 4; // C

			FPT[0] = clampGrad(b[0], a[0], c[0]);
			FPT[1] = clampGrad(b[1], a[1], c[1]);
			FPT[2] = clampGrad(b[2], a[2], c[2]);

			return FPT;
		} else {
			// Assume image is not really narrow
			return p - width*4 + 4; // D
		}
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_CLAMP_GRAD(const u8 *p, int x, int y, int width) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < width-1);

	const u8 *a = p - 4; // A
	const u8 *b = p - width*4; // B
	const u8 *c = b - 4; // C

	FPT[0] = clampGrad(b[0], a[0], c[0]);
	FPT[1] = clampGrad(b[1], a[1], c[1]);
	FPT[2] = clampGrad(b[2], a[2], c[2]);

	return FPT;
}

static u8 skewGrad(int b, int a, int c) {
	int pred = (3 * (b + a) - (c << 1)) >> 2;
	if (pred >= 255) {
		return 255;
	}
	if (pred <= 0) {
		return 0;
	}
	return pred;
}

static const u8 *SFF_SKEW_GRAD(const u8 *p, int x, int y, int width) {
	if (y > 0) {
		if (x > 0) {
			const u8 *a = p - 4; // A
			const u8 *b = p - width*4; // B
			const u8 *c = b - 4; // C

			FPT[0] = skewGrad(b[0], a[0], c[0]);
			FPT[1] = skewGrad(b[1], a[1], c[1]);
			FPT[2] = skewGrad(b[2], a[2], c[2]);

			return FPT;
		} else {
			// Assume image is not really narrow
			return p - width*4 + 4; // D
		}
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_SKEW_GRAD(const u8 *p, int x, int y, int width) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < width-1);

	const u8 *a = p - 4; // A
	const u8 *b = p - width*4; // B
	const u8 *c = b - 4; // C

	FPT[0] = skewGrad(b[0], a[0], c[0]);
	FPT[1] = skewGrad(b[1], a[1], c[1]);
	FPT[2] = skewGrad(b[2], a[2], c[2]);

	return FPT;
}

static const u8 *SFF_AD(const u8 *p, int x, int y, int width) {
	if (y > 0) {
		if (x > 0) {
			const u8 *a = p - 4; // A

			const u8 *src = p - width*4; // B
			if (x < width-1) {
				src += 4; // D
			}

			FPT[0] = (a[0] + (u16)src[0]) >> 1;
			FPT[1] = (a[1] + (u16)src[1]) >> 1;
			FPT[2] = (a[2] + (u16)src[2]) >> 1;

			return FPT;
		} else {
			// Assume image is not really narrow
			return p - width*4 + 4; // D
		}
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_AD(const u8 *p, int x, int y, int width) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < width-1);

	const u8 *a = p - 4; // A
	const u8 *src = p - width*4 + 4; // D

	FPT[0] = (a[0] + (u16)src[0]) >> 1;
	FPT[1] = (a[1] + (u16)src[1]) >> 1;
	FPT[2] = (a[2] + (u16)src[2]) >> 1;

	return FPT;
}

#if 0

static const u8 *SFF_A_BC(const u8 *p, int x, int y, int width) {
	if (x > 0) {
		const u8 *a = p - 4; // A

		if (y > 0) {
			const u8 *b = p - width*4; // B
			const u8 *c = b - 4; // C

			FPT[0] = a[0] + (b[0] - (int)c[0]) >> 1;
			FPT[1] = a[1] + (b[1] - (int)c[1]) >> 1;
			FPT[2] = a[2] + (b[2] - (int)c[2]) >> 1;

			return FPT;
		} else {
			return a;
		}
	} else if (y > 0) {
		return p - width*4; // B
	}

	return FPZ;
}

static const u8 *SFF_B_AC(const u8 *p, int x, int y, int width) {
	if (x > 0) {
		const u8 *a = p - 4; // A

		if (y > 0) {
			const u8 *b = p - width*4; // B
			const u8 *c = b - 4; // C

			FPT[0] = b[0] + (a[0] - (int)c[0]) >> 1;
			FPT[1] = b[1] + (a[1] - (int)c[1]) >> 1;
			FPT[2] = b[2] + (a[2] - (int)c[2]) >> 1;

			return FPT;
		} else {
			return a;
		}
	} else if (y > 0) {
		return p - width*4; // B
	}

	return FPZ;
}

static const u8 *SFF_PL(const u8 *p, int x, int y, int width) {
	if (x > 0) {
		const u8 *a = p - 4; // A

		if (y > 0) {
			const u8 *b = p - width*4; // B
			const u8 *c = b - 4; // C

			FPT[0] = predLevel(a[0], b[0], c[0]);
			FPT[1] = predLevel(a[1], b[1], c[1]);
			FPT[2] = predLevel(a[2], b[2], c[2]);

			return FPT;
		} else {
			return a;
		}
	} else if (y > 0) {
		return p - width*4; // B
	}

	return FPZ;
}

static const u8 *SFFU_PL(const u8 *p, int x, int y, int width) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0);

	const u8 *a = p - 4; // A
	const u8 *b = p - width*4; // B
	const u8 *c = b - 4; // C

	FPT[0] = predLevel(a[0], b[0], c[0]);
	FPT[1] = predLevel(a[1], b[1], c[1]);
	FPT[2] = predLevel(a[2], b[2], c[2]);

	return FPT;
}

#endif


static const SpatialFilterFunction DEF_SPATIAL_FILTERS[SF_COUNT] = {
	SFF_Z,
	SFF_D,
	SFF_C,
	SFF_B,
	SFF_A,
	SFF_AB,
	SFF_BD,
	SFF_CLAMP_GRAD,
	SFF_SKEW_GRAD,
	SFF_PICK_LEFT,
	SFF_PRED_UR,
	SFF_ABC_CLAMP,
	SFF_PAETH,
	SFF_ABC_PAETH,
	SFF_PLO,
	SFF_ABCD,
	SFF_AD,
};

static const SpatialFilterFunction DEF_UNSAFE_SPATIAL_FILTERS[SF_COUNT] = {
	SFFU_Z,
	SFFU_D,
	SFFU_C,
	SFFU_B,
	SFFU_A,
	SFFU_AB,
	SFFU_BD,
	SFFU_CLAMP_GRAD,
	SFFU_SKEW_GRAD,
	SFFU_PICK_LEFT,
	SFFU_PRED_UR,
	SFFU_ABC_CLAMP,
	SFFU_PAETH,
	SFFU_ABC_PAETH,
	SFFU_PLO,
	SFFU_ABCD,
	SFFU_AD,
};

SpatialFilterFunction cat::SPATIAL_FILTERS[SF_COUNT];
SpatialFilterFunction cat::UNSAFE_SPATIAL_FILTERS[SF_COUNT];


static int m_taps[SF_COUNT][4];

#define DEFINE_TAPS(TAP) \
	static const u8 *SFF_TAPS_ ## TAP (const u8 *p, int x, int y, int width) { \
		if (x > 0) { \
			const u8 *a = p - 4; /* A */ \
			if (y > 0) { \
				const u8 *b = p - width*4; /* B */ \
				const u8 *c = b - 4; /* C */ \
				const u8 *d = b; /* B */ \
				if (x < width-1) { \
					d += 4; /* D */ \
				} \
				const int ta = m_taps[TAP][0]; \
				const int tb = m_taps[TAP][1]; \
				const int tc = m_taps[TAP][2]; \
				const int td = m_taps[TAP][3]; \
				FPT[0] = (ta*a[0] + tb*b[0] + tc*c[0] + td*d[0]) / 2; \
				FPT[1] = (ta*a[1] + tb*b[1] + tc*c[1] + td*d[1]) / 2; \
				FPT[2] = (ta*a[2] + tb*b[2] + tc*c[2] + td*d[2]) / 2; \
				return FPT; \
			} else { \
				return a; \
			} \
		} else if (y > 0) { \
			return  p - width*4; /* B */ \
		} \
		return FPZ; \
	} \
	static const u8 *SFFU_TAPS_ ## TAP (const u8 *p, int x, int y, int width) { \
		CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < width-1); \
		const u8 *a = p - 4; \
		const u8 *b = p - width*4; \
		const u8 *c = b - 4; \
		const u8 *d = b + 4; \
		const int ta = m_taps[TAP][0]; \
		const int tb = m_taps[TAP][1]; \
		const int tc = m_taps[TAP][2]; \
		const int td = m_taps[TAP][3]; \
		FPT[0] = (ta*a[0] + tb*b[0] + tc*c[0] + td*d[0]) / 2; \
		FPT[1] = (ta*a[1] + tb*b[1] + tc*c[1] + td*d[1]) / 2; \
		FPT[2] = (ta*a[2] + tb*b[2] + tc*c[2] + td*d[2]) / 2; \
		return FPT; \
	}

DEFINE_TAPS(0);
DEFINE_TAPS(1);
DEFINE_TAPS(2);
DEFINE_TAPS(3);
DEFINE_TAPS(4);
DEFINE_TAPS(5);
DEFINE_TAPS(6);
DEFINE_TAPS(7);
DEFINE_TAPS(8);
DEFINE_TAPS(9);
DEFINE_TAPS(10);
DEFINE_TAPS(11);
DEFINE_TAPS(12);
DEFINE_TAPS(13);
DEFINE_TAPS(14);
DEFINE_TAPS(15);

static SpatialFilterFunction m_safeTapFunctions[SF_COUNT] = {
	SFF_TAPS_0,
	SFF_TAPS_1,
	SFF_TAPS_2,
	SFF_TAPS_3,
	SFF_TAPS_4,
	SFF_TAPS_5,
	SFF_TAPS_6,
	SFF_TAPS_7,
	SFF_TAPS_8,
	SFF_TAPS_9,
	SFF_TAPS_10,
	SFF_TAPS_11,
	SFF_TAPS_12,
	SFF_TAPS_13,
	SFF_TAPS_14,
	SFF_TAPS_15
};

static SpatialFilterFunction m_unsafeTapFunctions[SF_COUNT] = {
	SFFU_TAPS_0,
	SFFU_TAPS_1,
	SFFU_TAPS_2,
	SFFU_TAPS_3,
	SFFU_TAPS_4,
	SFFU_TAPS_5,
	SFFU_TAPS_6,
	SFFU_TAPS_7,
	SFFU_TAPS_8,
	SFFU_TAPS_9,
	SFFU_TAPS_10,
	SFFU_TAPS_11,
	SFFU_TAPS_12,
	SFFU_TAPS_13,
	SFFU_TAPS_14,
	SFFU_TAPS_15
};


void cat::ResetSpatialFilters() {
	memcpy(SPATIAL_FILTERS, DEF_SPATIAL_FILTERS, sizeof(SPATIAL_FILTERS));
	memcpy(UNSAFE_SPATIAL_FILTERS, DEF_UNSAFE_SPATIAL_FILTERS, sizeof(UNSAFE_SPATIAL_FILTERS));
}

void cat::SetSpatialFilter(int index, int a, int b, int c, int d) {
	SPATIAL_FILTERS[index] = m_safeTapFunctions[index];
	UNSAFE_SPATIAL_FILTERS[index] = m_unsafeTapFunctions[index];

	m_taps[index][0] = a;
	m_taps[index][1] = b;
	m_taps[index][2] = c;
	m_taps[index][3] = d;
}


//// Color Filters

#define START_R2Y \
	const u8 R = rgb[0]; \
	const u8 G = rgb[1]; \
	const u8 B = rgb[2]; \
	u8 Y, U, V;

#define END_R2Y \
	yuv[0] = Y; \
	yuv[1] = U; \
	yuv[2] = V;

void CFF_R2Y_GB_RG(const u8 rgb[3], u8 yuv[3]) {
	START_R2Y;

	Y = B;
	U = G - B;
	V = G - R;

	END_R2Y;
}

void CFF_R2Y_GR_BG(const u8 rgb[3], u8 yuv[3]) {
	START_R2Y;

	Y = G - B;
	U = G - R;
	V = R;

	END_R2Y;
}

void CFF_R2Y_YUVr(const u8 rgb[3], u8 yuv[3]) {
	START_R2Y;

	U = B - G;
	V = R - G;
	Y = G + (((char)U + (char)V) >> 2);

	END_R2Y;
}

void CFF_R2Y_D9(const u8 rgb[3], u8 yuv[3]) {
	START_R2Y;

	Y = R;
	U = B - ((R + G*3) >> 2);
	V = G - R;

	END_R2Y;
}

void CFF_R2Y_D12(const u8 rgb[3], u8 yuv[3]) {
	START_R2Y;

	Y = B;
	U = G - ((R*3 + B) >> 2);
	V = R - B;

	END_R2Y;
}

void CFF_R2Y_D8(const u8 rgb[3], u8 yuv[3]) {
	START_R2Y;

	Y = R;
	U = B - ((R + G) >> 1);
	V = G - R;

	END_R2Y;
}

void CFF_R2Y_E2_R(const u8 rgb[3], u8 yuv[3]) {
	START_R2Y;

	char Co = R - G;
	int t = G + (Co >> 1);
	char Cg = B - t;

	Y = t + (Cg >> 1);
	U = Cg;
	V = Co;

	END_R2Y;
}

void CFF_R2Y_BG_RG(const u8 rgb[3], u8 yuv[3]) {
	START_R2Y;

	Y = G - B;
	U = G;
	V = G - R;

	END_R2Y;
}

void CFF_R2Y_GR_BR(const u8 rgb[3], u8 yuv[3]) {
	START_R2Y;

	Y = B - R;
	U = G - R;
	V = R;

	END_R2Y;
}

void CFF_R2Y_D18(const u8 rgb[3], u8 yuv[3]) {
	START_R2Y;

	Y = B;
	U = R - ((G*3 + B) >> 2);
	V = G - B;

	END_R2Y;
}

void CFF_R2Y_B_GR_R(const u8 rgb[3], u8 yuv[3]) {
	START_R2Y;

	Y = B;
	U = G - R;
	V = R;

	END_R2Y;
}

void CFF_R2Y_D11(const u8 rgb[3], u8 yuv[3]) {
	START_R2Y;

	Y = B;
	U = G - ((R + B) >> 1);
	V = R - B;

	END_R2Y;
}

void CFF_R2Y_D14(const u8 rgb[3], u8 yuv[3]) {
	START_R2Y;

	Y = R;
	U = G - ((R + B) >> 1);
	V = B - R;

	END_R2Y;
}

void CFF_R2Y_D10(const u8 rgb[3], u8 yuv[3]) {
	START_R2Y;

	Y = B;
	U = G - ((R + B*3) >> 2);
	V = R - B;

	END_R2Y;
}

void CFF_R2Y_YCgCo_R(const u8 rgb[3], u8 yuv[3]) {
	START_R2Y;

	char Co = R - B;
	int t = B + (Co >> 1);
	char Cg = G - t;

	Y = t + (Cg >> 1);
	U = Cg;
	V = Co;

	END_R2Y;
}

void CFF_R2Y_GB_RB(const u8 rgb[3], u8 yuv[3]) {
	START_R2Y;

	Y = B;
	U = G - B;
	V = R - B;

	END_R2Y;
}

#undef START_R2Y
#undef END_R2Y


RGB2YUVFilterFunction cat::RGB2YUV_FILTERS[CF_COUNT] = {
	CFF_R2Y_GB_RG,
	CFF_R2Y_GR_BG,
	CFF_R2Y_YUVr,
	CFF_R2Y_D9,
	CFF_R2Y_D12,
	CFF_R2Y_D8,
	CFF_R2Y_E2_R,
	CFF_R2Y_BG_RG,
	CFF_R2Y_GR_BR,
	CFF_R2Y_D18,
	CFF_R2Y_B_GR_R,
	CFF_R2Y_D11,
	CFF_R2Y_D14,
	CFF_R2Y_D10,
	CFF_R2Y_YCgCo_R,
	CFF_R2Y_GB_RB
};


#define START_Y2R \
	const u8 Y = yuv[0]; \
	const u8 U = yuv[1]; \
	const u8 V = yuv[2]; \
	u8 R, G, B;

#define END_Y2R \
	rgb[0] = R; \
	rgb[1] = G; \
	rgb[2] = B;

void CFF_Y2R_GB_RG(const u8 yuv[3], u8 rgb[3]) {
	START_Y2R;

	B = Y;
	G = U + B;
	R = G - V;

	END_Y2R;
}

void CFF_Y2R_GR_BG(const u8 yuv[3], u8 rgb[3]) {
	START_Y2R;

	R = V;
	G = U + R;
	B = G - Y;

	END_Y2R;
}

void CFF_Y2R_YUVr(const u8 yuv[3], u8 rgb[3]) {
	START_Y2R;

	G = Y - (((char)U + (char)V) >> 2);
	R = V + G;
	B = U + G;

	END_Y2R;
}

void CFF_Y2R_D9(const u8 yuv[3], u8 rgb[3]) {
	START_Y2R;

	R = Y;
	G = V + R;
	B = U + (((u8)R + (u8)G*3) >> 2);

	END_Y2R;
}

void CFF_Y2R_D12(const u8 yuv[3], u8 rgb[3]) {
	START_Y2R;

	B = Y;
	R = B + V;
	G = U + (((u8)R*3 + (u8)B) >> 2);

	END_Y2R;
}

void CFF_Y2R_D8(const u8 yuv[3], u8 rgb[3]) {
	START_Y2R;

	R = Y;
	G = V + R;
	B = U + (((u8)R + (u8)G) >> 1);

	END_Y2R;
}

void CFF_Y2R_E2_R(const u8 yuv[3], u8 rgb[3]) {
	START_Y2R;

	char Co = V;
	char Cg = U;
	const int t = Y - (Cg >> 1);

	B = Cg + t;
	G = t - (Co >> 1);
	R = Co + G;

	END_Y2R;
}

void CFF_Y2R_BG_RG(const u8 yuv[3], u8 rgb[3]) {
	START_Y2R;

	G = U;
	B = G - Y;
	R = G - V;

	END_Y2R;
}

void CFF_Y2R_GR_BR(const u8 yuv[3], u8 rgb[3]) {
	START_Y2R;

	R = V;
	B = Y + R;
	G = U + R;

	END_Y2R;
}

void CFF_Y2R_D18(const u8 yuv[3], u8 rgb[3]) {
	START_Y2R;

	B = Y;
	G = V + B;
	R = U + (((u8)G*3 + (u8)B) >> 2);

	END_Y2R;
}

void CFF_Y2R_B_GR_R(const u8 yuv[3], u8 rgb[3]) {
	START_Y2R;

	R = V;
	G = U + R;
	B = Y;

	END_Y2R;
}

void CFF_Y2R_D11(const u8 yuv[3], u8 rgb[3]) {
	START_Y2R;

	B = Y;
	R = V + B;
	G = U + (((u8)R + (u8)B) >> 1);

	END_Y2R;
}

void CFF_Y2R_D14(const u8 yuv[3], u8 rgb[3]) {
	START_Y2R;

	R = Y;
	B = V + R;
	G = U + (((u8)R + (u8)B) >> 1);

	END_Y2R;
}

void CFF_Y2R_D10(const u8 yuv[3], u8 rgb[3]) {
	START_Y2R;

	B = Y;
	R = V + B;
	G = U + (((u8)R + (u8)B*3) >> 2);

	END_Y2R;
}

void CFF_Y2R_YCgCo_R(const u8 yuv[3], u8 rgb[3]) {
	START_Y2R;

	char Co = V;
	char Cg = U;
	const int t = Y - (Cg >> 1);

	G = Cg + t;
	B = t - (Co >> 1);
	R = Co + B;

	END_Y2R;
}

void CFF_Y2R_GB_RB(const u8 yuv[3], u8 rgb[3]) {
	START_Y2R;

	B = Y;
	G = U + B;
	R = V + B;

	END_Y2R;
}

#undef START_Y2R
#undef END_Y2R


YUV2RGBFilterFunction cat::YUV2RGB_FILTERS[CF_COUNT] = {
	CFF_Y2R_GB_RG,
	CFF_Y2R_GR_BG,
	CFF_Y2R_YUVr,
	CFF_Y2R_D9,
	CFF_Y2R_D12,
	CFF_Y2R_D8,
	CFF_Y2R_E2_R,
	CFF_Y2R_BG_RG,
	CFF_Y2R_GR_BR,
	CFF_Y2R_D18,
	CFF_Y2R_B_GR_R,
	CFF_Y2R_D11,
	CFF_Y2R_D14,
	CFF_Y2R_D10,
	CFF_Y2R_YCgCo_R,
	CFF_Y2R_GB_RB
};


const char *cat::GetColorFilterString(int cf) {
	switch (cf) {
		case CF_YUVr:	// YUVr from JPEG2000
			return "YUVr";

		case CF_E2_R:	// derived from E2 and YCgCo-R
			 return "E2-R";

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
	}

	return "Unknown";
}


#ifdef TEST_COLOR_FILTERS

#include <iostream>
using namespace std;

void cat::testColorFilters() {
	for (int cf = 0; cf < CF_COUNT; ++cf) {
		for (int r = 0; r < 256; ++r) {
			for (int g = 0; g < 256; ++g) {
				for (int b = 0; b < 256; ++b) {
					u8 yuv[3];
					u8 rgb[3] = {r, g, b};
					RGB2YUV_FILTERS[cf](rgb, yuv);
					u8 rgb2[3];
					YUV2RGB_FILTERS[cf](yuv, rgb2);

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

const u8 cat::CHAOS_TABLE_1[512] = {
	0
};

const u8 cat::CHAOS_TABLE_8[512] = {
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

