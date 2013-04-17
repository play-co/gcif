#ifndef ENTROPY_ENCODER_HPP
#define ENTROPY_ENCODER_HPP

#include "Platform.hpp"
#include "ImageWriter.hpp"
#include "EntropyDecoder.hpp"
#include "HuffmanEncoder.hpp"
#include <vector>

namespace cat {


//// EntropyEncoder

class EntropyEncoder {
#ifdef USE_AZ
	static const int AZ_SYMS = EntropyDecoder::AZ_SYMS;
#endif
	static const int BZ_SYMS = EntropyDecoder::BZ_SYMS;
	static const int FILTER_RLE_SYMS = EntropyDecoder::FILTER_RLE_SYMS;
	static const int RECENT_SYMS = EntropyDecoder::RECENT_SYMS;

	FreqHistogram<BZ_SYMS> bz_hist;
#ifdef USE_AZ
	FreqHistogram<AZ_SYMS> az_hist;
#endif
	u32 zeroRun;

	std::vector<int> runList;
	int runListReadIndex;

#ifdef ADAPTIVE_ZRLE
	u32 zeros, total;
	bool usingZ;
#endif

	HuffmanEncoder<BZ_SYMS> bz_encoder;

#ifdef USE_AZ
	HuffmanEncoder<AZ_SYMS> az_encoder;
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

