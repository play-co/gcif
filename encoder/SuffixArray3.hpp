#ifndef SUFFIX_ARRAY_3_HPP
#define SUFFIX_ARRAY_3_HPP

#include "../decoder/Platform.hpp"
#include <vector>

/*
 * SuffixArray3 from Charles Bloom's public domain code:
 * http://cbloomrants.blogspot.com/2011/10/10-01-11-string-match-results-part-6.html
 *
 * This is useful for finding the longest LZ match within a 1M window at each
 * byte, which is then used for optimal LZ parsing.
 * See LZMatchFinder.hpp for more information about how we use it.
 */

namespace cat {

	struct SuffixArraySearcher
	{
		// sortIndex[i] gives you the file position that is in sort order i
		// sortLookup[i] gives you the sort order of file position i
		// sortIndex[sortLookup[i]] == i
		// sortSameLen[i] gives you the pairwise match len of {i} and {i+1} in the sort order

		std::vector<int> sortIndex;
		std::vector<int> sortSameLen;
		std::vector<int> sortIndexInverse;
		int * pSortSameLen;

		int size;
		const u8 * ubuf;
		int start;
	};

	struct IntervalData
	{
		int lo,hi; // lowest and highest sortIndex[] seen in range
		int	ml; // walking matchlen from start to end of interval
	};

	struct SuffixArray3_State {
		SuffixArraySearcher SAS;
		int numLevels;
		std::vector< std::vector<IntervalData> > intervalLevels;
		int window_size;
	};

	void SuffixArray3_Init(SuffixArray3_State *state, u8 *ubuf, int size, int window_size);

	// Return top two matches
	void SuffixArray3_BestML(SuffixArray3_State *state, int pos, int &bestoff_n, int &bestoff_p, int &bestml_n, int &bestml_p);
} // namespace cat

#endif // SUFFIX_ARRAY_3_HPP

