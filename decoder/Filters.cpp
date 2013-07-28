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
#include "Enforcer.hpp"
using namespace cat;


// Default zero
static const u8 FPZ[3] = {0};


//// Simple Spatial Filters

static const u8 *SFF_A(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (x > 0) {
		return p - 4; // A
	} else if (y > 0) {
		return p - xsize*4; // B
	}

	return FPZ;
}

static const u8 *SFFU_A(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0);

	return p - 4; // A
}

static const u8 *SFF_Z(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	return FPZ;
}

#define SFFU_Z SFF_Z

static const u8 *SFF_B(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (y > 0) {
		return p - xsize*4; // B
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_B(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(y > 0);

	return p - xsize*4; // B
}

static const u8 *SFF_C(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (x > 0) {
		if (y > 0) {
			return p - xsize*4 - 4; // C
		} else {
			return p - 4; // A
		}
	} else if (y > 0) {
		return p - xsize*4; // B
	}

	return FPZ;
}

static const u8 *SFFU_C(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0);

	return p - xsize*4 - 4; // C
}

static const u8 *SFF_D(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT fp = p - xsize*4; // B
		if (x < xsize-1) {
			fp += 4; // D
		}
		return fp;
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_D(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(y > 0 && x < xsize-1);

	return p - xsize*4 + 4; // D
}


//// Dual Average Filters (Round Down)

static const u8 *SFF_AVG_AB(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize*4; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT a = p - 4; // A

			temp[0] = (a[0] + (u16)b[0]) >> 1;
			temp[1] = (a[1] + (u16)b[1]) >> 1;
			temp[2] = (a[2] + (u16)b[2]) >> 1;
			return temp;
		} else {
			return b; // B
		}
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_AVG_AB(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 4; // A
	const u8 * CAT_RESTRICT b = p - xsize*4; // B

	temp[0] = (a[0] + (u16)b[0]) >> 1;
	temp[1] = (a[1] + (u16)b[1]) >> 1;
	temp[2] = (a[2] + (u16)b[2]) >> 1;
	return temp;
}

static const u8 *SFF_AVG_AC(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 4; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT c = a - xsize*4; // C

			temp[0] = (a[0] + (u16)c[0]) >> 1;
			temp[1] = (a[1] + (u16)c[1]) >> 1;
			temp[2] = (a[2] + (u16)c[2]) >> 1;
			return temp;
		} else {
			return a; // A
		}
	} else if (y > 0) {
		return p - xsize*4; // B
	}

	return FPZ;
}

static const u8 *SFFU_AVG_AC(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 4; // A
	const u8 * CAT_RESTRICT c = a - xsize*4; // C

	temp[0] = (a[0] + (u16)c[0]) >> 1;
	temp[1] = (a[1] + (u16)c[1]) >> 1;
	temp[2] = (a[2] + (u16)c[2]) >> 1;
	return temp;
}

static const u8 *SFF_AVG_AD(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT src = p - xsize*4; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT a = p - 4; // A
			if (x < xsize-1) {
				src += 4; // D
			}

			temp[0] = (a[0] + (u16)src[0]) >> 1;
			temp[1] = (a[1] + (u16)src[1]) >> 1;
			temp[2] = (a[2] + (u16)src[2]) >> 1;
			return temp;
		} else {
			return src; // B
		}
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_AVG_AD(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 4; // A
	const u8 * CAT_RESTRICT d = p - xsize*4 + 4; // D

	temp[0] = (a[0] + (u16)d[0]) >> 1;
	temp[1] = (a[1] + (u16)d[1]) >> 1;
	temp[2] = (a[2] + (u16)d[2]) >> 1;
	return temp;
}

static const u8 *SFF_AVG_BC(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize*4; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT c = b - 4; // C

			temp[0] = (b[0] + (u16)c[0]) >> 1;
			temp[1] = (b[1] + (u16)c[1]) >> 1;
			temp[2] = (b[2] + (u16)c[2]) >> 1;
			return temp;
		} else {
			return b; // B
		}
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_AVG_BC(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT b = p - xsize*4; // B
	const u8 * CAT_RESTRICT c = b - 4; // C

	temp[0] = (b[0] + (u16)c[0]) >> 1;
	temp[1] = (b[1] + (u16)c[1]) >> 1;
	temp[2] = (b[2] + (u16)c[2]) >> 1;
	return temp;
}

static const u8 *SFF_AVG_BD(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize*4; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT src = b - 4; // C
			if (x < xsize-1) {
				src += 8; // D
			}

			temp[0] = (b[0] + (u16)src[0]) >> 1;
			temp[1] = (b[1] + (u16)src[1]) >> 1;
			temp[2] = (b[2] + (u16)src[2]) >> 1;
			return temp;
		} else {
			return b; // B
		}
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_AVG_BD(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT b = p - xsize*4; // B
	const u8 * CAT_RESTRICT d = b + 4; // D

	temp[0] = (b[0] + (u16)d[0]) >> 1;
	temp[1] = (b[1] + (u16)d[1]) >> 1;
	temp[2] = (b[2] + (u16)d[2]) >> 1;
	return temp;
}

static const u8 *SFF_AVG_CD(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT src = p - xsize*4; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT c = src - 4; // C
			if (x < xsize-1) {
				src += 4; // D
			}

			temp[0] = (c[0] + (u16)src[0]) >> 1;
			temp[1] = (c[1] + (u16)src[1]) >> 1;
			temp[2] = (c[2] + (u16)src[2]) >> 1;
			return temp;
		} else {
			return src; // B
		}
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_AVG_CD(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT c = p - xsize*4 - 4; // C
	const u8 * CAT_RESTRICT d = c + 8; // D

	temp[0] = (c[0] + (u16)d[0]) >> 1;
	temp[1] = (c[1] + (u16)d[1]) >> 1;
	temp[2] = (c[2] + (u16)d[2]) >> 1;
	return temp;
}


//// Dual Average Filters (Round Up)

static const u8 *SFF_AVG_AB1(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize*4; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT a = p - 4; // A

			temp[0] = (a[0] + (u16)b[0] + 1) >> 1;
			temp[1] = (a[1] + (u16)b[1] + 1) >> 1;
			temp[2] = (a[2] + (u16)b[2] + 1) >> 1;
			return temp;
		} else {
			return b; // B
		}
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_AVG_AB1(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 4; // A
	const u8 * CAT_RESTRICT b = p - xsize*4; // B

	temp[0] = (a[0] + (u16)b[0] + 1) >> 1;
	temp[1] = (a[1] + (u16)b[1] + 1) >> 1;
	temp[2] = (a[2] + (u16)b[2] + 1) >> 1;
	return temp;
}

static const u8 *SFF_AVG_AC1(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize*4; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT a = p - 4; // A
			const u8 * CAT_RESTRICT c = b - 4; // C

			temp[0] = (a[0] + (u16)c[0] + 1) >> 1;
			temp[1] = (a[1] + (u16)c[1] + 1) >> 1;
			temp[2] = (a[2] + (u16)c[2] + 1) >> 1;
			return temp;
		} else {
			return b; // B
		}
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_AVG_AC1(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 4; // A
	const u8 * CAT_RESTRICT c = p - xsize*4 - 4; // C

	temp[0] = (a[0] + (u16)c[0] + 1) >> 1;
	temp[1] = (a[1] + (u16)c[1] + 1) >> 1;
	temp[2] = (a[2] + (u16)c[2] + 1) >> 1;
	return temp;
}

static const u8 *SFF_AVG_AD1(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT src = p - xsize*4; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT a = p - 4; // A
			if (x < xsize-1) {
				src += 4; // D
			}

			temp[0] = (a[0] + (u16)src[0] + 1) >> 1;
			temp[1] = (a[1] + (u16)src[1] + 1) >> 1;
			temp[2] = (a[2] + (u16)src[2] + 1) >> 1;
			return temp;
		} else {
			return src; // B
		}
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_AVG_AD1(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 4; // A
	const u8 * CAT_RESTRICT d = p - xsize*4 + 4; // D

	temp[0] = (a[0] + (u16)d[0] + 1) >> 1;
	temp[1] = (a[1] + (u16)d[1] + 1) >> 1;
	temp[2] = (a[2] + (u16)d[2] + 1) >> 1;
	return temp;
}

static const u8 *SFF_AVG_BC1(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize*4; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT c = b - 4; // C

			temp[0] = (b[0] + (u16)c[0] + 1) >> 1;
			temp[1] = (b[1] + (u16)c[1] + 1) >> 1;
			temp[2] = (b[2] + (u16)c[2] + 1) >> 1;
			return temp;
		} else {
			return b; // B
		}
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_AVG_BC1(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT b = p - xsize*4; // B
	const u8 * CAT_RESTRICT c = b - 4; // C

	temp[0] = (b[0] + (u16)c[0] + 1) >> 1;
	temp[1] = (b[1] + (u16)c[1] + 1) >> 1;
	temp[2] = (b[2] + (u16)c[2] + 1) >> 1;
	return temp;
}

static const u8 *SFF_AVG_BD1(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize*4; // B

		if (x < xsize-1) {
			const u8 * CAT_RESTRICT d = b + 4; // D

			temp[0] = (b[0] + (u16)d[0] + 1) >> 1;
			temp[1] = (b[1] + (u16)d[1] + 1) >> 1;
			temp[2] = (b[2] + (u16)d[2] + 1) >> 1;
			return temp;
		} else if (x > 0) {
			const u8 * CAT_RESTRICT c = b - 4; // C

			temp[0] = (b[0] + (u16)c[0] + 1) >> 1;
			temp[1] = (b[1] + (u16)c[1] + 1) >> 1;
			temp[2] = (b[2] + (u16)c[2] + 1) >> 1;
			return temp;
		} else {
			return b; // B
		}
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_AVG_BD1(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT b = p - xsize*4; // B
	const u8 * CAT_RESTRICT d = b + 4; // D

	temp[0] = (b[0] + (u16)d[0] + 1) >> 1;
	temp[1] = (b[1] + (u16)d[1] + 1) >> 1;
	temp[2] = (b[2] + (u16)d[2] + 1) >> 1;
	return temp;
}

static const u8 *SFF_AVG_CD1(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT src = p - xsize*4; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT c = src - 4; // C
			if (x < xsize-1) {
				src += 4; // D
			}

			temp[0] = (c[0] + (u16)src[0] + 1) >> 1;
			temp[1] = (c[1] + (u16)src[1] + 1) >> 1;
			temp[2] = (c[2] + (u16)src[2] + 1) >> 1;
			return temp;
		} else {
			return src; // B
		}
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_AVG_CD1(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT c = p - xsize*4 - 4; // C
	const u8 * CAT_RESTRICT d = c + 8; // D

	temp[0] = (c[0] + (u16)d[0] + 1) >> 1;
	temp[1] = (c[1] + (u16)d[1] + 1) >> 1;
	temp[2] = (c[2] + (u16)d[2] + 1) >> 1;
	return temp;
}


//// Triple Average Filters (Round Down)

static const u8 *SFF_AVG_ABC(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 4; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize*4; // B
			const u8 * CAT_RESTRICT c = b - 4; // C

			temp[0] = (a[0] + (u16)b[0] + c[0]) / 3;
			temp[1] = (a[1] + (u16)b[1] + c[1]) / 3;
			temp[2] = (a[2] + (u16)b[2] + c[2]) / 3;
			return temp;
		} else {
			return a;
		}
	} else if (y > 0) {
		return p - xsize*4; // B
	}

	return FPZ;
}

static const u8 *SFFU_AVG_ABC(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 4; // A
	const u8 * CAT_RESTRICT b = p - xsize*4; // B
	const u8 * CAT_RESTRICT c = b - 4; // C

	temp[0] = (a[0] + (u16)b[0] + c[0]) / 3;
	temp[1] = (a[1] + (u16)b[1] + c[1]) / 3;
	temp[2] = (a[2] + (u16)b[2] + c[2]) / 3;
	return temp;
}

static const u8 *SFF_AVG_ACD(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 4; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize*4; // B
			const u8 * CAT_RESTRICT c = b - 4; // C

			const u8 * CAT_RESTRICT src = b; // B
			if (x < xsize-1) {
				src += 4; // D
			}

			temp[0] = (a[0] + (u16)c[0] + src[0]) / 3;
			temp[1] = (a[1] + (u16)c[1] + src[1]) / 3;
			temp[2] = (a[2] + (u16)c[2] + src[2]) / 3;
			return temp;
		} else {
			return a;
		}
	} else if (y > 0) {
		const u8 * CAT_RESTRICT src = p - xsize*4; // B
		if CAT_LIKELY(x < xsize-1) {
			src += 4; // D
		}

		return src;
	}

	return FPZ;
}

static const u8 *SFFU_AVG_ACD(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 4; // A
	const u8 * CAT_RESTRICT b = p - xsize*4; // B
	const u8 * CAT_RESTRICT c = b - 4; // C
	const u8 * CAT_RESTRICT d = b + 4; // D

	temp[0] = (a[0] + (u16)c[0] + d[0]) / 3;
	temp[1] = (a[1] + (u16)c[1] + d[1]) / 3;
	temp[2] = (a[2] + (u16)c[2] + d[2]) / 3;
	return temp;
}

static const u8 *SFF_AVG_ABD(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 4; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize*4; // B

			const u8 * CAT_RESTRICT src = b; // B
			if (x < xsize-1) {
				src += 4; // D
			}

			temp[0] = (a[0] + (u16)b[0] + src[0]) / 3;
			temp[1] = (a[1] + (u16)b[1] + src[1]) / 3;
			temp[2] = (a[2] + (u16)b[2] + src[2]) / 3;
			return temp;
		} else {
			return a;
		}
	} else if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize*4; // B
		const u8 * CAT_RESTRICT d = b; // D
		if CAT_LIKELY(x < xsize-1) {
			b += 4;
		}

		temp[0] = (b[0] + (u16)d[0]) >> 1;
		temp[1] = (b[1] + (u16)d[1]) >> 1;
		temp[2] = (b[2] + (u16)d[2]) >> 1;
		return temp;
	}

	return FPZ;
}

static const u8 *SFFU_AVG_ABD(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 4; // A
	const u8 * CAT_RESTRICT b = p - xsize*4; // B
	const u8 * CAT_RESTRICT d = b + 4; // D

	temp[0] = (a[0] + (u16)b[0] + d[0]) / 3;
	temp[1] = (a[1] + (u16)b[1] + d[1]) / 3;
	temp[2] = (a[2] + (u16)b[2] + d[2]) / 3;
	return temp;
}

static const u8 *SFF_AVG_BCD(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 4; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize*4; // B
			const u8 * CAT_RESTRICT c = b - 4; // C

			const u8 * CAT_RESTRICT src = b; // B
			if (x < xsize-1) {
				src += 4; // D
			}

			temp[0] = (b[0] + (u16)c[0] + src[0]) / 3;
			temp[1] = (b[1] + (u16)c[1] + src[1]) / 3;
			temp[2] = (b[2] + (u16)c[2] + src[2]) / 3;
			return temp;
		} else {
			return a;
		}
	} else if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize*4; // B
		const u8 * CAT_RESTRICT d = b; // D
		if CAT_LIKELY(x < xsize-1) {
			b += 4;
		}

		temp[0] = (b[0] + (u16)d[0]) >> 1;
		temp[1] = (b[1] + (u16)d[1]) >> 1;
		temp[2] = (b[2] + (u16)d[2]) >> 1;
		return temp;
	}

	return FPZ;
}

static const u8 *SFFU_AVG_BCD(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT b = p - xsize*4; // B
	const u8 * CAT_RESTRICT c = b - 4; // C
	const u8 * CAT_RESTRICT d = b + 4; // D

	temp[0] = (b[0] + (u16)c[0] + d[0]) / 3;
	temp[1] = (b[1] + (u16)c[1] + d[1]) / 3;
	temp[2] = (b[2] + (u16)c[2] + d[2]) / 3;
	return temp;
}


//// Quad Average Filters (Round Down)

static const u8 *SFF_AVG_ABCD(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 4; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize*4; // B
			const u8 * CAT_RESTRICT c = b - 4; // C

			const u8 * CAT_RESTRICT src = b; // B
			if (x < xsize-1) {
				src += 4; // D
			}

			temp[0] = (a[0] + (u16)b[0] + c[0] + (u16)src[0]) >> 2;
			temp[1] = (a[1] + (u16)b[1] + c[1] + (u16)src[1]) >> 2;
			temp[2] = (a[2] + (u16)b[2] + c[2] + (u16)src[2]) >> 2;
			return temp;
		} else {
			return a;
		}
	} else if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize*4; // B
		const u8 * CAT_RESTRICT d = b; // D
		if CAT_LIKELY(x < xsize-1) {
			b += 4;
		}

		temp[0] = (b[0] + (u16)d[0]) >> 1;
		temp[1] = (b[1] + (u16)d[1]) >> 1;
		temp[2] = (b[2] + (u16)d[2]) >> 1;
		return temp;
	}

	return FPZ;
}

static const u8 *SFFU_AVG_ABCD(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 4; // A
	const u8 * CAT_RESTRICT b = p - xsize*4; // B
	const u8 * CAT_RESTRICT c = b - 4; // C
	const u8 * CAT_RESTRICT d = b + 4; // D

	temp[0] = (a[0] + (u16)b[0] + c[0] + (u16)d[0]) >> 2;
	temp[1] = (a[1] + (u16)b[1] + c[1] + (u16)d[1]) >> 2;
	temp[2] = (a[2] + (u16)b[2] + c[2] + (u16)d[2]) >> 2;
	return temp;
}


//// Quad Average Filters (Round Up)

static const u8 *SFF_AVG_ABCD1(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 4; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize*4; // B
			const u8 * CAT_RESTRICT c = b - 4; // C

			const u8 * CAT_RESTRICT src = b; // B
			if (x < xsize-1) {
				src += 4; // D
			}

			temp[0] = (a[0] + (u16)b[0] + c[0] + (u16)src[0] + 2) >> 2;
			temp[1] = (a[1] + (u16)b[1] + c[1] + (u16)src[1] + 2) >> 2;
			temp[2] = (a[2] + (u16)b[2] + c[2] + (u16)src[2] + 2) >> 2;
			return temp;
		} else {
			return a;
		}
	} else if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize*4; // B
		const u8 * CAT_RESTRICT d = b; // D
		if CAT_LIKELY(x < xsize-1) {
			b += 4;
		}

		temp[0] = (b[0] + (u16)d[0] + 1) >> 1;
		temp[1] = (b[1] + (u16)d[1] + 1) >> 1;
		temp[2] = (b[2] + (u16)d[2] + 1) >> 1;
		return temp;
	}

	return FPZ;
}

static const u8 *SFFU_AVG_ABCD1(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 4; // A
	const u8 * CAT_RESTRICT b = p - xsize*4; // B
	const u8 * CAT_RESTRICT c = b - 4; // C
	const u8 * CAT_RESTRICT d = b + 4; // D

	temp[0] = (a[0] + (u16)b[0] + c[0] + (u16)d[0] + 2) >> 2;
	temp[1] = (a[1] + (u16)b[1] + c[1] + (u16)d[1] + 2) >> 2;
	temp[2] = (a[2] + (u16)b[2] + c[2] + (u16)d[2] + 2) >> 2;
	return temp;
}


//// Clamped Gradient Filter

static CAT_INLINE u8 clampGrad(int b, int a, int c) {
	int grad = (int)b + (int)a - (int)c;

	int lo = b, hi = b;
	if (lo > a) {
		lo = a;
	} else {
		hi = a;
	}
	if (lo > c) {
		lo = c;
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

static const u8 *SFF_CLAMP_GRAD(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize*4; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT a = p - 4; // A
			const u8 * CAT_RESTRICT c = b - 4; // C

			temp[0] = clampGrad(b[0], a[0], c[0]);
			temp[1] = clampGrad(b[1], a[1], c[1]);
			temp[2] = clampGrad(b[2], a[2], c[2]);
			return temp;
		} else {
			return b;
		}
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_CLAMP_GRAD(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 4; // A
	const u8 * CAT_RESTRICT b = p - xsize*4; // B
	const u8 * CAT_RESTRICT c = b - 4; // C

	temp[0] = clampGrad(b[0], a[0], c[0]);
	temp[1] = clampGrad(b[1], a[1], c[1]);
	temp[2] = clampGrad(b[2], a[2], c[2]);
	return temp;
}


//// Skewed Gradient Filter

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

static const u8 *SFF_SKEW_GRAD(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize*4; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT a = p - 4; // A
			const u8 * CAT_RESTRICT c = b - 4; // C

			temp[0] = skewGrad(b[0], a[0], c[0]);
			temp[1] = skewGrad(b[1], a[1], c[1]);
			temp[2] = skewGrad(b[2], a[2], c[2]);
			return temp;
		} else {
			return b;
		}
	} else if (x > 0) {
		return p - 4; // A
	}

	return FPZ;
}

static const u8 *SFFU_SKEW_GRAD(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 4; // A
	const u8 * CAT_RESTRICT b = p - xsize*4; // B
	const u8 * CAT_RESTRICT c = b - 4; // C

	temp[0] = skewGrad(b[0], a[0], c[0]);
	temp[1] = skewGrad(b[1], a[1], c[1]);
	temp[2] = skewGrad(b[2], a[2], c[2]);
	return temp;
}


//// ABC Clamped Gradient Filter

static CAT_INLINE u8 abcClamp(int a, int b, int c) {
	int sum = a + b - c;
	if (sum <= 0) {
		return 0;
	} else if (sum >= 255) {
		return 255;
	}

	return sum;
}

static const u8 *SFF_ABC_CLAMP(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 4; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize*4; // B
			const u8 * CAT_RESTRICT c = b - 4; // C

			temp[0] = abcClamp(a[0], b[0], c[0]);
			temp[1] = abcClamp(a[1], b[1], c[1]);
			temp[2] = abcClamp(a[2], b[2], c[2]);
			return temp;
		} else {
			return a;
		}
	} else if (y > 0) {
		return p - xsize*4; // B
	}

	return FPZ;
}

static const u8 *SFFU_ABC_CLAMP(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0);

	const u8 * CAT_RESTRICT a = p - 4; // A
	const u8 * CAT_RESTRICT b = p - xsize*4; // B
	const u8 * CAT_RESTRICT c = b - 4; // C

	temp[0] = abcClamp(a[0], b[0], c[0]);
	temp[1] = abcClamp(a[1], b[1], c[1]);
	temp[2] = abcClamp(a[2], b[2], c[2]);
	return temp;
}


//// Paeth Filter

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

static const u8 *SFF_PAETH(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 4; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize*4; // B
			const u8 * CAT_RESTRICT c = b - 4; // C

			temp[0] = paeth(a[0], b[0], c[0]);
			temp[1] = paeth(a[1], b[1], c[1]);
			temp[2] = paeth(a[2], b[2], c[2]);
			return temp;
		} else {
			return a;
		}
	} else if (y > 0) {
		return p - xsize*4; // B
	}

	return FPZ;
}

static const u8 *SFFU_PAETH(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0);

	const u8 * CAT_RESTRICT a = p - 4; // A
	const u8 * CAT_RESTRICT b = p - xsize*4; // B
	const u8 * CAT_RESTRICT c = b - 4; // C

	temp[0] = paeth(a[0], b[0], c[0]);
	temp[1] = paeth(a[1], b[1], c[1]);
	temp[2] = paeth(a[2], b[2], c[2]);
	return temp;
}


//// ABC Paeth Filter

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

static const u8 *SFF_ABC_PAETH(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 4; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize*4; // B
			const u8 * CAT_RESTRICT c = b - 4; // C

			temp[0] = abc_paeth(a[0], b[0], c[0]);
			temp[1] = abc_paeth(a[1], b[1], c[1]);
			temp[2] = abc_paeth(a[2], b[2], c[2]);
			return temp;
		} else {
			return a;
		}
	} else if (y > 0) {
		return p - xsize*4; // B
	}

	return FPZ;
}

static const u8 *SFFU_ABC_PAETH(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0);

	const u8 * CAT_RESTRICT a = p - 4; // A
	const u8 * CAT_RESTRICT b = p - xsize*4; // B
	const u8 * CAT_RESTRICT c = b - 4; // C

	temp[0] = abc_paeth(a[0], b[0], c[0]);
	temp[1] = abc_paeth(a[1], b[1], c[1]);
	temp[2] = abc_paeth(a[2], b[2], c[2]);
	return temp;
}


//// Offset PL Filter

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
	}

	return b + a - c;
}

static const u8 *SFF_PLO(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 4; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize*4; // B

			const u8 * CAT_RESTRICT src = b; // B
			if (x < xsize-1) {
				src += 4; // D
			}

			temp[0] = predLevel(a[0], src[0], b[0]);
			temp[1] = predLevel(a[1], src[1], b[1]);
			temp[2] = predLevel(a[2], src[2], b[2]);
			return temp;
		} else {
			return a;
		}
	} else if (y > 0) {
		return p - xsize*4; // B
	}

	return FPZ;
}

static const u8 *SFFU_PLO(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 4; // A
	const u8 * CAT_RESTRICT b = p - xsize*4; // B
	const u8 * CAT_RESTRICT src = b + 4; // D

	temp[0] = predLevel(a[0], src[0], b[0]);
	temp[1] = predLevel(a[1], src[1], b[1]);
	temp[2] = predLevel(a[2], src[2], b[2]);
	return temp;
}


//// Select Filter

static CAT_INLINE u8 predSelect(int a, int b, int c) {
	// Prediction for current pixel
	int pred = a + b - c;

	// Pick closer distance, preferring A
	if (AbsVal(pred - a) <= AbsVal(pred - b)) {
		return a;
	} else {
		return b;
	}
}

static const u8 *SFF_SELECT(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 4; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize*4; // B
			const u8 * CAT_RESTRICT c = b - 4; // C

			temp[0] = predSelect(a[0], b[0], c[0]);
			temp[1] = predSelect(a[1], b[1], c[1]);
			temp[2] = predSelect(a[2], b[2], c[2]);
			return temp;
		} else {
			return a;
		}
	} else if (y > 0) {
		return p - xsize*4; // B
	}

	return FPZ;
}

static const u8 *SFFU_SELECT(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 4; // A
	const u8 * CAT_RESTRICT b = p - xsize*4; // B
	const u8 * CAT_RESTRICT c = b - 4; // C

	temp[0] = predSelect(a[0], b[0], c[0]);
	temp[1] = predSelect(a[1], b[1], c[1]);
	temp[2] = predSelect(a[2], b[2], c[2]);
	return temp;
}


//// Select F Filter

static CAT_INLINE u8 leftSel(int f, int c, int a) {
	if (AbsVal(f - c) < AbsVal(f - a)) {
		return c;
	} else {
		return a;
	}
}

static const u8 *SFF_SELECT_F(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (x > 1 && y > 0) {
		const u8 * CAT_RESTRICT a = p - 4;
		const u8 * CAT_RESTRICT c = a - xsize*4;
		const u8 * CAT_RESTRICT f = c - 4;

		temp[0] = leftSel(f[0], c[0], a[0]);
		temp[1] = leftSel(f[1], c[1], a[1]);
		temp[2] = leftSel(f[2], c[2], a[2]);
		return temp;
	} else if (x > 0) {
		return p - 4; // A
	} else if (y > 0) {
		return p - xsize*4; // B
	}

	return FPZ;
}

static const u8 *SFFU_SELECT_F(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	if (x > 1) {
		const u8 * CAT_RESTRICT a = p - 4;
		const u8 * CAT_RESTRICT c = a - xsize*4;
		const u8 * CAT_RESTRICT f = c - 4;

		temp[0] = leftSel(f[0], c[0], a[0]);
		temp[1] = leftSel(f[1], c[1], a[1]);
		temp[2] = leftSel(f[2], c[2], a[2]);
		return temp;
	}

	return p - 4; // A
}


//// ED Gradient Filter

static const u8 *SFF_ED_GRAD(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (y > 1 && x < xsize - 2) {
		const u8 * CAT_RESTRICT d = p + 4 - xsize*4;
		const u8 * CAT_RESTRICT e = d + 4 - xsize*4;

		temp[0] = d[0] * 2 - e[0];
		temp[1] = d[1] * 2 - e[1];
		temp[2] = d[2] * 2 - e[2];
		return temp;
	} else if (x > 0) {
		return p - 4; // A
	} else if (y > 0) {
		return p - xsize*4; // B
	}

	return FPZ;
}

static const u8 *SFFU_ED_GRAD(const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) {
	if (y > 1 && x < xsize - 2) {
		const u8 * CAT_RESTRICT d = p + 4 - xsize*4;
		const u8 * CAT_RESTRICT e = d + 4 - xsize*4;

		temp[0] = d[0] * 2 - e[0];
		temp[1] = d[1] * 2 - e[1];
		temp[2] = d[2] * 2 - e[2];
		return temp;
	}

	return p - 4; // A
}


//// Tapped Filters

/*
 * Extended tapped linear filters
 *
 * The taps correspond to coefficients in the expression:
 *
 * Prediction = (t0*A + t1*B + t2*C + t3*D) / 2
 *
 * And the taps are selected from: {-4, -3, -2, -1, 0, 1, 2, 3, 4}.
 *
 * We ran simulations with a number of test images and chose the linear filters
 * of this form that were consistently better than the default spatial filters.
 */

static const int DIV2_FILTER_TAPS[DIV2_TAPPED_COUNT][4] = {
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


//// Static function versions of tapped linear filters

#define DEFINE_TAPS(TAP) \
	static const u8 *SFF_TAPS_ ## TAP (const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) { \
		if (x > 0) { \
			const u8 * CAT_RESTRICT a = p - 4; /* A */ \
			if (y > 0) { \
				const u8 * CAT_RESTRICT b = p - xsize*4; /* B */ \
				const u8 * CAT_RESTRICT c = b - 4; /* C */ \
				const u8 * CAT_RESTRICT d = b; /* B */ \
				if (x < xsize-1) { \
					d += 4; /* D */ \
				} \
				static const int TA = DIV2_FILTER_TAPS[TAP][0]; \
				static const int TB = DIV2_FILTER_TAPS[TAP][1]; \
				static const int TC = DIV2_FILTER_TAPS[TAP][2]; \
				static const int TD = DIV2_FILTER_TAPS[TAP][3]; \
				temp[0] = (TA*a[0] + TB*b[0] + TC*c[0] + TD*d[0]) >> 1; \
				temp[1] = (TA*a[1] + TB*b[1] + TC*c[1] + TD*d[1]) >> 1; \
				temp[2] = (TA*a[2] + TB*b[2] + TC*c[2] + TD*d[2]) >> 1; \
				return temp; \
			} else { \
				return a; \
			} \
		} else if (y > 0) { \
			return p - xsize*4; /* B */ \
		} \
		return FPZ; \
	} \
	static const u8 *SFFU_TAPS_ ## TAP (const u8 * CAT_RESTRICT p, u8 * CAT_RESTRICT temp, int x, int y, int xsize) { \
		CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1); \
		const u8 * CAT_RESTRICT a = p - 4; \
		const u8 * CAT_RESTRICT b = p - xsize*4; \
		const u8 * CAT_RESTRICT c = b - 4; \
		const u8 * CAT_RESTRICT d = b + 4; \
		static const int TA = DIV2_FILTER_TAPS[TAP][0]; \
		static const int TB = DIV2_FILTER_TAPS[TAP][1]; \
		static const int TC = DIV2_FILTER_TAPS[TAP][2]; \
		static const int TD = DIV2_FILTER_TAPS[TAP][3]; \
		temp[0] = (TA*a[0] + TB*b[0] + TC*c[0] + TD*d[0]) >> 1; \
		temp[1] = (TA*a[1] + TB*b[1] + TC*c[1] + TD*d[1]) >> 1; \
		temp[2] = (TA*a[2] + TB*b[2] + TC*c[2] + TD*d[2]) >> 1; \
		return temp; \
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


//// RGBA Filter Function Table

const RGBAFilterFuncs cat::RGBA_FILTERS[SF_COUNT] = {
	{ SFF_A, SFFU_A },
	{ SFF_B, SFFU_B },
	{ SFF_C, SFFU_C },
	{ SFF_D, SFFU_D },
	{ SFF_Z, SFFU_Z },
	{ SFF_AVG_AB, SFFU_AVG_AB },
	{ SFF_AVG_AC, SFFU_AVG_AC },
	{ SFF_AVG_AD, SFFU_AVG_AD },
	{ SFF_AVG_BC, SFFU_AVG_BC },
	{ SFF_AVG_BD, SFFU_AVG_BD },
	{ SFF_AVG_CD, SFFU_AVG_CD },
	{ SFF_AVG_AB1, SFFU_AVG_AB1 },
	{ SFF_AVG_AC1, SFFU_AVG_AC1 },
	{ SFF_AVG_AD1, SFFU_AVG_AD1 },
	{ SFF_AVG_BC1, SFFU_AVG_BC1 },
	{ SFF_AVG_BD1, SFFU_AVG_BD1 },
	{ SFF_AVG_CD1, SFFU_AVG_CD1 },
	{ SFF_AVG_ABC, SFFU_AVG_ABC },
	{ SFF_AVG_ACD, SFFU_AVG_ACD },
	{ SFF_AVG_ABD, SFFU_AVG_ABD },
	{ SFF_AVG_BCD, SFFU_AVG_BCD },
	{ SFF_AVG_ABCD, SFFU_AVG_ABCD },
	{ SFF_AVG_ABCD1, SFFU_AVG_ABCD1 },
	{ SFF_CLAMP_GRAD, SFFU_CLAMP_GRAD },
	{ SFF_SKEW_GRAD, SFFU_SKEW_GRAD },
	{ SFF_ABC_CLAMP, SFFU_ABC_CLAMP },
	{ SFF_PAETH, SFFU_PAETH },
	{ SFF_ABC_PAETH, SFFU_ABC_PAETH },
	{ SFF_PLO, SFFU_PLO },
	{ SFF_SELECT, SFFU_SELECT },
	{ SFF_SELECT_F, SFFU_SELECT_F },
	{ SFF_ED_GRAD, SFFU_ED_GRAD },
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
	LIST_TAPS(75), LIST_TAPS(76), LIST_TAPS(77), LIST_TAPS(78), LIST_TAPS(79)
};

#undef LIST_TAPS


//// Simple Spatial Filters

static u8 MFF_A(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (x > 0) {
		return p[-1]; // A
	} else if (y > 0) {
		return p[-xsize]; // B
	}

	return 0;
}

static u8 MFFU_A(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0);

	return p[-1]; // A
}

static u8 MFF_Z(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	return 0;
}

#define MFFU_Z MFF_Z

static u8 MFF_B(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (y > 0) {
		return p[-xsize]; // B
	} else if (x > 0) {
		return p[-1]; // A
	}

	return 0;
}

static u8 MFFU_B(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(y > 0);

	return p[-xsize]; // B
}

static u8 MFF_C(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (x > 0) {
		if (y > 0) {
			return p[-xsize - 1]; // C
		} else {
			return p[-1]; // A
		}
	} else if (y > 0) {
		return p[-xsize]; // B
	}

	return 0;
}

static u8 MFFU_C(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0);

	return p[-xsize - 1]; // C
}

static u8 MFF_D(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT fp = p - xsize; // B
		if (x < xsize-1) {
			++fp; // D
		}
		return *fp;
	} else if (x > 0) {
		return p[-1]; // A
	}

	return 0;
}

static u8 MFFU_D(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(y > 0 && x < xsize-1);

	return p[-xsize + 1]; // D
}


//// Dual Average Filters (Round Down)

static u8 MFF_AVG_AB(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT a = p - 1; // A

			return (a[0] + (u16)b[0]) >> 1;
		} else {
			return b[0]; // B
		}
	} else if (x > 0) {
		return p[-1]; // A
	}

	return 0;
}

static u8 MFFU_AVG_AB(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 1; // A
	const u8 * CAT_RESTRICT b = p - xsize; // B

	return (a[0] + (u16)b[0]) >> 1;
}

static u8 MFF_AVG_AC(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT a = p - 1; // A
			const u8 * CAT_RESTRICT c = b - 1; // C

			return (a[0] + (u16)c[0]) >> 1;
		} else {
			return b[0]; // B
		}
	} else if (x > 0) {
		return p[-1]; // A
	}

	return 0;
}

static u8 MFFU_AVG_AC(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 1; // A
	const u8 * CAT_RESTRICT c = p - xsize - 1; // C

	return (a[0] + (u16)c[0]) >> 1;
}

static u8 MFF_AVG_AD(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT src = p - xsize; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT a = p - 1; // A
			if (x < xsize-1) {
				src++; // D
			}

			return (a[0] + (u16)src[0]) >> 1;
		} else {
			return src[0]; // B
		}
	} else if (x > 0) {
		return p[-1]; // A
	}

	return 0;
}

static u8 MFFU_AVG_AD(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 1; // A
	const u8 * CAT_RESTRICT d = p - xsize + 1; // D

	return (a[0] + (u16)d[0]) >> 1;
}

static u8 MFF_AVG_BC(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT c = b - 1; // C

			return (b[0] + (u16)c[0]) >> 1;
		} else {
			return b[0]; // B
		}
	} else if (x > 0) {
		return p[-1]; // A
	}

	return 0;
}

static u8 MFFU_AVG_BC(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT b = p - xsize; // B
	const u8 * CAT_RESTRICT c = b - 1; // C

	return (b[0] + (u16)c[0]) >> 1;
}

static u8 MFF_AVG_BD(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT src = b - 1; // C
			if (x < xsize-1) {
				src += 2; // D
			}

			return (b[0] + (u16)src[0]) >> 1;
		} else {
			return b[0]; // B
		}
	} else if (x > 0) {
		return p[-1]; // A
	}

	return 0;
}

static u8 MFFU_AVG_BD(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT b = p - xsize; // B
	const u8 * CAT_RESTRICT d = b + 1; // D

	return (b[0] + (u16)d[0]) >> 1;
}

static u8 MFF_AVG_CD(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT src = p - xsize; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT c = src - 1; // C
			if (x < xsize-1) {
				++src; // D
			}

			return (c[0] + (u16)src[0]) >> 1;
		} else {
			return src[0]; // B
		}
	} else if (x > 0) {
		return p[-1]; // A
	}

	return 0;
}

static u8 MFFU_AVG_CD(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT c = p - xsize - 1; // C
	const u8 * CAT_RESTRICT d = c + 2; // D

	return (c[0] + (u16)d[0]) >> 1;
}


//// Dual Average Filters (Round Up)

static u8 MFF_AVG_AB1(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT a = p - 1; // A

			return (a[0] + (u16)b[0] + 1) >> 1;
		} else {
			return b[0]; // B
		}
	} else if (x > 0) {
		return p[-1]; // A
	}

	return 0;
}

static u8 MFFU_AVG_AB1(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 1; // A
	const u8 * CAT_RESTRICT b = p - xsize; // B

	return (a[0] + (u16)b[0] + 1) >> 1;
}

static u8 MFF_AVG_AC1(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT a = p - 1; // A
			const u8 * CAT_RESTRICT c = b - 1; // C

			return (a[0] + (u16)c[0] + 1) >> 1;
		} else {
			return b[0]; // B
		}
	} else if (x > 0) {
		return p[-1]; // A
	}

	return 0;
}

static u8 MFFU_AVG_AC1(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 1; // A
	const u8 * CAT_RESTRICT c = p - xsize - 1; // C

	return (a[0] + (u16)c[0] + 1) >> 1;
}

static u8 MFF_AVG_AD1(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT src = p - xsize; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT a = p - 1; // A
			if (x < xsize-1) {
				++src; // D
			}

			return (a[0] + (u16)src[0] + 1) >> 1;
		} else {
			return src[0]; // B
		}
	} else if (x > 0) {
		return p[-1]; // A
	}

	return 0;
}

static u8 MFFU_AVG_AD1(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 1; // A
	const u8 * CAT_RESTRICT d = p - xsize + 1; // D

	return (a[0] + (u16)d[0] + 1) >> 1;
}

static u8 MFF_AVG_BC1(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT c = b - 1; // C

			return (b[0] + (u16)c[0] + 1) >> 1;
		} else {
			return b[0]; // B
		}
	} else if (x > 0) {
		return p[-1]; // A
	}

	return 0;
}

static u8 MFFU_AVG_BC1(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT b = p - xsize; // B
	const u8 * CAT_RESTRICT c = b - 1; // C

	return (b[0] + (u16)c[0] + 1) >> 1;
}

static u8 MFF_AVG_BD1(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize; // B

		if (x < xsize-1) {
			const u8 * CAT_RESTRICT d = b + 1; // D
			return (b[0] + (u16)d[0] + 1) >> 1;
		} else if (x > 0) {
			const u8 * CAT_RESTRICT c = b - 1; // C
			return (b[0] + (u16)c[0] + 1) >> 1;
		} else {
			return b[0]; // B
		}
	} else if (x > 0) {
		return p[-1]; // A
	}

	return 0;
}

static u8 MFFU_AVG_BD1(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT b = p - xsize; // B
	const u8 * CAT_RESTRICT d = b + 1; // D

	return (b[0] + (u16)d[0] + 1) >> 1;
}

static u8 MFF_AVG_CD1(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (y > 0) {
		const u8 * CAT_RESTRICT src = p - xsize; // B

		if (x > 0) {
			const u8 * CAT_RESTRICT c = src - 1; // C
			if (x < xsize-1) {
				++src; // D
			}

			return (c[0] + (u16)src[0] + 1) >> 1;
		} else {
			return src[0]; // B
		}
	} else if (x > 0) {
		return p[-1]; // A
	}

	return 0;
}

static u8 MFFU_AVG_CD1(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT c = p - xsize - 1; // C
	const u8 * CAT_RESTRICT d = c + 2; // D

	return (c[0] + (u16)d[0] + 1) >> 1;
}


//// Triple Average Filters (Round Down)

static u8 MFF_AVG_ABC(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 1; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize; // B
			const u8 * CAT_RESTRICT c = b - 1; // C

			return (a[0] + (u16)b[0] + c[0]) / 3;
		} else {
			return a[0];
		}
	} else if (y > 0) {
		return p[-xsize]; // B
	}

	return 0;
}

static u8 MFFU_AVG_ABC(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 1; // A
	const u8 * CAT_RESTRICT b = p - xsize; // B
	const u8 * CAT_RESTRICT c = b - 1; // C

	return (a[0] + (u16)b[0] + c[0]) / 3;
}

static u8 MFF_AVG_ACD(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 1; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize; // B
			const u8 * CAT_RESTRICT c = b - 1; // C

			const u8 * CAT_RESTRICT src = b; // B
			if (x < xsize-1) {
				src++; // D
			}

			return (a[0] + (u16)c[0] + src[0]) / 3;
		} else {
			return a[0];
		}
	} else if (y > 0) {
		const u8 * CAT_RESTRICT src = p - xsize; // B
		if CAT_LIKELY(x < xsize-1) {
			src++; // D
		}

		return src[0];
	}

	return 0;
}

static u8 MFFU_AVG_ACD(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 1; // A
	const u8 * CAT_RESTRICT b = p - xsize; // B
	const u8 * CAT_RESTRICT c = b - 1; // C
	const u8 * CAT_RESTRICT d = b + 1; // D

	return (a[0] + (u16)c[0] + d[0]) / 3;
}

static u8 MFF_AVG_ABD(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 1; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize; // B

			const u8 * CAT_RESTRICT src = b; // B
			if (x < xsize-1) {
				src++; // D
			}

			return (a[0] + (u16)b[0] + src[0]) / 3;
		} else {
			return a[0];
		}
	} else if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize; // B
		const u8 * CAT_RESTRICT d = b; // D
		if CAT_LIKELY(x < xsize-1) {
			b++;
		}

		return (b[0] + (u16)d[0]) >> 1;
	}

	return 0;
}

static u8 MFFU_AVG_ABD(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 1; // A
	const u8 * CAT_RESTRICT b = p - xsize; // B
	const u8 * CAT_RESTRICT d = b + 1; // D

	return (a[0] + (u16)b[0] + d[0]) / 3;
}

static u8 MFF_AVG_BCD(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 1; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize; // B
			const u8 * CAT_RESTRICT c = b - 1; // C

			const u8 * CAT_RESTRICT src = b; // B
			if (x < xsize-1) {
				src++; // D
			}

			return (b[0] + (u16)c[0] + src[0]) / 3;
		} else {
			return a[0];
		}
	} else if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize; // B
		const u8 * CAT_RESTRICT d = b; // B
		if CAT_LIKELY(x < xsize-1) {
			b++; // D
		}

		return (b[0] + (u16)d[0]) >> 1;
	}

	return 0;
}

static u8 MFFU_AVG_BCD(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT b = p - xsize; // B
	const u8 * CAT_RESTRICT c = b - 1; // C
	const u8 * CAT_RESTRICT d = b + 1; // D

	return (b[0] + (u16)c[0] + d[0]) / 3;
}


//// Quad Average Filters (Round Down)

static u8 MFF_AVG_ABCD(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 1; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize; // B
			const u8 * CAT_RESTRICT c = b - 1; // C

			const u8 * CAT_RESTRICT src = b; // B
			if (x < xsize-1) {
				src++; // D
			}

			return (a[0] + (u16)b[0] + c[0] + (u16)src[0]) >> 2;
		} else {
			return a[0];
		}
	} else if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize; // B
		const u8 * CAT_RESTRICT d = b; // D
		if CAT_LIKELY(x < xsize-1) {
			b++;
		}

		return (b[0] + (u16)d[0]) >> 1;
	}

	return 0;
}

static u8 MFFU_AVG_ABCD(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 1; // A
	const u8 * CAT_RESTRICT b = p - xsize; // B
	const u8 * CAT_RESTRICT c = b - 1; // C
	const u8 * CAT_RESTRICT d = b + 1; // D

	return (a[0] + (u16)b[0] + c[0] + (u16)d[0]) >> 2;
}


//// Quad Average Filters (Round Up)

static u8 MFF_AVG_ABCD1(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 1; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize; // B
			const u8 * CAT_RESTRICT c = b - 1; // C

			const u8 * CAT_RESTRICT src = b; // B
			if (x < xsize-1) {
				src++; // D
			}

			return (a[0] + (u16)b[0] + c[0] + (u16)src[0] + 2) >> 2;
		} else {
			return a[0];
		}
	} else if (y > 0) {
		const u8 * CAT_RESTRICT b = p - xsize; // B
		const u8 * CAT_RESTRICT d = b; // D
		if CAT_LIKELY(x < xsize-1) {
			b++;
		}

		return (b[0] + (u16)d[0] + 1) >> 1;
	}

	return 0;
}

static u8 MFFU_AVG_ABCD1(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 1; // A
	const u8 * CAT_RESTRICT b = p - xsize; // B
	const u8 * CAT_RESTRICT c = b - 1; // C
	const u8 * CAT_RESTRICT d = b + 1; // D

	return (a[0] + (u16)b[0] + c[0] + (u16)d[0] + 2) >> 2;
}


//// Clamp Gradient Filter

static u8 MFF_CLAMP_GRAD(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (y > 0) {
		if (x > 0) {
			const u8 * CAT_RESTRICT a = p - 1; // A
			const u8 * CAT_RESTRICT b = p - xsize; // B
			const u8 * CAT_RESTRICT c = b - 1; // C

			return clampGrad(b[0], a[0], c[0]);
		} else {
			return p[-xsize]; // B
		}
	} else if (x > 0) {
		return p[-1]; // A
	}

	return 0;
}

static u8 MFFU_CLAMP_GRAD(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 1; // A
	const u8 * CAT_RESTRICT b = p - xsize; // B
	const u8 * CAT_RESTRICT c = b - 1; // C

	return clampGrad(b[0], a[0], c[0]);
}


//// Skewed Gradient Filter

static CAT_INLINE u8 skewGradMono(int b, int a, int c, int num_syms) {
	int pred = (3 * (b + a) - (c << 1)) >> 2;
	if (pred >= num_syms) {
		return (u8)(num_syms - 1);
	} else if (pred <= 0) {
		return 0;
	}
	return pred;
}

static u8 MFF_SKEW_GRAD(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (y > 0) {
		if (x > 0) {
			const u8 * CAT_RESTRICT a = p - 1; // A
			const u8 * CAT_RESTRICT b = p - xsize; // B
			const u8 * CAT_RESTRICT c = b - 1; // C

			return skewGradMono(b[0], a[0], c[0], num_syms);
		} else {
			return p[-xsize]; // B
		}
	} else if (x > 0) {
		return p[-1]; // A
	}

	return 0;
}

static u8 MFFU_SKEW_GRAD(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 1; // A
	const u8 * CAT_RESTRICT b = p - xsize; // B
	const u8 * CAT_RESTRICT c = b - 1; // C

	return skewGradMono(b[0], a[0], c[0], num_syms);
}


//// ABC Clamp Filter

static CAT_INLINE u8 abcClampMono(int a, int b, int c, int num_syms) {
	int sum = a + b - c;
	if (sum <= 0) {
		return 0;
	} else if (sum >= num_syms) {
		return (u8)(num_syms - 1);
	}

	return sum;
}

static u8 MFF_ABC_CLAMP(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 1; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize; // B
			const u8 * CAT_RESTRICT c = b - 1; // C

			return abcClampMono(a[0], b[0], c[0], num_syms);
		} else {
			return a[0];
		}
	} else if (y > 0) {
		return p[-xsize]; // B
	}

	return 0;
}

static u8 MFFU_ABC_CLAMP(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0);

	const u8 * CAT_RESTRICT a = p - 1; // A
	const u8 * CAT_RESTRICT b = p - xsize; // B
	const u8 * CAT_RESTRICT c = b - 1; // C

	return abcClampMono(a[0], b[0], c[0], num_syms);
}


//// Paeth Filter

static u8 MFF_PAETH(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 1; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize; // B
			const u8 * CAT_RESTRICT c = b - 1; // C

			return paeth(a[0], b[0], c[0]);
		} else {
			return a[0];
		}
	} else if (y > 0) {
		return p[-xsize]; // B
	}

	return 0;
}

static u8 MFFU_PAETH(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0);

	const u8 * CAT_RESTRICT a = p - 1; // A
	const u8 * CAT_RESTRICT b = p - xsize; // B
	const u8 * CAT_RESTRICT c = b - 1; // C

	return paeth(a[0], b[0], c[0]);
}


//// ABC Paeth Filter

static u8 MFF_ABC_PAETH(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 1; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize; // B
			const u8 * CAT_RESTRICT c = b - 1; // C

			return abc_paeth(a[0], b[0], c[0]);
		} else {
			return a[0];
		}
	} else if (y > 0) {
		return p[-xsize]; // B
	}

	return 0;
}

static u8 MFFU_ABC_PAETH(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0);

	const u8 * CAT_RESTRICT a = p - 1; // A
	const u8 * CAT_RESTRICT b = p - xsize; // B
	const u8 * CAT_RESTRICT c = b - 1; // C

	return abc_paeth(a[0], b[0], c[0]);
}


//// Offet PL Filter

static u8 MFF_PLO(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 1; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize; // B

			const u8 * CAT_RESTRICT src = b; // B
			if (x < xsize-1) {
				++src; // D
			}

			return predLevel(a[0], src[0], b[0]);
		} else {
			return a[0];
		}
	} else if (y > 0) {
		return p[-xsize]; // B
	}

	return 0;
}

static u8 MFFU_PLO(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 1; // A
	const u8 * CAT_RESTRICT b = p - xsize; // B
	const u8 * CAT_RESTRICT src = b + 1; // D

	return predLevel(a[0], src[0], b[0]);
}


//// Select Filter

static u8 MFF_SELECT(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (x > 0) {
		const u8 * CAT_RESTRICT a = p - 1; // A

		if (y > 0) {
			const u8 * CAT_RESTRICT b = p - xsize; // B
			const u8 * CAT_RESTRICT c = b - 1; // C

			return predSelect(a[0], b[0], c[0]);
		} else {
			return a[0];
		}
	} else if (y > 0) {
		return p[-xsize]; // B
	}

	return 0;
}

static u8 MFFU_SELECT(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	const u8 * CAT_RESTRICT a = p - 1; // A
	const u8 * CAT_RESTRICT b = p - xsize; // B
	const u8 * CAT_RESTRICT c = b - 1; // C

	return predSelect(a[0], b[0], c[0]);
}


//// Select F Filter

static u8 MFF_SELECT_F(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (x > 1 && y > 0) {
		const u8 * CAT_RESTRICT a = p - 1;
		const u8 * CAT_RESTRICT c = a - xsize;
		const u8 * CAT_RESTRICT f = c - 1;

		return leftSel(f[0], c[0], a[0]);
	} else if (x > 0) {
		return p[-1]; // A
	} else if (y > 0) {
		return p[-xsize]; // B
	}

	return 0;
}

static u8 MFFU_SELECT_F(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1);

	if (x > 1) {
		const u8 * CAT_RESTRICT a = p - 1;
		const u8 * CAT_RESTRICT c = a - xsize;
		const u8 * CAT_RESTRICT f = c - 1;

		return leftSel(f[0], c[0], a[0]);
	}

	return p[-1]; // A
}


//// E-D Gradient Filter

static u8 MFF_ED_GRAD(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (y > 1 && x < xsize - 2) {
		const u8 * CAT_RESTRICT d = p + 1 - xsize;
		const u8 * CAT_RESTRICT e = d + 1 - xsize;

		int v = (int)d[0] * 2 - e[0];
		if (v <= 0) {
			v = 0;
		} else if (v >= num_syms) {
			v -= num_syms;
		}
		return (u8)v;
	} else if (x > 0) {
		return p[-1]; // A
	} else if (y > 0) {
		return p[-xsize]; // B
	}

	return 0;
}

static u8 MFFU_ED_GRAD(const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) {
	if (y > 1 && x < xsize - 2) {
		const u8 * CAT_RESTRICT d = p + 1 - xsize;
		const u8 * CAT_RESTRICT e = d + 1 - xsize;

		int v = (int)d[0] * 2 - e[0];
		if (v <= 0) {
			v = 0;
		} else if (v >= num_syms) {
			v -= num_syms;
		}
		return (u8)v;
	}

	return p[-1]; // A
}


//// PaletteFilterSet

#define DEFINE_TAPS(TAP) \
	static u8 MFF_TAPS_ ## TAP (const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) { \
		if (x > 0) { \
			const u8 * CAT_RESTRICT a = p - 1; /* A */ \
			if (y > 0) { \
				const u8 * CAT_RESTRICT b = p - xsize; /* B */ \
				const u8 * CAT_RESTRICT c = b - 1; /* C */ \
				const u8 * CAT_RESTRICT d = b; /* B */ \
				if (x < xsize-1) { \
					d += 1; /* D */ \
				} \
				static const int TA = DIV2_FILTER_TAPS[TAP][0]; \
				static const int TB = DIV2_FILTER_TAPS[TAP][1]; \
				static const int TC = DIV2_FILTER_TAPS[TAP][2]; \
				static const int TD = DIV2_FILTER_TAPS[TAP][3]; \
				int result = (TA*a[0] + TB*b[0] + TC*c[0] + TD*d[0]) >> 1; \
				while (result >= num_syms) result -= num_syms; \
				while (result < 0) result += num_syms; \
				return result; \
			} else { \
				return *a; \
			} \
		} else if (y > 0) { \
			return p[-xsize]; /* B */ \
		} \
		return 0; \
	} \
	static u8 MFFU_TAPS_ ## TAP (const u8 * CAT_RESTRICT p, u16 num_syms, int x, int y, int xsize) { \
		CAT_DEBUG_ENFORCE(x > 0 && y > 0 && x < xsize-1); \
		const u8 * CAT_RESTRICT a = p - 1; \
		const u8 * CAT_RESTRICT b = p - xsize; \
		const u8 * CAT_RESTRICT c = b - 1; \
		const u8 * CAT_RESTRICT d = b + 1; \
		static const int TA = DIV2_FILTER_TAPS[TAP][0]; \
		static const int TB = DIV2_FILTER_TAPS[TAP][1]; \
		static const int TC = DIV2_FILTER_TAPS[TAP][2]; \
		static const int TD = DIV2_FILTER_TAPS[TAP][3]; \
		int result = (TA*a[0] + TB*b[0] + TC*c[0] + TD*d[0]) >> 1; \
		while (result >= num_syms) result -= num_syms; \
		while (result < 0) result += num_syms; \
		return result; \
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
	{ MFF_TAPS_ ## TAP, MFFU_TAPS_ ## TAP }


//// Monochrome Filter Function Table

const MonoFilterFuncs cat::MONO_FILTERS[SF_COUNT] = {
	{ MFF_A, MFFU_A },
	{ MFF_B, MFFU_B },
	{ MFF_C, MFFU_C },
	{ MFF_D, MFFU_D },
	{ MFF_Z, MFFU_Z },
	{ MFF_AVG_AB, MFFU_AVG_AB },
	{ MFF_AVG_AC, MFFU_AVG_AC },
	{ MFF_AVG_AD, MFFU_AVG_AD },
	{ MFF_AVG_BC, MFFU_AVG_BC },
	{ MFF_AVG_BD, MFFU_AVG_BD },
	{ MFF_AVG_CD, MFFU_AVG_CD },
	{ MFF_AVG_AB1, MFFU_AVG_AB1 },
	{ MFF_AVG_AC1, MFFU_AVG_AC1 },
	{ MFF_AVG_AD1, MFFU_AVG_AD1 },
	{ MFF_AVG_BC1, MFFU_AVG_BC1 },
	{ MFF_AVG_BD1, MFFU_AVG_BD1 },
	{ MFF_AVG_CD1, MFFU_AVG_CD1 },
	{ MFF_AVG_ABC, MFFU_AVG_ABC },
	{ MFF_AVG_ACD, MFFU_AVG_ACD },
	{ MFF_AVG_ABD, MFFU_AVG_ABD },
	{ MFF_AVG_BCD, MFFU_AVG_BCD },
	{ MFF_AVG_ABCD, MFFU_AVG_ABCD },
	{ MFF_AVG_ABCD1, MFFU_AVG_ABCD1 },
	{ MFF_CLAMP_GRAD, MFFU_CLAMP_GRAD },
	{ MFF_SKEW_GRAD, MFFU_SKEW_GRAD },
	{ MFF_ABC_CLAMP, MFFU_ABC_CLAMP },
	{ MFF_PAETH, MFFU_PAETH },
	{ MFF_ABC_PAETH, MFFU_ABC_PAETH },
	{ MFF_PLO, MFFU_PLO },
	{ MFF_SELECT, MFFU_SELECT },
	{ MFF_SELECT_F, MFFU_SELECT_F },
	{ MFF_ED_GRAD, MFFU_ED_GRAD },
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
	LIST_TAPS(75), LIST_TAPS(76), LIST_TAPS(77), LIST_TAPS(78), LIST_TAPS(79)
};

#undef LIST_TAPS


//// Color Filters: RGB -> YUV

void CFF_R2Y_GB_RG(const u8 * CAT_RESTRICT rgb, u8 * CAT_RESTRICT yuv) {
	const u8 G = rgb[1], B = rgb[2];
	yuv[0] = B;
	yuv[1] = G - B;
	yuv[2] = G - rgb[0];
}

void CFF_R2Y_GR_BG(const u8 * CAT_RESTRICT rgb, u8 * CAT_RESTRICT yuv) {
	const u8 R = rgb[0], G = rgb[1];
	yuv[0] = G - rgb[2];
	yuv[1] = G - R;
	yuv[2] = R;
}

void CFF_R2Y_YUVr(const u8 * CAT_RESTRICT rgb, u8 * CAT_RESTRICT yuv) {
	const u8 G = rgb[1];
	const u8 U = rgb[2] - G;
	const u8 V = rgb[0] - G;
	yuv[0] = G + (((s8)U + (s8)V) >> 2);
	yuv[1] = U;
	yuv[2] = V;
}

void CFF_R2Y_D9(const u8 * CAT_RESTRICT rgb, u8 * CAT_RESTRICT yuv) {
	const u8 R = rgb[0];
	const u8 G = rgb[1];
	yuv[0] = R;
	yuv[1] = rgb[2] - static_cast<u8>( (R + (static_cast<u16>( G ) << 1) + G) >> 2 );
	yuv[2] = G - R;
}

void CFF_R2Y_D12(const u8 * CAT_RESTRICT rgb, u8 * CAT_RESTRICT yuv) {
	const u8 B = rgb[2];
	const u8 R = rgb[0];
	yuv[0] = B;
	yuv[1] = rgb[1] - static_cast<u8>( (B + (static_cast<u16>( R ) << 1) + R) >> 2 );
	yuv[2] = R - B;
}

void CFF_R2Y_D8(const u8 * CAT_RESTRICT rgb, u8 * CAT_RESTRICT yuv) {
	const u8 R = rgb[0];
	const u8 G = rgb[1];
	yuv[0] = R;
	yuv[1] = rgb[2] - static_cast<u8>( (R + static_cast<u16>( G )) >> 1 );
	yuv[2] = G - R;
}

void CFF_R2Y_E2_R(const u8 * CAT_RESTRICT rgb, u8 * CAT_RESTRICT yuv) {
	const s8 Co = rgb[0] - rgb[1];
	const s16 t = rgb[1] + (Co >> 1);
	const s8 Cg = static_cast<s8>( rgb[2] - t );
	yuv[0] = static_cast<u8>( t + (Cg >> 1) );
	yuv[1] = Cg;
	yuv[2] = Co;
}

void CFF_R2Y_BG_RG(const u8 * CAT_RESTRICT rgb, u8 * CAT_RESTRICT yuv) {
	const u8 G = rgb[1];
	yuv[0] = G - rgb[2];
	yuv[1] = G;
	yuv[2] = G - rgb[0];
}

void CFF_R2Y_GR_BR(const u8 * CAT_RESTRICT rgb, u8 * CAT_RESTRICT yuv) {
	const u8 R = rgb[0];
	yuv[0] = rgb[2] - R;
	yuv[1] = rgb[1] - R;
	yuv[2] = R;
}

void CFF_R2Y_D18(const u8 * CAT_RESTRICT rgb, u8 * CAT_RESTRICT yuv) {
	const u8 B = rgb[2];
	const u8 G = rgb[1];
	yuv[0] = B;
	yuv[1] = rgb[0] - static_cast<u8>( (B + (static_cast<u16>( G ) << 1) + G) >> 2 );
	yuv[2] = G - B;
}

void CFF_R2Y_B_GR_R(const u8 * CAT_RESTRICT rgb, u8 * CAT_RESTRICT yuv) {
	const u8 R = rgb[0];
	yuv[0] = rgb[2];
	yuv[1] = rgb[1] - R;
	yuv[2] = R;
}

void CFF_R2Y_D11(const u8 * CAT_RESTRICT rgb, u8 * CAT_RESTRICT yuv) {
	const u8 B = rgb[2];
	const u8 R = rgb[0];
	yuv[0] = B;
	yuv[1] = rgb[1] - static_cast<u8>( (R + static_cast<u16>( B )) >> 1 );
	yuv[2] = R - B;
}

void CFF_R2Y_D14(const u8 * CAT_RESTRICT rgb, u8 * CAT_RESTRICT yuv) {
	const u8 R = rgb[0];
	const u8 B = rgb[2];
	yuv[0] = R;
	yuv[1] = rgb[1] - static_cast<u8>( (R + static_cast<u16>( B )) >> 1 );
	yuv[2] = B - R;
}

void CFF_R2Y_D10(const u8 * CAT_RESTRICT rgb, u8 * CAT_RESTRICT yuv) {
	const u8 R = rgb[0];
	const u8 B = rgb[2];
	yuv[0] = B;
	yuv[1] = rgb[1] - static_cast<u8>( (R + (static_cast<u16>( B ) << 1) + B) >> 2 );
	yuv[2] = R - B;
}

void CFF_R2Y_YCgCo_R(const u8 * CAT_RESTRICT rgb, u8 * CAT_RESTRICT yuv) {
	const s8 Co = rgb[0] - rgb[2];
	const s16 t = rgb[2] + (Co >> 1);
	const s8 Cg = static_cast<s8>( rgb[1] - t );
	yuv[0] = static_cast<u8>( t + (Cg >> 1) );
	yuv[1] = Cg;
	yuv[2] = Co;
}

void CFF_R2Y_GB_RB(const u8 * CAT_RESTRICT rgb, u8 * CAT_RESTRICT yuv) {
	const u8 B = rgb[2];
	yuv[0] = B;
	yuv[1] = rgb[1] - B;
	yuv[2] = rgb[0] - B;
}

void CFF_R2Y_NONE(const u8 * CAT_RESTRICT rgb, u8 * CAT_RESTRICT yuv) {
	yuv[0] = rgb[2];
	yuv[1] = rgb[1];
	yuv[2] = rgb[0];
}

const RGB2YUVFilterFunction cat::RGB2YUV_FILTERS[CF_COUNT] = {
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
	CFF_R2Y_GB_RB,
	CFF_R2Y_NONE
};


//// Color Filters: YUV -> RGB

void CFF_Y2R_GB_RG(const u8 * CAT_RESTRICT yuv, u8 * CAT_RESTRICT rgb) {
	const u8 B = yuv[0];
	const u8 G = yuv[1] + B;
	rgb[0] = G - yuv[2];
	rgb[1] = G;
	rgb[2] = B;
}

void CFF_Y2R_GR_BG(const u8 * CAT_RESTRICT yuv, u8 * CAT_RESTRICT rgb) {
	const u8 R = yuv[2];
	const u8 G = yuv[1] + R;
	rgb[2] = G - yuv[0];
	rgb[1] = G;
	rgb[0] = R;
}

void CFF_Y2R_YUVr(const u8 * CAT_RESTRICT yuv, u8 * CAT_RESTRICT rgb) {
	const s8 U = yuv[1];
	const s8 V = yuv[2];
	const u8 G = yuv[0] - static_cast<u8>( (U + static_cast<s16>( V )) >> 2 );
	rgb[0] = V + G;
	rgb[2] = U + G;
	rgb[1] = G;
}

void CFF_Y2R_D9(const u8 * CAT_RESTRICT yuv, u8 * CAT_RESTRICT rgb) {
	const u8 R = yuv[0];
	const u8 G = yuv[2] + R;
	rgb[2] = yuv[1] + static_cast<u8>( (R + (static_cast<u16>( G ) << 1) + G) >> 2 );
	rgb[0] = R;
	rgb[1] = G;
}

void CFF_Y2R_D12(const u8 * CAT_RESTRICT yuv, u8 * CAT_RESTRICT rgb) {
	const u8 B = yuv[0];
	const u8 R = B + yuv[2];
	rgb[1] = yuv[1] + static_cast<u8>( (B + (static_cast<u16>( R ) << 1) + R) >> 2 );
	rgb[0] = R;
	rgb[2] = B;
}

void CFF_Y2R_D8(const u8 * CAT_RESTRICT yuv, u8 * CAT_RESTRICT rgb) {
	const u8 R = yuv[0];
	const u8 G = yuv[2] + R;
	rgb[2] = yuv[1] + static_cast<u8>( (R + static_cast<u16>( G )) >> 1 );
	rgb[0] = R;
	rgb[1] = G;
}

void CFF_Y2R_E2_R(const u8 * CAT_RESTRICT yuv, u8 * CAT_RESTRICT rgb) {
	const s8 Co = yuv[2];
	const s8 Cg = yuv[1];
	const s16 t = yuv[0] - (Cg >> 1);
	rgb[2] = static_cast<u8>( Cg + t );
	const u8 G = static_cast<u8>( t - (Co >> 1) );
	rgb[0] = Co + G;
	rgb[1] = G;
}

void CFF_Y2R_BG_RG(const u8 * CAT_RESTRICT yuv, u8 * CAT_RESTRICT rgb) {
	const u8 G = yuv[1];
	rgb[2] = G - yuv[0];
	rgb[0] = G - yuv[2];
	rgb[1] = G;
}

void CFF_Y2R_GR_BR(const u8 * CAT_RESTRICT yuv, u8 * CAT_RESTRICT rgb) {
	const u8 R = yuv[2];
	rgb[2] = yuv[0] + R;
	rgb[1] = yuv[1] + R;
	rgb[0] = R;
}

void CFF_Y2R_D18(const u8 * CAT_RESTRICT yuv, u8 * CAT_RESTRICT rgb) {
	const u8 B = yuv[0];
	const u8 G = yuv[2] + B;
	rgb[0] = yuv[1] + static_cast<u8>( (B + (static_cast<u16>( G ) << 1) + G) >> 2 );
	rgb[1] = G;
	rgb[2] = B;
}

void CFF_Y2R_B_GR_R(const u8 * CAT_RESTRICT yuv, u8 * CAT_RESTRICT rgb) {
	const u8 R = yuv[2];
	rgb[1] = yuv[1] + R;
	rgb[2] = yuv[0];
	rgb[0] = R;
}

void CFF_Y2R_D11(const u8 * CAT_RESTRICT yuv, u8 * CAT_RESTRICT rgb) {
	const u8 B = yuv[0];
	const u8 R = yuv[2] + B;
	rgb[1] = yuv[1] + static_cast<u8>( (B + static_cast<u16>( R )) >> 1 );
	rgb[0] = R;
	rgb[2] = B;
}

void CFF_Y2R_D14(const u8 * CAT_RESTRICT yuv, u8 * CAT_RESTRICT rgb) {
	const u8 R = yuv[0];
	const u8 B = yuv[2] + R;
	rgb[1] = yuv[1] + static_cast<u8>( (R + static_cast<u16>( B )) >> 1 );
	rgb[0] = R;
	rgb[2] = B;
}

void CFF_Y2R_D10(const u8 * CAT_RESTRICT yuv, u8 * CAT_RESTRICT rgb) {
	const u8 B = yuv[0];
	const u8 R = yuv[2] + B;
	rgb[1] = yuv[1] + static_cast<u8>( (R + (static_cast<u16>( B ) << 1) + B) >> 2 );
	rgb[0] = R;
	rgb[2] = B;
}

void CFF_Y2R_YCgCo_R(const u8 * CAT_RESTRICT yuv, u8 * CAT_RESTRICT rgb) {
	const s8 Co = yuv[2];
	const s8 Cg = yuv[1];
	const s16 t = yuv[0] - (Cg >> 1);
	rgb[1] = Cg + t;
	const u8 B = static_cast<u8>( t - (Co >> 1) );
	rgb[0] = Co + B;
	rgb[2] = B;
}

void CFF_Y2R_GB_RB(const u8 * CAT_RESTRICT yuv, u8 * CAT_RESTRICT rgb) {
	const u8 B = yuv[0];
	rgb[1] = yuv[1] + B;
	rgb[0] = yuv[2] + B;
	rgb[2] = B;
}

void CFF_Y2R_NONE(const u8 * CAT_RESTRICT yuv, u8 * CAT_RESTRICT rgb) {
	rgb[2] = yuv[0];
	rgb[1] = yuv[1];
	rgb[0] = yuv[2];
}

const YUV2RGBFilterFunction cat::YUV2RGB_FILTERS[CF_COUNT] = {
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
	CFF_Y2R_GB_RB,
	CFF_Y2R_NONE
};

/*

   Simple verification program:

int main() {
	cout << "Color filter verification" << endl;

	u8 rgb[3], yuv[3], test[3];
	for (int ii = 0; ii < 256; ++ii) {
		rgb[0] = ii;
		for (int jj = 0; jj < 256; ++jj) {
			rgb[1] = jj;
			for (int kk = 0; kk < 256; ++kk) {
				rgb[2] = kk;
				for (int f = 0; f < CF_COUNT; ++f) {
					RGB2YUV_FILTERS[f](rgb, yuv);
					YUV2RGB_FILTERS[f](yuv, test);

					if (test[0] != ii || test[1] != jj || test[2] != kk) {
						cout << "Color filter " << f << " failed test" << endl;
						return 1;
					}
				}
			}
		}
	}

	cout << "All filters are reversible for all input" << endl;

	return 0;
}

 */
