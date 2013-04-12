#ifndef ENTROPY_ENCODER_HPP
#define ENTROPY_ENCODER_HPP

#include "Platform.hpp"
#include "ImageWriter.hpp"
#include "EntropyDecoder.hpp"
#include <vector>

namespace cat {


//// EntropyEncoder

class EntropyEncoder {
#ifdef USE_AZ
	static const int AZ_SYMS = EntropyDecoder::AZ_SYMS;
#endif
	static const int BZ_SYMS = EntropyDecoder::BZ_SYMS;
	static const int FILTER_RLE_SYMS = EntropyDecoder::FILTER_RLE_SYMS;

	u32 histBZ[BZ_SYMS], maxBZ;
#ifdef USE_AZ
	u32 histAZ[AZ_SYMS], maxAZ;
#endif
	u32 zeroRun;

	std::vector<int> runList;
	int runListReadIndex;

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

	void reset();

	void endSymbols();

	int writeZeroRun(int run, ImageWriter &writer);

public:
	CAT_INLINE EntropyEncoder() {
		reset();
	}
	CAT_INLINE virtual ~EntropyEncoder() {
	}

	void push(u8 symbol);
	void finalize();

	u32 writeOverhead(ImageWriter &writer);
	u32 encode(u8 symbol, ImageWriter &writer);
};


} // namespace cat

#endif // ENTROPY_ENCODER_HPP

