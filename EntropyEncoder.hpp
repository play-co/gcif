#ifndef ENTROPY_ENCODER_HPP
#define ENTROPY_ENCODER_HPP

#include "Platform.hpp"
#include "ImageWriter.hpp"

namespace cat {


//#define ADAPTIVE_ZRLE
#define USE_AZ /* After-Zero Context (Pseudo-Order-1) */

static const int FILTER_RLE_SYMS = 8; // Number of symbols set apart for zRLE


//// EntropyEncoder

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

	void endSymbols();
	void normalizeFreqs(u32 max_freq, int num_syms, u32 hist[], u16 freqs[]);
	void reset();

public:
	CAT_INLINE EntropyEncoder() {
		reset();
	}
	CAT_INLINE virtual ~EntropyEncoder() {
	}

	void push(u8 symbol);
	void finalize();

	u32 encode(u8 symbol, ImageWriter &writer);
	u32 encodeFinalize(ImageWriter &writer);
};


} // namespace cat

#endif // ENTROPY_ENCODER_HPP

