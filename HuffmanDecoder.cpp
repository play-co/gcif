#include "HuffmanDecoder.hpp"
#include "HuffmanEncoder.hpp"
#include "BitMath.hpp"
#include "Log.hpp"
#include "EndianNeutral.hpp"
using namespace cat;
using namespace huffman;


//// HuffmanDecoder

void HuffmanDecoder::clear() {
	if (_sorted_symbol_order) {
		delete []_sorted_symbol_order;
		_sorted_symbol_order = 0;
	}

	if (_lookup) {
		delete []_lookup;
		_lookup = 0;
	}
}

bool HuffmanDecoder::init(int count, const u8 *codelens, u32 table_bits) {
	clear();

	u32 min_codes[MAX_CODE_SIZE];

	if (count <= 0 || (table_bits > MAX_TABLE_BITS)) {
		return false;
	}

	_num_syms = count;

	// Codelen histogram
	u32 num_codes[MAX_CODE_SIZE + 1] = { 0 };
	for (u32 ii = 0; ii < _num_syms; ++ii) {
		num_codes[codelens[ii]]++;
	}

	u32 sorted_positions[MAX_CODE_SIZE + 1];

	u32 next_code = 0;
	u32 total_used_syms = 0;
	u32 max_code_size = 0;
	u32 min_code_size = 0x7fffffff;

	for (u32 ii = 1; ii <= MAX_CODE_SIZE; ++ii) {
		const u32 n = num_codes[ii];

		if (!n) {
			_max_codes[ii - 1] = 0;
		} else {
			min_code_size = min_code_size < ii ? min_code_size : ii;
			max_code_size = max_code_size > ii ? max_code_size : ii;

			min_codes[ii - 1] = next_code;

			_max_codes[ii - 1] = next_code + n - 1;
			_max_codes[ii - 1] = 1 + ((_max_codes[ii - 1] << (16 - ii)) | ((1 << (16 - ii)) - 1));

			_val_ptrs[ii - 1] = total_used_syms;

			sorted_positions[ii] = total_used_syms;

			next_code += n;
			total_used_syms += n;
		}

		next_code <<= 1;
	}

	_total_used_syms = total_used_syms;

	if (total_used_syms > _cur_sorted_symbol_order_size) {
		_cur_sorted_symbol_order_size = total_used_syms;

		if (!CAT_IS_POWER_OF_2(total_used_syms)) {
			u32 nextPOT = NextHighestPow2(total_used_syms);

			_cur_sorted_symbol_order_size = count < nextPOT ? count : nextPOT;
		}

		_sorted_symbol_order = new u16[_cur_sorted_symbol_order_size + 6000];
	}

	_min_code_size = static_cast<u8>( min_code_size );
	_max_code_size = static_cast<u8>( max_code_size );

	for (u16 sym = 0; sym < count; ++sym) {
		int len = codelens[sym];
		if (len > 0) {
			int spos = sorted_positions[len]++;
			_sorted_symbol_order[ spos ] = sym;
		}
	}

	if (table_bits <= _min_code_size) {
		table_bits = 0;
	}

	_table_bits = table_bits;

	if (table_bits > 0) {
		u32 table_size = 1 << table_bits;
		if (_cur_lookup_size < table_size) {
			_cur_lookup_size = table_size;

			_lookup = new u32[table_size + 6000];
		}

		memset(_lookup, 0xFF, 4 << table_bits);

		for (u32 codesize = 1; codesize <= table_bits; ++codesize) {
			if (!num_codes[codesize]) {
				continue;
			}

			const u32 fillsize = table_bits - codesize;
			const u32 fillnum = 1 << fillsize;

			const u32 min_code = min_codes[codesize - 1];
			u32 max_code = _max_codes[codesize - 1];
			if (!max_code) {
				max_code = 0xffffffff;
			} else {
				max_code = (max_code - 1) >> (16 - codesize);
			}
			const u32 val_ptr = _val_ptrs[codesize - 1];

			for (u32 code = min_code; code <= max_code; code++) {
				const u32 sym_index = _sorted_symbol_order[ val_ptr + code - min_code ];

				for (u32 jj = 0; jj < fillnum; ++jj) {
					const u32 tt = jj + (code << fillsize);

					_lookup[tt] = sym_index | (codesize << 16U);
				}
			}
		}
	}         

	for (u32 ii = 0; ii < MAX_CODE_SIZE; ++ii) {
		_val_ptrs[ii] -= min_codes[ii];
	}

	_table_max_code = 0;
	_decode_start_code_size = _min_code_size;

	if (table_bits > 0) {
		u32 ii;

		for (ii = table_bits; ii >= 1; --ii) {
			if (num_codes[ii]) {
				_table_max_code = _max_codes[ii - 1];
				break;
			}
		}

		if (ii >= 1) {
			_decode_start_code_size = table_bits + 1;

			for (ii = table_bits + 1; ii <= max_code_size; ++ii) {
				if (num_codes[ii]) {
					_decode_start_code_size = ii;
					break;
				}
			}
		}
	}

	// sentinels
	_max_codes[MAX_CODE_SIZE] = 0xffffffff;
	_val_ptrs[MAX_CODE_SIZE] = 0xFFFFF;

	_table_shift = 32 - _table_bits;

	return true;
}

bool HuffmanDecoder::init(int num_syms, ImageReader &reader, u32 table_bits) {
	static const int HUFF_SYMS = MAX_CODE_SIZE + 1;
	u8 codelens[huffman::cHuffmanMaxSupportedSyms];

	CAT_ENFORCE(HUFF_SYMS == 17);
	CAT_ENFORCE(num_syms >= 2);

	// Shaved?

	if (reader.readBit()) {
		int num_syms_bits = BSR32(num_syms - 1) + 1;

		int shaved = reader.readBits(num_syms_bits) + 1;
		if (shaved >= num_syms) {
			// Invalid input
			return false;
		}

		for (int ii = shaved; ii < num_syms; ++ii) {
			codelens[ii] = 0;
		}

		num_syms = shaved;
	}

	// If the symbol count is low,
	if (num_syms <= 20) {
		for (int ii = 0; ii < num_syms; ++ii) {
			u8 len = reader.readBits(4);

			if (len >= 15) {
				len += reader.readBit();
			}

			codelens[ii] = len;
		}

		return init(num_syms, codelens, table_bits);
	}

	// Read the table decoder codelens
	u8 table_codelens[HUFF_SYMS];
	for (int ii = 0; ii < HUFF_SYMS; ++ii) {
		u8 len = reader.readBits(4);

		if (len >= 15) {
			len += reader.readBit();
		}

		table_codelens[ii] = len;
	}

	// Initialize the table decoder
	HuffmanDecoder table_decoder;
	if (!table_decoder.init(HUFF_SYMS, table_codelens, 8)) {
		// Init fail
		return false;
	}

	u32 nonzero_count = 0, nonzero_sym = 0;

	// Read the method chosen
	switch (reader.readBits(2)) {
	default:
	case 0:
		{
			for (int ii = 0; ii < num_syms; ++ii) {
				u32 len = table_decoder.next(reader);

				if (len > 0) {
					nonzero_count++;
					nonzero_sym = ii;
				}

				codelens[ii] = len;
			}
		}
		break;
	case 1:
		{
			u8 prev = 1;
			for (int ii = 0; ii < num_syms; ++ii) {
				u32 sym = table_decoder.next(reader);

				u8 len = (sym + prev) % HUFF_SYMS;
				prev = len;

				if (len > 0) {
					nonzero_count++;
					nonzero_sym = ii;
				}

				codelens[ii] = len;
			}
		}
		break;
	case 2:
		{
			u8 prev = 1;
			const int half = num_syms/2;
			for (int ii = 0; ii < half; ++ii) {
				u32 sym = table_decoder.next(reader);

				u8 len = (sym + prev) % HUFF_SYMS;
				prev = len;

				codelens[ii] = len;
			}
			for (int ii = half; ii < num_syms; ++ii) {
				u32 sym = table_decoder.next(reader);

				u8 len = (prev - sym + HUFF_SYMS) % HUFF_SYMS;
				prev = len;

				if (len > 0) {
					nonzero_count++;
					nonzero_sym = ii;
				}

				codelens[ii] = len;
			}
		}
		break;
	case 3:
		{
			u32 lag0 = 1, lag1 = 1;
			for (int ii = 0; ii < num_syms; ++ii) {
				u32 sym = table_decoder.next(reader);

				u32 pred = (lag0 + lag1) >> 1;

				u8 len = (sym + pred) % HUFF_SYMS;
				lag1 = lag0;
				lag0 = len;

				if (len > 0) {
					nonzero_count++;
					nonzero_sym = ii;
				}

				codelens[ii] = len;
			}
		}
		break;
	}

	// If only one symbol,
	if (nonzero_count == 1) {
		_one_sym = nonzero_sym + 1;
		return true;
	} else {
		_one_sym = 0;
		return init(num_syms, codelens, table_bits);
	}
}

u32 HuffmanDecoder::next(ImageReader &reader) {
	// If only one symbol,
	const u32 one_sym = _one_sym;
	if (one_sym) {
		// Does not require any bits to store
		return one_sym - 1;
	}

	// Read next 32-bit file chunk
	u32 code = reader.peek(16);
	u32 k = static_cast<u32>((code >> 16) + 1);
	u32 sym, len;

	// If the symbol can be looked up in the table,
	if CAT_LIKELY(k <= _table_max_code) {
		u32 t = _lookup[code >> (32 - _table_bits)];

		// Seriously that fast.
		sym = static_cast<u16>( t );
		len = static_cast<u16>( t >> 16 );
	}
	else {
		// Handle longer codelens outside of table
		len = _decode_start_code_size;
		const u32 *max_codes = _max_codes;

		for (;;) {
			if CAT_LIKELY(k <= max_codes[len - 1])
				break;
			len++;
		}

		// Decode from codelen to code
		int val_ptr = _val_ptrs[len - 1] + static_cast<int>((code >> (32 - len)));

		if CAT_UNLIKELY(((u32)val_ptr >= _num_syms)) {
			return 0;
		}

		sym = _sorted_symbol_order[val_ptr];
	}

	// Consume bits used for symbol
	reader.eat(len);
	return sym;
}

