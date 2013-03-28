#ifndef CAT_HUFFMAN_DECODER_H
#define CAT_HUFFMAN_DECODER_H

#include "Platform.hpp"

// See copyright notice at the top of HuffmanEncoder.hpp

namespace cat {

namespace huffman {


static const u32 cMaxExpectedCodeSize = 16;
static const u32 cMaxTableBits = 11; // Time-memory tradeoff LUT optimization limit


struct decoder_tables {
	u32 num_syms;
	u32 max_codes[cMaxExpectedCodeSize + 1];
	int val_ptrs[cMaxExpectedCodeSize + 1];
	u32 total_used_syms;

	u32 cur_sorted_symbol_order_size;
	u16 *sorted_symbol_order;

	u8 min_code_size, max_code_size;

	u32 table_bits;

	u32 *lookup;
	u32 cur_lookup_size;

	u32 table_max_code;
	u32 decode_start_code_size;

	u32 table_shift;
};


void init_decoder_tables(decoder_tables *pTables);

bool generate_decoder_tables(u32 num_syms, const u8 *pCodesizes, decoder_tables *pTables, u32 table_bits);

void clean_decoder_tables(decoder_tables *pTables);



class HuffmanDecoder {
	decoder_tables _tables;

	bool _eof;

	u32 *_words;
	int _wordsLeft;

	u32 _bits;
	int _bitsLeft;

	u32 _nextWord;
	int _nextLeft;

public:
	bool init(u32 *words, int wordCount);

	u32 next();

	CAT_INLINE bool isEOF() {
		return _eof;
	}
};


} // namespace huffman

} // namespace cat

#endif // CAT_HUFFMAN_DECODER_H

