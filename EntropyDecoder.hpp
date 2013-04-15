#ifndef ENTROPY_DECODER_HPP
#define ENTROPY_DECODER_HPP

#include "Platform.hpp"
#include "HuffmanDecoder.hpp"
#include "ImageReader.hpp"

namespace cat {


//#define ADAPTIVE_ZRLE
#define ADAPTIVE_ZRLE_THRESH 10 /* percent */

#define USE_AZ /* After-Zero Context (Pseudo-Order-1) */



//// EntropyDecoder

class EntropyDecoder {
public:
	static const int FILTER_RLE_SYMS = 128; // Number of symbols set apart for zRLE
	static const int BZ_SYMS = 256 + FILTER_RLE_SYMS;
#ifdef USE_AZ
	static const int AZ_SYMS = 256;
#endif

protected:
	int _zeroRun;
	HuffmanDecoder _bz;
#ifdef USE_AZ
	HuffmanDecoder _az;
	bool _afterZero;
#endif

	void clear();

public:
	CAT_INLINE EntropyDecoder() {
	}
	virtual CAT_INLINE ~EntropyDecoder() {
		clear();
	}

	bool init(ImageReader &reader);

	u8 next(ImageReader &reader);
};

} // namespace cat

#endif // ENTROPY_DECODER_HPP

