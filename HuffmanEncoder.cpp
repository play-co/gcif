#include "HuffmanEncoder.hpp"
#include "HuffmanDecoder.hpp"
#include "Log.hpp"
#include "BitMath.hpp"
using namespace cat;
using namespace huffman;


// This code is all adapted from LZHAM (see header for license)


static sym_freq *radix_sort_syms(u32 num_syms, sym_freq *syms0, sym_freq *syms1) {
	const u32 cMaxPasses = 2;
	u32 hist[256 * cMaxPasses];

	memset(hist, 0, sizeof(hist[0]) * 256 * cMaxPasses);

	{
		sym_freq *p = syms0;
		sym_freq *q = syms0 + (num_syms >> 1) * 2;

		for (; p != q; p += 2)
		{
			const u32 freq0 = p[0].freq;
			const u32 freq1 = p[1].freq;

			hist[        freq0        & 0xFF]++;
			hist[256 + ((freq0 >>  8) & 0xFF)]++;

			hist[        freq1        & 0xFF]++;
			hist[256 + ((freq1 >>  8) & 0xFF)]++;
		}

		if (num_syms & 1)
		{
			const u32 freq = p->freq;

			hist[        freq        & 0xFF]++;
			hist[256 + ((freq >>  8) & 0xFF)]++;
		}
	}

	sym_freq *pCur_syms = syms0;
	sym_freq *pNew_syms = syms1;

	const u32 total_passes = (hist[256] == num_syms) ? 1 : cMaxPasses;

	for (u32 pass = 0; pass < total_passes; pass++) {
		const u32 *pHist = &hist[pass << 8];

		u32 offsets[256];

		u32 cur_ofs = 0;
		for (u32 i = 0; i < 256; i += 2) {
			offsets[i] = cur_ofs;
			cur_ofs += pHist[i];

			offsets[i+1] = cur_ofs;
			cur_ofs += pHist[i+1];
		}

		const u32 pass_shift = pass << 3;

		sym_freq *p = pCur_syms;
		sym_freq *q = pCur_syms + (num_syms >> 1) * 2;

		for (; p != q; p += 2) {
			u32 c0 = p[0].freq;
			u32 c1 = p[1].freq;

			if (pass) {
				c0 >>= 8;
				c1 >>= 8;
			}

			c0 &= 0xFF;
			c1 &= 0xFF;

			if (c0 == c1) {
				u32 dst_offset0 = offsets[c0];

				offsets[c0] = dst_offset0 + 2;

				pNew_syms[dst_offset0] = p[0];
				pNew_syms[dst_offset0 + 1] = p[1];
			}
			else {
				u32 dst_offset0 = offsets[c0]++;
				u32 dst_offset1 = offsets[c1]++;

				pNew_syms[dst_offset0] = p[0];
				pNew_syms[dst_offset1] = p[1];
			}
		}

		if (num_syms & 1) {
			u32 c = ((p->freq) >> pass_shift) & 0xFF;

			u32 dst_offset = offsets[c];
			offsets[c] = dst_offset + 1;

			pNew_syms[dst_offset] = *p;
		}

		sym_freq *t = pCur_syms;
		pCur_syms = pNew_syms;
		pNew_syms = t;
	}            

	return pCur_syms;
}


/*
 * Original authors:
 * Alistair Moffat < alistair@cs.mu.oz.au >
 * Jyrki Katajainen < jyrki@diku.dk >
 * November 1996
*/
static void calculate_minimum_redundancy(int A[], int n) {
	int root; /* next root node to be used */
	int leaf; /* next leaf to be used */
	int next; /* next value to be assigned */
	int avbl; /* number of available nodes */
	int used; /* number of internal nodes */
	int dpth; /* current depth of leaves */

	/* check for pathological cases */
	if (n == 0) {
		return;
	}
	if (n == 1) {
		A[0] = 0;
		return;
	}

	/* first pass, left to right, setting parent pointers */
	A[0] += A[1];
	root = 0;
	leaf = 2;

	for (next = 1; next < n - 1; ++next) {
		/* select first item for a pairing */
		if (leaf >= n || A[root] < A[leaf]) {
			A[next] = A[root];
			A[root++] = next;
		} else {
			A[next] = A[leaf++];
		}

		/* add on the second item */
		if (leaf >= n || (root < next && A[root] < A[leaf])) {
		   A[next] += A[root];
		   A[root++] = next;
		} else {
		   A[next] += A[leaf++];
		}
	}

	/* second pass, right to left, setting internal depths */
	A[n - 2] = 0;

	for (next = n - 3; next >= 0; --next) {
		A[next] = A[A[next]] + 1;
	}

	/* third pass, right to left, setting leaf depths */
	avbl = 1;
	used = dpth = 0;
	root = n - 2;
	next = n - 1;

	while (avbl > 0) {
		while (root >= 0 && A[root] == dpth) {
			++used;
			--root;
		}

		while (avbl > used) {
			A[next--] = dpth;
			--avbl;
		}

		avbl = 2 * used;
		++dpth;
		used = 0;
	 }
}


bool huffman::generate_huffman_codes(huffman_work_tables *state, u32 num_syms, const u16 *pFreq, u8 *pCodesizes, u32 &max_code_size, u32 &total_freq_ret, u32 &one_sym) {
	one_sym = 0;

	if ((!num_syms) || (num_syms > cHuffmanMaxSupportedSyms)) {
		return false;
	}

	u32 max_freq = 0;
	u32 total_freq = 0;
	u32 num_used_syms = 0;

	for (u32 ii = 0; ii < num_syms; ++ii) {
		u32 freq = pFreq[ii];

		if (!freq) {
			pCodesizes[ii] = 0;
		} else {
			total_freq += freq;
			max_freq = max_freq > freq ? max_freq : freq;

			sym_freq &sf = state->syms0[num_used_syms];
			sf.left = static_cast<u16>( ii );
			sf.right = UINT16_MAX;
			sf.freq = freq;
			++num_used_syms;
		}
	}

	total_freq_ret = total_freq;

	if (num_used_syms == 1) {
		one_sym = state->syms0[0].left - 1;

		return true;
	}

	sym_freq *syms = radix_sort_syms(num_used_syms, state->syms0, state->syms1);

	int x[cHuffmanMaxSupportedSyms];

	for (u32 ii = 0; ii < num_used_syms; ++ii) {
		x[ii] = syms[ii].freq;
	}

	calculate_minimum_redundancy(x, num_used_syms);

	u32 max_len = 0;

	for (u32 ii = 0; ii < num_used_syms; ++ii) {
		u32 len = x[ii];

		max_len = len > max_len ? len : max_len;

		pCodesizes[syms[ii].left] = static_cast<u8>(len);
	}

	max_code_size = max_len;

	return true;
}


bool huffman::limit_max_code_size(u32 num_syms, u8 *pCodesizes, u32 max_code_size) {
	const u32 cMaxEverCodeSize = 34;

	if ((!num_syms) || (num_syms > cHuffmanMaxSupportedSyms) || (max_code_size < 1) || (max_code_size > cMaxEverCodeSize)) {
		return false;
	}

	u32 num_codes[cMaxEverCodeSize + 1] = {0};
	bool should_limit = false;

	for (u32 ii = 0; ii < num_syms; ++ii)
	{
		u32 size = pCodesizes[ii];

		num_codes[size]++;

		if (size > max_code_size) {
			should_limit = true;
		}
	}

	if (!should_limit) {
		return true;
	}

	u32 ofs = 0;
	u32 next_sorted_ofs[cMaxEverCodeSize + 1];

	for (u32 ii = 1; ii <= cMaxEverCodeSize; ++ii) {
		next_sorted_ofs[ii] = ofs;
		ofs += num_codes[ii];
	}

	if ((ofs < 2) || (ofs > cHuffmanMaxSupportedSyms)) {
		return true;
	}

	if (ofs > (1U << max_code_size)) {
		return false;
	}

	for (u32 ii = max_code_size + 1; ii <= cMaxEverCodeSize; ++ii) {
		num_codes[max_code_size] += num_codes[ii];
	}

	// Technique of adjusting tree to enforce maximum code size from LHArc

	u32 total = 0;
	for (u32 ii = max_code_size; ii; --ii) {
		total += (num_codes[ii] << (max_code_size - ii));
	}

	if (total == (1U << max_code_size)) {
		return true;
	}

	do {
		num_codes[max_code_size]--;

		u32 ii;
		for (ii = max_code_size - 1; ii; --ii) {
			if (num_codes[ii]) {
				num_codes[ii]--;
				num_codes[ii + 1] += 2;   
				break;
			}
		}

		if (!ii) {
			return false;
		}

		total--;   
	} while (total != (1U << max_code_size));

	u8 new_codesizes[cHuffmanMaxSupportedSyms];
	u8 *p = new_codesizes;

	for (u32 ii = 1; ii <= max_code_size; ++ii) {
		u32 n = num_codes[ii];

		if (n) {
			memset(p, ii, n);
			p += n;
		}
	}

	for (u32 ii = 0; ii < num_syms; ++ii) {
		const u32 size = pCodesizes[ii];

		if (size) {
			u32 next_ofs = next_sorted_ofs[size];
			next_sorted_ofs[size] = next_ofs + 1;

			pCodesizes[ii] = static_cast<u8>( new_codesizes[next_ofs] );
		}
	}

	return true;
}


bool huffman::generate_codes(u32 num_syms, const u8 *pCodesizes, u16 *pCodes) {
	static const int MAX_CODE_SIZE = HuffmanDecoder::MAX_CODE_SIZE;

	u32 num_codes[MAX_CODE_SIZE + 1] = { 0 };

	for (u32 ii = 0; ii < num_syms; ++ii) {
		num_codes[pCodesizes[ii]]++;
	}

	u32 code = 0;

	u32 next_code[MAX_CODE_SIZE + 1];
	next_code[0] = 0;

	for (u32 ii = 1; ii <= MAX_CODE_SIZE; ++ii) {
		next_code[ii] = code;

		code = (code + num_codes[ii]) << 1;
	}

	if (code != (1 << (MAX_CODE_SIZE + 1))) {
		u32 tt = 0;

		for (u32 ii = 1; ii <= MAX_CODE_SIZE; ++ii) {
			tt += num_codes[ii];

			if (tt > 1) {
				return false;
			}
		}
	}

	for (u32 ii = 0; ii < num_syms; ++ii) {
		pCodes[ii] = static_cast<u16>( next_code[pCodesizes[ii]]++ );
	}

	return true;
}

void cat::collectFreqs(int num_syms, const std::vector<u8> &lz, u16 freqs[]) {
	const int lzSize = static_cast<int>( lz.size() );
	const int MAX_FREQ = 0xffff;

	int hist[256] = {0};
	int max_freq = 0;

	// Perform histogram, and find maximum symbol count
	for (int ii = 0; ii < lzSize; ++ii) {
		int count = ++hist[lz[ii]];

		if (max_freq < count) {
			max_freq = count;
		}
	}

	// Scale to fit in 16-bit frequency counter
	while (max_freq > MAX_FREQ) {
		// For each symbol,
		for (int ii = 0; ii < num_syms; ++ii) {
			int count = hist[ii];

			// If it exists,
			if (count) {
				count >>= 1;

				// Do not let it go to zero if it is actually used
				if (!count) {
					count = 1;
				}
			}
		}

		// Update max
		max_freq >>= 1;
	}

	// Store resulting scaled histogram
	for (int ii = 0; ii < num_syms; ++ii) {
		freqs[ii] = static_cast<u16>( hist[ii] );
	}
}

void cat::normalizeFreqs(u32 max_freq, int num_syms, u32 hist[], u16 freqs[]) {
	static const int MAX_FREQ = 0xffff;

	// Scale to fit in 16-bit frequency counter
	while (max_freq > MAX_FREQ) {
		// For each symbol,
		for (int ii = 0; ii < num_syms; ++ii) {
			int count = hist[ii];

			// If it exists,
			if (count) {
				count >>= 1;

				// Do not let it go to zero if it is actually used
				if (!count) {
					count = 1;
				}
			}
		}

		// Update max
		max_freq >>= 1;
	}

	// Store resulting scaled histogram
	for (int ii = 0; ii < num_syms; ++ii) {
		freqs[ii] = static_cast<u16>( hist[ii] );
	}
}

void cat::collectArrayFreqs(int num_syms, int data_size, u8 data[], u16 freqs[]) {
	u32 hist[cHuffmanMaxSupportedSyms] = {0};
	int max_freq = 0;

	// Perform histogram, and find maximum symbol count
	for (int ii = 0; ii < data_size; ++ii) {
		int count = ++hist[data[ii]];

		if (max_freq < count) {
			max_freq = count;
		}
	}

	normalizeFreqs(max_freq, num_syms, hist, freqs);
}

int cat::writeCompressedHuffmanTable(int num_syms, u8 codelens[], ImageWriter &writer) {
	static const int HUFF_SYMS = HuffmanDecoder::MAX_CODE_SIZE + 1;

	CAT_ENFORCE(HUFF_SYMS == 17);
	CAT_ENFORCE(num_syms >= 2);

	int bc = 0;

	// Find last non-zero symbol
	int last_non_zero = 0;
	for (int ii = 1; ii < num_syms; ++ii) {
		if (codelens[ii] > 0) {
			last_non_zero = ii;
		}
	}

	// Determine if it is worth shaving
	int shaved = num_syms - last_non_zero - 1;
	int num_syms_bits = BSR32(num_syms - 1) + 1;
	if (shaved >= num_syms_bits) {
		writer.writeBit(1);
		writer.writeBits(last_non_zero, num_syms_bits);
		bc += num_syms_bits;

		num_syms = last_non_zero + 1;
	} else {
		writer.writeBit(0);
	}
	bc++;

	// If the symbol count is low,
	if (num_syms <= 20) {
		// Encode the symbols directly
		for (int ii = 0; ii < num_syms; ++ii) {
			u8 len = codelens[ii];

			CAT_ENFORCE(len < HUFF_SYMS);

			if (len >= 15) {
				writer.writeBits(15, 4);
				writer.writeBit(len - 15);
				bc += 5;
			} else {
				writer.writeBits(len, 4);
				bc += 4;
			}
		}

		return bc;
	}

	/*
	 * Choose from one of four models of the codelens:
	 * 00 : No modifications
	 * 01 : Smoothed average with a cutoff at 32 symbols
	 * 10 : Monotonically increasing with a smoothed average of the last two, rounding up
	 * 11 : Monotonically increasing with a smoothed average of the last two
	 */

	int bitcount[4] = { 0 };
	HuffmanEncoder<HUFF_SYMS> encoders[4];

	// 00 : No modifications

	{
		// Collect statistics
		u16 freqs[HUFF_SYMS] = {0};
		for (int ii = 0; ii < num_syms; ++ii) {
			u8 len = codelens[ii];

			CAT_ENFORCE(len < HUFF_SYMS);

			freqs[len]++;
		}

		// Generate candidate codes
		if (!encoders[0].initCodelens(freqs)) {
			return 0;
		}

		// Add bitcount for table
		for (int ii = 0; ii < HUFF_SYMS; ++ii) {
			u8 len = encoders[0].simulateWrite(ii);

			CAT_ENFORCE(len < HUFF_SYMS);

			if (len >= 15) {
				bitcount[0] += 5;
			} else {
				bitcount[0] += 4;
			}
		}

		// Add bitcount for symbols
		for (int ii = 0; ii < num_syms; ++ii) {
			u8 sym = codelens[ii];

			bitcount[0] += encoders[0].simulateWrite(sym);
		}
	}

	// 01 : Smoothed average with a cutoff at 32 symbols

	{
		// Collect statistics
		u16 freqs[HUFF_SYMS] = {0};
		u32 lag0 = 1, lag1 = 1;
		for (int ii = 0; ii < num_syms; ++ii) {
			u8 len = codelens[ii];

			CAT_ENFORCE(len < HUFF_SYMS);

			u32 pred = (lag0 + lag1 + 1) >> 1;

			if (ii >= 32) {
				pred = 0;
			}

			u8 sym = (len - pred + HUFF_SYMS) % HUFF_SYMS;

			lag1 = lag0;
			lag0 = len;

			freqs[sym]++;
		}

		// Generate candidate codes
		if (!encoders[1].initCodelens(freqs)) {
			return 0;
		}

		// Add bitcount for table
		for (int ii = 0; ii < HUFF_SYMS; ++ii) {
			u8 len = encoders[1].simulateWrite(ii);

			CAT_ENFORCE(len < HUFF_SYMS);

			if (len >= 15) {
				bitcount[1] += 5;
			} else {
				bitcount[1] += 4;
			}
		}

		// Add bitcount for symbols
		lag0 = 1, lag1 = 1;
		for (int ii = 0; ii < num_syms; ++ii) {
			u8 len = codelens[ii];

			u32 pred = (lag0 + lag1 + 1) >> 1;

			if (ii >= 32) {
				pred = 0;
			}

			u8 sym = (len - pred + HUFF_SYMS) % HUFF_SYMS;

			lag1 = lag0;
			lag0 = len;

			bitcount[1] += encoders[1].simulateWrite(sym);
		}
	}

	// 10 : Monotonically increasing with a smoothed average of the last two, rounding up

	{
		// Collect statistics
		u16 freqs[HUFF_SYMS] = {0};
		u32 lag0 = 1, lag1 = 1;
		for (int ii = 0; ii < num_syms; ++ii) {
			u8 len = codelens[ii];

			CAT_ENFORCE(len < HUFF_SYMS);

			u32 pred = (lag0 + lag1 + 1) >> 1;

			u8 sym = (len - pred + HUFF_SYMS) % HUFF_SYMS;

			lag1 = lag0;
			lag0 = len;

			freqs[sym]++;
		}

		// Generate candidate codes
		if (!encoders[2].initCodelens(freqs)) {
			return 0;
		}

		// Add bitcount for table
		for (int ii = 0; ii < HUFF_SYMS; ++ii) {
			u8 len = encoders[2].simulateWrite(ii);

			CAT_ENFORCE(len < HUFF_SYMS);

			if (len >= 15) {
				bitcount[2] += 5;
			} else {
				bitcount[2] += 4;
			}
		}

		// Add bitcount for symbols
		lag0 = 1, lag1 = 1;
		for (int ii = 0; ii < num_syms; ++ii) {
			u8 len = codelens[ii];

			u32 pred = (lag0 + lag1 + 1) >> 1;

			u8 sym = (len - pred + HUFF_SYMS) % HUFF_SYMS;

			lag1 = lag0;
			lag0 = len;

			bitcount[2] += encoders[2].simulateWrite(sym);
		}
	}

	// 11 : Predict using running average

	{
		// Collect statistics
		u16 freqs[HUFF_SYMS] = {0};
		u32 lag0 = 1, lag1 = 1;
		for (int ii = 0; ii < num_syms; ++ii) {
			u8 len = codelens[ii];

			CAT_ENFORCE(len < HUFF_SYMS);

			u32 pred = (lag0 + lag1) >> 1;

			u8 sym = (len - pred + HUFF_SYMS) % HUFF_SYMS;

			lag1 = lag0;
			lag0 = len;

			freqs[sym]++;
		}

		// Generate candidate codes
		if (!encoders[3].initCodelens(freqs)) {
			return 0;
		}

		// Add bitcount for table
		for (int ii = 0; ii < HUFF_SYMS; ++ii) {
			u8 len = encoders[3].simulateWrite(ii);

			CAT_ENFORCE(len < HUFF_SYMS);

			if (len >= 15) {
				bitcount[3] += 5;
			} else {
				bitcount[3] += 4;
			}
		}

		// Add bitcount for symbols
		lag0 = 1, lag1 = 1;
		for (int ii = 0; ii < num_syms; ++ii) {
			u8 len = codelens[ii];

			u32 pred = (lag0 + lag1) >> 1;

			u8 sym = (len - pred + HUFF_SYMS) % HUFF_SYMS;

			lag1 = lag0;
			lag0 = len;

			bitcount[3] += encoders[3].simulateWrite(sym);
		}
	}

	// Determine best option
	int best = 0, bestScore = bitcount[0];
	for (int ii = 1; ii < 4; ++ii) {
		if (bestScore < bitcount[ii]) {
			bestScore = bitcount[ii];
			best = ii;
		}
	}

	// Write best table
	{
		u8 *table_codelens = encoders[best]._codelens;

		// Find last non-zero symbol
		int last_nzt = 0;
		for (int ii = 1; ii < HUFF_SYMS; ++ii) {
			if (table_codelens[ii] > 0) {
				last_nzt = ii;
			}
		}

		// Determine if it is worth shaving
		if (last_nzt <= 14) {
			writer.writeBit(1);
			writer.writeBits(last_nzt, 4);
			bc += 4;
		} else {
			writer.writeBit(0);
			last_nzt = HUFF_SYMS - 1;
		}
		bc++;

		// Encode the symbols directly
		for (int ii = 0; ii <= last_nzt; ++ii) {
			u8 len = table_codelens[ii];

			CAT_ENFORCE(len < HUFF_SYMS);

			if (len >= 15) {
				writer.writeBits(15, 4);
				writer.writeBit(len - 15);
				bc += 5;
			} else {
				writer.writeBits(len, 4);
				bc += 4;
			}
		}
	}

	// Finish initializing encoder
	encoders[best].initCodes();

	// Write best method
	writer.writeBits(best, 2);
	bc += 2;

	// Write best symbols
	switch (best) {
	default:
	case 0:
		{
			for (int ii = 0; ii < num_syms; ++ii) {
				u8 sym = codelens[ii];

				encoders[best].writeSymbol(sym, writer);
			}
		}
		break;
	case 1:
		{
			u32 lag0 = 1, lag1 = 1;
			for (int ii = 0; ii < num_syms; ++ii) {
				u8 len = codelens[ii];

				u32 pred = (lag0 + lag1 + 1) >> 1;

				if (ii >= 32) {
					pred = 0;
				}

				u8 sym = (len - pred + HUFF_SYMS) % HUFF_SYMS;

				lag1 = lag0;
				lag0 = len;

				encoders[best].writeSymbol(sym, writer);
			}
		}
		break;
	case 2:
		{
			u32 lag0 = 1, lag1 = 1;
			for (int ii = 0; ii < num_syms; ++ii) {
				u8 len = codelens[ii];

				u32 pred = (lag0 + lag1 + 1) >> 1;

				u8 sym = (len - pred + HUFF_SYMS) % HUFF_SYMS;

				lag1 = lag0;
				lag0 = len;

				encoders[best].writeSymbol(sym, writer);
			}
		}
		break;
	case 3:
		{
			u32 lag0 = 1, lag1 = 1;
			for (int ii = 0; ii < num_syms; ++ii) {
				u8 len = codelens[ii];

				u32 pred = (lag0 + lag1) >> 1;

				u8 sym = (len - pred + HUFF_SYMS) % HUFF_SYMS;

				lag1 = lag0;
				lag0 = len;

				encoders[best].writeSymbol(sym, writer);
			}
		}
		break;
	}

	return bc;
}

