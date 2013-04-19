#ifndef CAT_HUFFMAN_DECODER_HPP
#define CAT_HUFFMAN_DECODER_HPP

#include "Platform.hpp"
#include "ImageReader.hpp"

// See copyright notice at the top of HuffmanEncoder.hpp

namespace cat {


//// HuffmanDecoder

class HuffmanDecoder {
public:
	static const u32 MAX_CODE_SIZE = 16; // Max bits per Huffman code (16 is upper limit)
	static const u32 MAX_TABLE_BITS = 11; // Time-memory tradeoff LUT optimization limit
	static const int TABLE_THRESH = 20; // Number of symbols before table is compressed

protected:
	u32 _num_syms;
	u32 _max_codes[MAX_CODE_SIZE + 1];
	int _val_ptrs[MAX_CODE_SIZE + 1];
	u32 _total_used_syms;

	u32 _cur_sorted_symbol_order_size;
	u16 *_sorted_symbol_order;

	u8 _min_code_size, _max_code_size;

	u32 _table_bits;

	u32 *_lookup;
	u32 _cur_lookup_size;

	u32 _table_max_code;
	u32 _decode_start_code_size;

	u32 _table_shift;

	u32 _one_sym;

	void clear();

public:
	CAT_INLINE HuffmanDecoder() {
		_cur_sorted_symbol_order_size = 0;
		_sorted_symbol_order = 0;

		_lookup = 0;
		_cur_lookup_size = 0;
	}
	CAT_INLINE virtual ~HuffmanDecoder() {
		clear();
	}

	bool init(int num_syms, const u8 codelens[], u32 table_bits);
	bool init(int num_syms, ImageReader &reader, u32 table_bits);

	u32 next(ImageReader &reader);
};


// Decoder for Huffman tables
class HuffmanTableDecoder {
	static const int NUM_SYMS = HuffmanDecoder::MAX_CODE_SIZE + 2;

	HuffmanDecoder _decoder;
	int _zeroRun;

public:
	bool init(ImageReader &reader);

	u8 next(ImageReader &reader);
};


} // namespace cat

#endif // CAT_HUFFMAN_DECODER_HPP

