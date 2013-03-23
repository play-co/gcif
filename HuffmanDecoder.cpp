#include "HuffmanDecoder.hpp"
#include "HuffmanEncoder.hpp"
#include "BitMath.hpp"
#include "Log.hpp"
#include "EndianNeutral.hpp"
using namespace cat;
using namespace huffman;

#include <iostream>
using namespace std;

void huffman::init_decoder_tables(decoder_tables *pTables) {
	pTables->cur_sorted_symbol_order_size = 0;
	pTables->sorted_symbol_order = 0;

	pTables->lookup = 0;
	pTables->cur_lookup_size = 0;
}

void huffman::clean_decoder_tables(decoder_tables *pTables) {
	if (pTables->sorted_symbol_order) {
		delete []pTables->sorted_symbol_order;
		pTables->sorted_symbol_order = 0;
	}

	if (pTables->lookup) {
		delete []pTables->lookup;
		pTables->lookup = 0;
	}
}

bool huffman::generate_decoder_tables(u32 num_syms, const u8 *pCodesizes, decoder_tables *pTables, u32 table_bits) {
	u32 min_codes[cMaxExpectedCodeSize];

	if ((!num_syms) || (table_bits > cMaxTableBits)) {
		return false;
	}

	pTables->num_syms = num_syms;

	u32 num_codes[cMaxExpectedCodeSize + 1] = { 0 };

	for (u32 ii = 0; ii < num_syms; ++ii) {
		num_codes[pCodesizes[ii]]++;
	}

	u32 sorted_positions[cMaxExpectedCodeSize + 1];

	u32 next_code = 0;
	u32 total_used_syms = 0;
	u32 max_code_size = 0;
	u32 min_code_size = 0x7fffffff;

	for (u32 ii = 1; ii <= cMaxExpectedCodeSize; ++ii) {
		const u32 n = num_codes[ii];

		if (!n) {
			pTables->max_codes[ii - 1] = 0;
		} else {
			min_code_size = min_code_size < ii ? min_code_size : ii;
			max_code_size = max_code_size > ii ? max_code_size : ii;

			min_codes[ii - 1] = next_code;

			pTables->max_codes[ii - 1] = next_code + n - 1;
			pTables->max_codes[ii - 1] = 1 + ((pTables->max_codes[ii - 1] << (16 - ii)) | ((1 << (16 - ii)) - 1));

			pTables->val_ptrs[ii - 1] = total_used_syms;

			sorted_positions[ii] = total_used_syms;

			next_code += n;
			total_used_syms += n;
		}

		next_code <<= 1;
	}

	pTables->total_used_syms = total_used_syms;

	if (total_used_syms > pTables->cur_sorted_symbol_order_size) {
		pTables->cur_sorted_symbol_order_size = total_used_syms;

		if (!CAT_IS_POWER_OF_2(total_used_syms)) {
			u32 nextPOT = NextHighestPow2(total_used_syms);

			pTables->cur_sorted_symbol_order_size = num_syms < nextPOT ? num_syms : nextPOT;
		}

		if (pTables->sorted_symbol_order) {
			delete []pTables->sorted_symbol_order;
		}

		pTables->sorted_symbol_order = new u16[pTables->cur_sorted_symbol_order_size];
		if (!pTables->sorted_symbol_order) {
			return false;
		}
	}

	pTables->min_code_size = static_cast<u8>( min_code_size );
	pTables->max_code_size = static_cast<u8>( max_code_size );

	for (u16 sym = 0; sym < num_syms; ++sym) {
		int codesize = pCodesizes[sym];
		if (codesize) {
			int spos = sorted_positions[codesize]++;
			pTables->sorted_symbol_order[ spos ] = sym;
		}
	}

	if (table_bits <= pTables->min_code_size) {
		table_bits = 0;
	}

	pTables->table_bits = table_bits;

	if (table_bits) {
		u32 table_size = 1 << table_bits;
		if (table_size > pTables->cur_lookup_size) {
			pTables->cur_lookup_size = table_size;

			if (pTables->lookup) {
				delete []pTables->lookup;
			}

			pTables->lookup = new u32[table_size];
			if (!pTables->lookup) {
				return false;
			}
		}

		memset(pTables->lookup, 0xFF, 4 << table_bits);

		for (u32 codesize = 1; codesize <= table_bits; ++codesize) {
			if (!num_codes[codesize]) {
				continue;
			}

			const u32 fillsize = table_bits - codesize;
			const u32 fillnum = 1 << fillsize;

			const u32 min_code = min_codes[codesize - 1];
			u32 max_code = pTables->max_codes[codesize - 1];
			if (!max_code) {
				max_code = 0xffffffff;
			} else {
				max_code = (max_code - 1) >> (16 - codesize);
			}
			const u32 val_ptr = pTables->val_ptrs[codesize - 1];

			for (u32 code = min_code; code <= max_code; code++) {
				const u32 sym_index = pTables->sorted_symbol_order[ val_ptr + code - min_code ];

				for (u32 jj = 0; jj < fillnum; ++jj) {
					const u32 tt = jj + (code << fillsize);

					pTables->lookup[tt] = sym_index | (codesize << 16U);
				}
			}
		}
	}         

	for (u32 ii = 0; ii < cMaxExpectedCodeSize; ++ii) {
		pTables->val_ptrs[ii] -= min_codes[ii];
	}

	pTables->table_max_code = 0;
	pTables->decode_start_code_size = pTables->min_code_size;

	if (table_bits) {
		u32 ii;

		for (ii = table_bits; ii >= 1; --ii) {
			if (num_codes[ii]) {
				pTables->table_max_code = pTables->max_codes[ii - 1];
				break;
			}
		}

		if (ii >= 1) {
			pTables->decode_start_code_size = table_bits + 1;

			for (ii = table_bits + 1; ii <= max_code_size; ++ii) {
				if (num_codes[ii]) {
					pTables->decode_start_code_size = ii;
					break;
				}
			}
		}
	}

	// sentinels
	pTables->max_codes[cMaxExpectedCodeSize] = 0xffffffff;
	pTables->val_ptrs[cMaxExpectedCodeSize] = 0xFFFFF;

	pTables->table_shift = 32 - pTables->table_bits;

	return true;
}








bool HuffmanDecoder::init(u32 *words, int wordCount) {
	_hash.init(GCIF_DATA_SEED);

	u32 word;
	int bitsLeft;

	// Decode Golomb-encoded Huffman table

	u8 codelens[256];
	int num_syms = 0;

	{
		if (wordCount-- < 1) {
			return false;
		}
		word = getLE(*words++);
		_hash.hashWord(word);

		u32 pivot = word >> 29;
		word <<= 3;
		bitsLeft = 29;

		CAT_INFO("main") << "Huffman table pivot: " << pivot << " bits";

		u32 pivotMask;
		if (!pivot) {
			pivotMask = 0;
		} else {
			pivotMask = (1 << pivot) - 1;
		}

		int tableWriteIndex = 0;

		int lag0 = 3, lag1 = 3, q = 0;
		for (;;) {
			if (bitsLeft <= 0) {
				if (--wordCount < 0) {
					return false;
				}
				word = getLE(*words++);
				_hash.hashWord(word);
				bitsLeft = 32;
			}

			u32 bit = word >> 31;
			q += bit;
			word <<= 1;
			--bitsLeft;

			if (!bit) {
				u32 result = word;

				if (bitsLeft < pivot) {
					if (--wordCount < 0) {
						return false;
					}
					word = getLE(*words++);
					_hash.hashWord(word);

					int newWordBits = pivot - bitsLeft;
					bitsLeft = 32 - newWordBits;

					result >>= 32 - pivot;
					result |= word >> bitsLeft;

					word <<= newWordBits;
				} else {
					result >>= (32 - pivot);

					bitsLeft -= pivot;
					word <<= pivot;
				}

				result += q << pivot;
				q = 0;

				int orig = result;
				if (orig & 1) {
					orig = -(orig >> 1);
				} else {
					orig >>= 1;
				}

				if (tableWriteIndex < 16) {
					orig += lag0;
				} else {
					orig += lag1;
				}
				lag1 = lag0;
				lag0 = orig;

				codelens[tableWriteIndex] = orig;

				// If we're done,
				if (++tableWriteIndex >= 256) {
					break;
				}
			}
		}
	}

	huffman::init_decoder_tables(&_tables);
	huffman::generate_decoder_tables(256, codelens, &_tables, 8);

	_words = words;
	_wordsLeft = wordCount;

	_bits = word;
	_bitsLeft = bitsLeft;

	_nextWord = 0;
	_nextLeft = 0;

	return true;
}

u32 HuffmanDecoder::next() {
	static const int cBitBufSize = 32;

	// Maintain buffer
	u32 code = _bits;
	int bitsLeft = _bitsLeft;
	cout << "Bits left in buffer " << bitsLeft << endl;
	if (bitsLeft < 16) { // symbols are not longer than this

		// fill back up to a full buffer

		u32 bits = _nextWord;
		int avail = _nextLeft;

		int readBits = 32 - bitsLeft;
		if (avail >= readBits) {
			code |= bits >> bitsLeft;
			bits <<= bitsLeft;
			bitsLeft = 32;
			avail -= readBits;
		} else {
			cout << "Out of bits, using up remaining " << avail << endl;
			code |= bits >> bitsLeft;
			bitsLeft += avail;

			if (_wordsLeft > 0) {
				--_wordsLeft;
				bits = getLE(*_words++);
				_hash.hashWord(bits);

				readBits -= avail;
				code |= bits >> bitsLeft;
				bits <<= readBits;
				avail = 32 - readBits;
			} else {
				cout << "Out of words!" << endl;
				bits = 0;
				avail = 32;
			}

			bitsLeft = 32;
		}

		_nextWord = bits;
		_nextLeft = avail;
		cout << "Next left = " << avail << endl;
	}
	cout << "NOW: Bits left in buffer " << bitsLeft << endl;

	u32 k = static_cast<u32>((code >> (cBitBufSize - 16)) + 1);
	u32 sym, len;

	if (k <= _tables.table_max_code) {
		u32 t = _tables.lookup[code >> (cBitBufSize - _tables.table_bits)];

		sym = static_cast<u16>( t );
		len = static_cast<u16>( t >> 16 );

		cout << "tabular symbol " << sym << " of length " << len << endl;
	}
	else {
		len = _tables.decode_start_code_size;

		for (;;) {
			if (k <= _tables.max_codes[len - 1])
				break;
			len++;
		}

		cout << "non-tabular symbol of length " << len << endl;

		int val_ptr = _tables.val_ptrs[len - 1] + static_cast<int>((code >> (cBitBufSize - len)));

		if (((u32)val_ptr >= _tables.num_syms)) {
			cout << "val_ptr exceeds num_syms" << endl;
			return 0;
		}

		sym = _tables.sorted_symbol_order[val_ptr];

		cout << "sym decoded as " << sym << endl;
	}

	// Remember buffer state
	_bits = code << len;
	_bitsLeft = bitsLeft - len;

	cout << "------------------ RETURNING SYMBOL " << sym << endl;

	return sym;
}

