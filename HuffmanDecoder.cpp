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

		if (_sorted_symbol_order) {
			delete []_sorted_symbol_order;
		}

		_sorted_symbol_order = new u16[_cur_sorted_symbol_order_size];
		if (!_sorted_symbol_order) {
			return false;
		}
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

			if (_lookup) {
				delete []_lookup;
			}

			_lookup = new u32[table_size];
			if (!_lookup) {
				return false;
			}
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

