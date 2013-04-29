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

#ifndef CAT_HUFFMAN_DECODER_HPP
#define CAT_HUFFMAN_DECODER_HPP

#include "Platform.hpp"
#include "ImageReader.hpp"

// See copyright notice at the top of HuffmanEncoder.hpp

namespace cat {


//// HuffmanDecoder

class HuffmanDecoder {
public:
	static const u32 MAX_SYMS = 512; // Maximum symbols in a Huffman encoding
	static const u32 MAX_CODE_SIZE = 16; // Max bits per Huffman code (16 is upper limit)
	static const u32 MAX_TABLE_BITS = 11; // Time-memory tradeoff LUT optimization limit
	static const int TABLE_THRESH = 20; // Number of symbols before table is compressed

protected:
	u32 _num_syms;
	u32 _max_codes[MAX_CODE_SIZE + 1];
	int _val_ptrs[MAX_CODE_SIZE + 1];
	u32 _total_used_syms;

	u16 *_sorted_symbol_order;
	u32 _sorted_symbol_order_alloc;
	u32 _cur_sorted_symbol_order_size;

	u8 _min_code_size, _max_code_size;

	u32 _table_bits;

	u32 *_lookup;
	u32 _lookup_alloc;
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
	static const int NUM_SYMS = HuffmanDecoder::MAX_CODE_SIZE + 1;

	HuffmanDecoder _decoder;
	int _zeroRun;
	bool _lastZero;

public:
	bool init(ImageReader &reader);

	u8 next(ImageReader &reader);
};


} // namespace cat

#endif // CAT_HUFFMAN_DECODER_HPP

