#ifndef CAT_HUFFMAN_DECODER_H
#define CAT_HUFFMAN_DECODER_H

#include "Platform.hpp"

// See copyright notice at the top of HuffmanEncoder.hpp

namespace cat {


//// HuffmanDecoder

class HuffmanDecoder {
public:
	static const u32 MAX_CODE_SIZE = 16;

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

	static const u32 MAX_TABLE_BITS = 11; // Time-memory tradeoff LUT optimization limit

	bool init(int num_syms, const u8 codelens[], u32 table_bits);

	// Returns symbol, bitlength
	CAT_INLINE u32 get(u32 code, u32 &bitLength) {
		u32 k = static_cast<u32>((code >> 16) + 1);
		u32 sym, len;

		if (k <= _table_max_code) {
			u32 t = _lookup[code >> (32 - _table_bits)];

			sym = static_cast<u16>( t );
			len = static_cast<u16>( t >> 16 );
		}
		else {
			len = _decode_start_code_size;

			const u32 *max_codes = _max_codes;

			for (;;) {
				if (k <= max_codes[len - 1])
					break;
				len++;
			}

			int val_ptr = _val_ptrs[len - 1] + static_cast<int>((code >> (32 - len)));

			if (((u32)val_ptr >= _num_syms)) {
				bitLength = len;
				return 0;
			}

			sym = _sorted_symbol_order[val_ptr];
		}

		bitLength = len;
		return sym;
	}
};


} // namespace cat

#endif // CAT_HUFFMAN_DECODER_H

