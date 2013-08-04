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
 *
 * There are a few small challenges to adopt this approach:
 *
 * (1) For RGBA it sometimes finds unaligned matches (since it's byte-wise)
 * that are not useful for compression.  We currently just discard these and
 * fall back to the bounded hash chain approach.  There is no way to change SA3
 * to return word matches without a lot of work.
 *
 * (2) We don't really want the longest match, since the savings for each pixel
 * are not equal.  Ssome pixels take more bits to represent than others.  The
 * cost to represent distances varies by up to 16 bits.  So we modified the SA3
 * algorithm to return two matches and we consider both as well as local
 * matches found using a simple hash chain approach, which returns nearby
 * matches first.  Nearby matches often have a short distance encoding and can
 * be preferable over the long-distance longest SA3 match.
 *
 * Combining both hash chains and suffix-array based matchers turned out to
 * yield an algorithm that is both fast and compresses well.
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

