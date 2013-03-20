#ifndef CAT_HUFFMAN_ENCODER_H
#define CAT_HUFFMAN_ENCODER_H

#include "Platform.hpp"

namespace cat {

namespace huffman {

static const u32 cHuffmanMaxSupportedSyms = 256;

struct sym_freq
{
	u32 freq;
	u16 left, right;

	CAT_INLINE bool operator<(const sym_freq &other) const
	{
		return freq > other.freq;
	}
};


struct huffman_work_tables {
	enum { cMaxInternalNodes = cHuffmanMaxSupportedSyms };

	sym_freq syms0[cHuffmanMaxSupportedSyms + 1 + cMaxInternalNodes];
	sym_freq syms1[cHuffmanMaxSupportedSyms + 1 + cMaxInternalNodes];
};


bool generate_huffman_codes(huffman_work_tables *state, u32 num_syms, const u16 *pFreq, u8 *pCodesizes, u32 &max_code_size, u32 &total_freq_ret) {


} // namespace huffman

} // namespace cat

#endif // CAT_HUFFMAN_ENCODER_H

