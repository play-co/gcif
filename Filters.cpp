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


//// Custom tapped filter subsystem

const int SpatialFilterSet::FILTER_TAPS[TAPPED_COUNT][4] = {
	{ 3, 3, 0, -4 }, // PRED394 = (3A + 3B + 0C + -4D) / 2  [score = 9]
	{ 2, 4, 0, -4 }, // PRED402 = (2A + 4B + 0C + -4D) / 2  [score = 7]
	{ 1, 2, 3, -4 }, // PRED626 = (1A + 2B + 3C + -4D) / 2  [score = 102]
	{ 2, 4, -1, -3 }, // PRED1050 = (2A + 4B + -1C + -3D) / 2  [score = 5]
	{ 3, 4, -3, -2 }, // PRED1618 = (3A + 4B + -3C + -2D) / 2  [score = 89]
	{ 2, 4, -2, -2 }, // PRED1698 = (2A + 4B + -2C + -2D) / 2  [score = 7]
	{ 4, 0, 0, -2 }, // PRED1826 = (4A + 0B + 0C + -2D) / 2  [score = 13]
	{ 3, 1, 0, -2 }, // PRED1834 = (3A + 1B + 0C + -2D) / 2  [score = 7]
	{ 2, 2, 0, -2 }, // PRED1842 = (2A + 2B + 0C + -2D) / 2  [score = 14]
	{ 4, -1, 1, -2 }, // PRED1898 = (4A + -1B + 1C + -2D) / 2  [score = 9]
	{ 3, 0, 1, -2 }, // PRED1906 = (3A + 0B + 1C + -2D) / 2  [score = 24]
	{ 2, 0, 2, -2 }, // PRED1986 = (2A + 0B + 2C + -2D) / 2  [score = 29]
	{ 0, 2, 2, -2 }, // PRED2002 = (0A + 2B + 2C + -2D) / 2  [score = 12]
	{ -1, 1, 4, -2 }, // PRED2154 = (-1A + 1B + 4C + -2D) / 2  [score = 14]
	{ -2, 2, 4, -2 }, // PRED2162 = (-2A + 2B + 4C + -2D) / 2  [score = 107]
	{ 2, 3, -2, -1 }, // PRED2418 = (2A + 3B + -2C + -1D) / 2  [score = 206]
	{ 2, 2, -1, -1 }, // PRED2490 = (2A + 2B + -1C + -1D) / 2  [score = 277]
	{ 1, 3, -1, -1 }, // PRED2498 = (1A + 3B + -1C + -1D) / 2  [score = 117]
	{ 3, 0, 0, -1 }, // PRED2554 = (3A + 0B + 0C + -1D) / 2  [score = 14]
	{ 2, 1, 0, -1 }, // PRED2562 = (2A + 1B + 0C + -1D) / 2  [score = 15]
	{ 1, 2, 0, -1 }, // PRED2570 = (1A + 2B + 0C + -1D) / 2  [score = 8]
	{ 0, 3, 0, -1 }, // PRED2578 = (0A + 3B + 0C + -1D) / 2  [score = 105]
	{ 4, -2, 1, -1 }, // PRED2618 = (4A + -2B + 1C + -1D) / 2  [score = 15]
	{ 2, 0, 1, -1 }, // PRED2634 = (2A + 0B + 1C + -1D) / 2  [score = 24]
	{ 1, 1, 1, -1 }, // PRED2642 = (1A + 1B + 1C + -1D) / 2  [score = 65]
	{ 0, 2, 1, -1 }, // PRED2650 = (0A + 2B + 1C + -1D) / 2  [score = 17]
	{ 2, -1, 2, -1 }, // PRED2706 = (2A + -1B + 2C + -1D) / 2  [score = 8]
	{ 1, 0, 2, -1 }, // PRED2714 = (1A + 0B + 2C + -1D) / 2  [score = 66]
	{ 0, 1, 2, -1 }, // PRED2722 = (0A + 1B + 2C + -1D) / 2  [score = 21]
	{ -2, 2, 3, -1 }, // PRED2810 = (-2A + 2B + 3C + -1D) / 2  [score = 11]
	{ 2, 3, -3, 0 }, // PRED3066 = (2A + 3B + -3C + 0D) / 2  [score = 8]
	{ 2, 1, -1, 0 }, // PRED3210 = (2A + 1B + -1C + 0D) / 2  [score = 54]
	{ 1, 2, -1, 0 }, // PRED3218 = (1A + 2B + -1C + 0D) / 2  [score = 30]
	{ 3, -1, 0, 0 }, // PRED3274 = (3A + -1B + 0C + 0D) / 2  [score = 49]
	{ 3, -2, 1, 0 }, // PRED3346 = (3A + -2B + 1C + 0D) / 2  [score = 9]
	{ 2, -1, 1, 0 }, // PRED3354 = (2A + -1B + 1C + 0D) / 2  [score = 21]
	{ 1, 0, 1, 0 }, // PRED3362 = (1A + 0B + 1C + 0D) / 2  [score = 211]
	{ 0, 1, 1, 0 }, // PRED3370 = (0A + 1B + 1C + 0D) / 2  [score = 383]
	{ -1, 2, 1, 0 }, // PRED3378 = (-1A + 2B + 1C + 0D) / 2  [score = 88]
	{ 2, -2, 2, 0 }, // PRED3426 = (2A + -2B + 2C + 0D) / 2  [score = 24]
	{ 1, -1, 2, 0 }, // PRED3434 = (1A + -1B + 2C + 0D) / 2  [score = 50]
	{ -1, 1, 2, 0 }, // PRED3450 = (-1A + 1B + 2C + 0D) / 2  [score = 134]
	{ -2, 2, 2, 0 }, // PRED3458 = (-2A + 2B + 2C + 0D) / 2  [score = 237]
	{ -1, 0, 3, 0 }, // PRED3522 = (-1A + 0B + 3C + 0D) / 2  [score = 7]
	{ 2, 1, -2, 1 }, // PRED3858 = (2A + 1B + -2C + 1D) / 2  [score = 8]
	{ 2, 0, -1, 1 }, // PRED3930 = (2A + 0B + -1C + 1D) / 2  [score = 121]
	{ 1, 1, -1, 1 }, // PRED3938 = (1A + 1B + -1C + 1D) / 2  [score = 24]
	{ 0, 2, -1, 1 }, // PRED3946 = (0A + 2B + -1C + 1D) / 2  [score = 13]
	{ 2, -1, 0, 1 }, // PRED4002 = (2A + -1B + 0C + 1D) / 2  [score = 74]
	{ -1, 2, 0, 1 }, // PRED4026 = (-1A + 2B + 0C + 1D) / 2  [score = 99]
	{ 2, -2, 1, 1 }, // PRED4074 = (2A + -2B + 1C + 1D) / 2  [score = 141]
	{ 1, -1, 1, 1 }, // PRED4082 = (1A + -1B + 1C + 1D) / 2  [score = 35]
	{ 0, 0, 1, 1 }, // PRED4090 = (0A + 0B + 1C + 1D) / 2  [score = 779]
	{ -1, 1, 1, 1 }, // PRED4098 = (-1A + 1B + 1C + 1D) / 2  [score = 617]
	{ -2, 2, 1, 1 }, // PRED4106 = (-2A + 2B + 1C + 1D) / 2  [score = 85]
	{ 1, -2, 2, 1 }, // PRED4154 = (1A + -2B + 2C + 1D) / 2  [score = 152]
	{ 2, -3, 2, 1 }, // PRED4146 = (2A + -3B + 2C + 1D) / 2  [score = 12]
	{ 0, -1, 2, 1 }, // PRED4162 = (0A + -1B + 2C + 1D) / 2  [score = 7]
	{ -1, 0, 2, 1 }, // PRED4170 = (-1A + 0B + 2C + 1D) / 2  [score = 40]
	{ 1, -3, 3, 1 }, // PRED4226 = (1A + -3B + 3C + 1D) / 2  [score = 75]
	{ 2, 0, -2, 2 }, // PRED4578 = (2A + 0B + -2C + 2D) / 2  [score = 17]
	{ 0, 2, -2, 2 }, // PRED4594 = (0A + 2B + -2C + 2D) / 2  [score = 22]
	{ 2, -1, -1, 2 }, // PRED4650 = (2A + -1B + -1C + 2D) / 2  [score = 175]
	{ 1, 0, -1, 2 }, // PRED4658 = (1A + 0B + -1C + 2D) / 2  [score = 12]
	{ 0, 1, -1, 2 }, // PRED4666 = (0A + 1B + -1C + 2D) / 2  [score = 24]
	{ 2, -2, 0, 2 }, // PRED4722 = (2A + -2B + 0C + 2D) / 2  [score = 15]
	{ 1, -1, 0, 2 }, // PRED4730 = (1A + -1B + 0C + 2D) / 2  [score = 18]
	{ -1, 1, 0, 2 }, // PRED4746 = (-1A + 1B + 0C + 2D) / 2  [score = 240]
	{ -2, 2, 0, 2 }, // PRED4754 = (-2A + 2B + 0C + 2D) / 2  [score = 379]
	{ 2, -3, 1, 2 }, // PRED4794 = (2A + -3B + 1C + 2D) / 2  [score = 250]
	{ 1, -2, 1, 2 }, // PRED4802 = (1A + -2B + 1C + 2D) / 2  [score = 13]
	{ 0, -1, 1, 2 }, // PRED4810 = (0A + -1B + 1C + 2D) / 2  [score = 13]
	{ -1, 0, 1, 2 }, // PRED4818 = (-1A + 0B + 1C + 2D) / 2  [score = 17]
	{ 2, -4, 2, 2 }, // PRED4866 = (2A + -4B + 2C + 2D) / 2  [score = 7]
	{ 0, -2, 2, 2 }, // PRED4882 = (0A + -2B + 2C + 2D) / 2  [score = 12]
	{ -2, 0, 2, 2 }, // PRED4898 = (-2A + 0B + 2C + 2D) / 2  [score = 18]
	{ 1, -4, 3, 2 }, // PRED4946 = (1A + -4B + 3C + 2D) / 2  [score = 12]
	{ 2, -2, -1, 3 }, // PRED5370 = (2A + -2B + -1C + 3D) / 2  [score = 5]
	{ 0, -1, 0, 3 }, // PRED5458 = (0A + -1B + 0C + 3D) / 2  [score = 8]
	{ 2, -4, 0, 4 }, // PRED6162 = (2A + -4B + 0C + 4D) / 2  [score = 6]
};


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
				static const int ta = SpatialFilterSet::FILTER_TAPS[TAP][0]; \
				static const int tb = SpatialFilterSet::FILTER_TAPS[TAP][1]; \
				static const int tc = SpatialFilterSet::FILTER_TAPS[TAP][2]; \
				static const int td = SpatialFilterSet::FILTER_TAPS[TAP][3]; \
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
		static const int ta = SpatialFilterSet::FILTER_TAPS[TAP][0]; \
		static const int tb = SpatialFilterSet::FILTER_TAPS[TAP][1]; \
		static const int tc = SpatialFilterSet::FILTER_TAPS[TAP][2]; \
		static const int td = SpatialFilterSet::FILTER_TAPS[TAP][3]; \
		FPT[0] = (ta*a[0] + tb*b[0] + tc*c[0] + td*d[0]) / 2; \
		FPT[1] = (ta*a[1] + tb*b[1] + tc*c[1] + td*d[1]) / 2; \
		FPT[2] = (ta*a[2] + tb*b[2] + tc*c[2] + td*d[2]) / 2; \
		return FPT; \
	}

DEFINE_TAPS( 0);DEFINE_TAPS( 1);DEFINE_TAPS( 2);DEFINE_TAPS( 3);DEFINE_TAPS( 4)
DEFINE_TAPS( 5);DEFINE_TAPS( 6);DEFINE_TAPS( 7);DEFINE_TAPS( 8);DEFINE_TAPS( 9)
DEFINE_TAPS(10);DEFINE_TAPS(11);DEFINE_TAPS(12);DEFINE_TAPS(13);DEFINE_TAPS(14)
DEFINE_TAPS(15);DEFINE_TAPS(16);DEFINE_TAPS(17);DEFINE_TAPS(18);DEFINE_TAPS(19)
DEFINE_TAPS(20);DEFINE_TAPS(21);DEFINE_TAPS(22);DEFINE_TAPS(23);DEFINE_TAPS(24)
DEFINE_TAPS(25);DEFINE_TAPS(26);DEFINE_TAPS(27);DEFINE_TAPS(28);DEFINE_TAPS(29)
DEFINE_TAPS(30);DEFINE_TAPS(31);DEFINE_TAPS(32);DEFINE_TAPS(33);DEFINE_TAPS(34)
DEFINE_TAPS(35);DEFINE_TAPS(36);DEFINE_TAPS(37);DEFINE_TAPS(38);DEFINE_TAPS(39)
DEFINE_TAPS(40);DEFINE_TAPS(41);DEFINE_TAPS(42);DEFINE_TAPS(43);DEFINE_TAPS(44)
DEFINE_TAPS(45);DEFINE_TAPS(46);DEFINE_TAPS(47);DEFINE_TAPS(48);DEFINE_TAPS(49)
DEFINE_TAPS(50);DEFINE_TAPS(51);DEFINE_TAPS(52);DEFINE_TAPS(53);DEFINE_TAPS(54)
DEFINE_TAPS(55);DEFINE_TAPS(56);DEFINE_TAPS(57);DEFINE_TAPS(58);DEFINE_TAPS(59)
DEFINE_TAPS(60);DEFINE_TAPS(61);DEFINE_TAPS(62);DEFINE_TAPS(63);DEFINE_TAPS(64)
DEFINE_TAPS(65);DEFINE_TAPS(66);DEFINE_TAPS(67);DEFINE_TAPS(68);DEFINE_TAPS(69)
DEFINE_TAPS(70);DEFINE_TAPS(71);DEFINE_TAPS(72);DEFINE_TAPS(73);DEFINE_TAPS(74)
DEFINE_TAPS(75);DEFINE_TAPS(76);DEFINE_TAPS(77);DEFINE_TAPS(78);DEFINE_TAPS(79)

#undef DEFINE_TAPS

#define LIST_TAPS(TAP) \
	{ SFF_TAPS_ ## TAP, SFFU_TAPS_ ## TAP }

static const SpatialFilterSet::Functions
TAPPED_FILTER_FUNCTIONS[SpatialFilterSet::TAPPED_COUNT] = {
	LIST_TAPS( 0), LIST_TAPS( 1), LIST_TAPS( 2), LIST_TAPS( 3), LIST_TAPS( 4),
	LIST_TAPS( 5), LIST_TAPS( 6), LIST_TAPS( 7), LIST_TAPS( 8), LIST_TAPS( 9),
	LIST_TAPS(10), LIST_TAPS(11), LIST_TAPS(12), LIST_TAPS(13), LIST_TAPS(14),
	LIST_TAPS(15), LIST_TAPS(16), LIST_TAPS(17), LIST_TAPS(18), LIST_TAPS(19),
	LIST_TAPS(20), LIST_TAPS(21), LIST_TAPS(22), LIST_TAPS(23), LIST_TAPS(24),
	LIST_TAPS(25), LIST_TAPS(26), LIST_TAPS(27), LIST_TAPS(28), LIST_TAPS(29),
	LIST_TAPS(30), LIST_TAPS(31), LIST_TAPS(32), LIST_TAPS(33), LIST_TAPS(34),
	LIST_TAPS(35), LIST_TAPS(36), LIST_TAPS(37), LIST_TAPS(38), LIST_TAPS(39),
	LIST_TAPS(40), LIST_TAPS(41), LIST_TAPS(42), LIST_TAPS(43), LIST_TAPS(44),
	LIST_TAPS(45), LIST_TAPS(46), LIST_TAPS(47), LIST_TAPS(48), LIST_TAPS(49),
	LIST_TAPS(50), LIST_TAPS(51), LIST_TAPS(52), LIST_TAPS(53), LIST_TAPS(54),
	LIST_TAPS(55), LIST_TAPS(56), LIST_TAPS(57), LIST_TAPS(58), LIST_TAPS(59),
	LIST_TAPS(60), LIST_TAPS(61), LIST_TAPS(62), LIST_TAPS(63), LIST_TAPS(64),
	LIST_TAPS(65), LIST_TAPS(66), LIST_TAPS(67), LIST_TAPS(68), LIST_TAPS(69),
	LIST_TAPS(70), LIST_TAPS(71), LIST_TAPS(72), LIST_TAPS(73), LIST_TAPS(74),
	LIST_TAPS(75), LIST_TAPS(76), LIST_TAPS(77), LIST_TAPS(78), LIST_TAPS(79),
};

#undef LIST_TAPS


//// SpatialFilterSet

static const SpatialFilterSet::Functions DEF_SPATIAL_FILTERS[SF_COUNT] = {
	{ SFF_Z, SFFU_Z },
	{ SFF_D, SFFU_D },
	{ SFF_C, SFFU_C },
	{ SFF_B, SFFU_B },
	{ SFF_A, SFFU_A },
	{ SFF_AB, SFFU_AB },
	{ SFF_BD, SFFU_BD },
	{ SFF_CLAMP_GRAD, SFFU_CLAMP_GRAD },
	{ SFF_SKEW_GRAD, SFFU_SKEW_GRAD },
	{ SFF_PICK_LEFT, SFFU_PICK_LEFT },
	{ SFF_PRED_UR, SFFU_PRED_UR },
	{ SFF_ABC_CLAMP, SFFU_ABC_CLAMP },
	{ SFF_PAETH, SFFU_PAETH },
	{ SFF_ABC_PAETH, SFFU_ABC_PAETH },
	{ SFF_PLO, SFFU_PLO },
	{ SFF_ABCD, SFFU_ABCD },
	{ SFF_AD, SFFU_AD }
};


void SpatialFilterSet::reset() {
	CAT_DEBUG_ENFORCE(SF_COUNT == 17); // Need to update default arrays
	CAT_DEBUG_ENFORCE(TAPPED_COUNT == 80); // Need to update the function defs

	memcpy(_filters, DEF_SPATIAL_FILTERS, sizeof(_filters));
}

void SpatialFilterSet::replace(int defaultIndex, int tappedIndex) {
	CAT_DEBUG_ENFORCE(defaultIndex < SF_COUNT && tappedIndex < TAPPED_COUNT);

	_filters[defaultIndex] = TAPPED_FILTER_FUNCTIONS[tappedIndex];
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

