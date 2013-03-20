#include "HuffmanEncoder.hpp"
using namespace cat;
using namespace huffman;


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


bool generate_huffman_codes(huffman_work_tables *state, u32 num_syms, const u16 *pFreq, u8 *pCodesizes, u32 &max_code_size, u32 &total_freq_ret) {
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
		pCodesizes[state->syms0[0].left] = 1;

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

