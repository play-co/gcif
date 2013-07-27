#include "SuffixArray3.hpp"
#include "libdivsufsort/divsufsort.h"
#include "../decoder/Enforcer.hpp"
#include "../decoder/BitMath.hpp"
using namespace std;
using namespace cat;

#define MINMATCH 2

#ifdef CAT_DEBUG

static int matchlen(const u8 *a, const u8 *b, const u8 *end) {
	int len = 0;

	while (a < end && b < end) {
		if (*a++ == *b++) {
			++len;
		} else {
			break;
		}
	}

	return len;
}

#endif

/*

from :

Linear-Time Longest-Common-Prefix Computation in Suffix Arrays and Its Applications
Toru Kasai 1, Gunho Lee2, Hiroki Arimura1;3, Setsuo Arikawa1, and Kunsoo Park2?

Algorithm GetHeight
input: A text A and its suffix array Pos
1 for i:=1 to n do
2 Rank[Pos[i]] := i
3od
4 h:=0
5 for i:=1 to n do
6 if Rank[i] > 1 then
7 k := Pos[Rank[i]-1]
8 while A[i+h] = A[j+h] do
9 h := h+1
10 od
11 Height[Rank[i]] := h
12 if h>0 then h := h-1 fi
13 fi
14 od   


The idea is obvious once you see it.  You walk in order of the original character array index,
(not the sorted order).  At index [i] you find a match of length L to its suffix-sorted neighbor.
At index [i+1] then, you must be able to match to a length of at least the same length-1 by
matching to the same neighbor !

At index 1 :

ississippi

matches :

issippi

with lenght 4.

At index 2 :

ssissippi

I know I can get a match of length 3 at least by matching "issippi"+1


*/

static void MakeSortSameLen(int * sortSameLen, // fills out sortSameLen[0 .. sortWindowLen-2]
		const int * sortIndex,const int * sortIndexInverse,const u8 * sortWindowBase,int sortWindowLen)
{
	int n = sortWindowLen;
	const u8 * A = sortWindowBase;

	int h = 0;
	for(int i=0; i< n ;i ++)
	{
		int sort_i = sortIndexInverse[i];
		if ( sort_i > 0 ) // @@ should be able to remove this
		{
			int j = sortIndex[sort_i-1];
			int h_max = sortWindowLen - (i < j ? j : i);
			while ( h < h_max && A[i+h] == A[j+h] )
			{
				h++;
			}
			sortSameLen[sort_i-1] = h;

			if ( h > 0 ) h--;
		}
	}
}

static void SuffixArraySearcher_Build(SuffixArraySearcher * SAS, const u8 * ubuf, int size )
{
	SAS->ubuf = ubuf;
	SAS->size = size;
	SAS->sortIndex.resize(size+1);
	SAS->sortIndexInverse.resize(size+1);
	SAS->sortSameLen.resize(size+3);
	
	int * pSortIndex = SAS->sortIndex.data();
	
    divsufsort(ubuf,pSortIndex,size);
    
	//---------------------------------------------------------------------------------------
	// construct sortSameLen between adjacent pairs :
	//	this is redundant, SufSort has this info already, but hard to get at it

	int * pSortSameLen = SAS->sortSameLen.data() + 1;
	SAS->pSortSameLen = pSortSameLen;
	pSortSameLen[-1] = 0;
	pSortSameLen[size] = 0;
	pSortSameLen[size+1] = 0;
	
	int * pSortLookup = SAS->sortIndexInverse.data();
		
	for(int i=0; i< size ;i ++)
	{
		pSortLookup[pSortIndex[i]] = i;
	}

	MakeSortSameLen(pSortSameLen,pSortIndex,pSortLookup,ubuf,size);	
}

template <int t_dir>
static int t_dir_SuffixArraySearcher_BestML(int pos,int sortPos,
	const int * pSortIndex,
	const int * pSortLookup,
	const int * pSortSameLen,
	const u8 * ubuf,int size)
{
	CAT_DEBUG_ENFORCE( t_dir == -1 || t_dir == 1 );

	// walk the sorted list going away from sortpos :
	//int numChecks = 0;
	int walkingMatchLen = size-pos;

	// longest match len is 
	//  sortSameLen[pos] or sortSameLen[pos-1]
	// as you walk in one direction you can update matchlen by taking RR_MIN(sortSameLen)
        
	// when walking forward I need to step back to get sameLen
	if ( t_dir == 1 ) pSortSameLen --;

	for(int vsSortPos = sortPos + t_dir;;vsSortPos += t_dir)
	{
		// as we walk away from our pos in the sort, match len is the RR_MIN of each pair matchlen
		int curSortSameLen = pSortSameLen[ vsSortPos ];
        // pSortSameLen is zero when you go off the edge of the array
        
        if ( curSortSameLen < MINMATCH )
			return 0;

		if (walkingMatchLen > curSortSameLen) {
			walkingMatchLen = curSortSameLen;
		}

		CAT_DEBUG_ENFORCE( walkingMatchLen >= MINMATCH );
		
		// check for forward offset :
		int vsPos = pSortIndex[ vsSortPos ];
		CAT_DEBUG_ENFORCE( vsPos != pos );
		if ( vsPos > pos )
			continue;
		
		#ifdef CAT_DEBUG
		{
			const u8 * pMe = ubuf+pos;
			const u8 * pVs = ubuf+vsPos;
			int ml = matchlen(pMe,pVs,ubuf+size);
			CAT_DEBUG_ENFORCE( ml == walkingMatchLen );
		}
		#endif
		
		// good!
		return walkingMatchLen;
	}		
}

#define	MIN_INTERVAL_SHIFT	5	// larger is faster except on the stress cases
#define MISSING_TOP_LEVELS	2	// pretty irrelevant

#define MIN_INTERVAL		(1<<MIN_INTERVAL_SHIFT)
#define MIN_INTERVAL_MASK	(MIN_INTERVAL-1)

void MakeFirstIntervals( vector<IntervalData> * pTo, const SuffixArraySearcher & SAS, int size )
{
	int numIntervals = (size + MIN_INTERVAL-1) / MIN_INTERVAL;
	pTo->resize(numIntervals);
	
	const int * pSortSameLen = SAS.pSortSameLen;
	const int * pSortIndex = SAS.sortIndex.data();
	
	int safesize = ((size / MIN_INTERVAL) - 1) * MIN_INTERVAL;
	//ASSERT_RELEASE( safesize > 0 );
	if (safesize < 0) {
		safesize = 0;
	}
	
	for(int i=0; i< safesize; i+= MIN_INTERVAL)
	{
		IntervalData & cur = pTo->at(i/MIN_INTERVAL);
		
		// cur is over [i,i+MIN_INTERVAL] inclusive
		
		// find lo & hi
		// walk match len across
		cur.lo = cur.hi = pSortIndex[i];
		cur.ml = INT_MAX;
		for(int j=0;j<MIN_INTERVAL;j++)
		{
			int val = pSortIndex[i+j+1];
			cur.lo = cur.lo < val ? cur.lo : val;
			cur.hi = cur.hi < val ? val : cur.hi;
			int mid = pSortSameLen[i+j];
			if (cur.ml > mid) {
				cur.ml = mid;
			}
		}
	}
	
	for(int i= safesize; i< size; i+= MIN_INTERVAL)
	{
		IntervalData & cur = pTo->at(i/MIN_INTERVAL);
		
		// cur is over [i,i+MIN_INTERVAL] inclusive
		
		cur.lo = cur.hi = pSortIndex[i];
		cur.ml = INT_MAX;
		for(int j=0;j<MIN_INTERVAL;j++)
		{
			int aa = i+j+1, bb = size-1;
			int mindex = aa < bb ? aa : bb;
			int val = pSortIndex[ mindex ];
			cur.lo = cur.lo < val ? cur.lo : val;
			cur.hi = cur.hi < val ? val : cur.hi;
			int mid = pSortSameLen[mindex];
			if (cur.ml > mid) {
				cur.ml = mid;
			}
		}
	}
}

void MakeNextIntervals( vector<IntervalData> * pTo, const vector<IntervalData> & from )
{
	int fmsize = from.size();
	int tosize = ( fmsize + 1 )/2;
	pTo->resize(tosize);
	
	for(int i=0;i<tosize;i++)
	{
		int a = i*2;
		CAT_DEBUG_ENFORCE( a < fmsize );
		int b = a+1;
		if (b > fmsize-1) {
			b = fmsize-1;
		}
	
		const IntervalData & A = from[a];
		const IntervalData & B = from[b];
		
		IntervalData & to = pTo->at(i);
		
		to.lo = A.lo < B.lo ? A.lo : B.lo;
		to.hi = A.hi < B.hi ? B.hi : A.hi;
		to.ml = A.ml < B.ml ? A.ml : B.ml;
	}
}

int LowestBitOn(u32 val)
{
	if ( val == 0 ) return 32;
    
    const u32 mask = val & -val;

    return BSR32(mask);
}


template <int t_dir>
static int t_dir_SuffixArray3_BestML(int pos,int sortPos,
	const int * pSortIndex,
	const int * pSortLookup,
	const int * pSortSameLen,
	const u8 * ubuf,int size,
	const vector<IntervalData> * pIntervalLevels,int numLevels,
	int window_size, int &match_offset)
{
	CAT_DEBUG_ENFORCE( t_dir == -1 || t_dir == 1 );

	// walk the sorted list going away from sortpos :
	//int numChecks = 0;
	int walkingMatchLen = size-pos;

	CAT_DEBUG_ENFORCE( pSortLookup[pos] == sortPos );
	CAT_DEBUG_ENFORCE( pSortIndex[sortPos] == pos );

	// when walking forward I need to step back to get sameLen
	// this makes pSortSameLen be indexed by vsSortPos and it compares to predecessor
	if ( t_dir == 1 ) pSortSameLen --;

	int curSortPos = sortPos;

	// longest match len is 
	//  sortSameLen[pos] or sortSameLen[pos-1]
	// as you walk in one direction you can update matchlen by taking RR_MIN(sortSameLen)
    // sortSameLen[i] is match len between {i and i+1}
 
    int singleStepEnd = curSortPos & (~MIN_INTERVAL_MASK);
    if ( t_dir == 1 ) singleStepEnd += MIN_INTERVAL;
       
	{
do_single_step:
		
		for(;;)
		{
			if ( curSortPos == singleStepEnd ) break;
			curSortPos += t_dir;
		
			// as we walk away from our pos in the sort, match len is the RR_MIN of each pair matchlen
			int curSortSameLen = pSortSameLen[ curSortPos ];
			// pSortSameLen is zero when you go off the edge of the array
	        
			if ( curSortSameLen < MINMATCH )
				return 0;

			if (walkingMatchLen > curSortSameLen) {
				walkingMatchLen = curSortSameLen;
			}

			CAT_DEBUG_ENFORCE( walkingMatchLen >= MINMATCH );
			
			// check for nonallowed offset :
			int vsPos = pSortIndex[ curSortPos ];
			CAT_DEBUG_ENFORCE( vsPos != pos );
			if ( vsPos > pos || vsPos < (pos - window_size) )
				continue;
			
			// passes - we're done
			
			#ifdef CAT_DEBUG
			{
				const u8 * pMe = ubuf+pos;
				const u8 * pVs = ubuf+vsPos;
				int ml = matchlen(pMe,pVs,ubuf+size);
				CAT_DEBUG_ENFORCE( ml == walkingMatchLen );
			}
			#endif
			
			// good!
			match_offset = vsPos;
			return walkingMatchLen;
		}	
	}
	
	if ( t_dir == -1 )
		if ( curSortPos == 0 ) return 0;
			
	// now I'm on an interval boundary
	{
		// start from curSortPos
		// step to the end of biggest interval I can use

step_further:

		CAT_DEBUG_ENFORCE( (curSortPos & MIN_INTERVAL_MASK) == 0 );

		int bbo = LowestBitOn( curSortPos >> MIN_INTERVAL_SHIFT );
		int level = numLevels-1;
		if (level > bbo) {
			level = bbo;
		}
		
		CAT_DEBUG_ENFORCE( level >= 0 );
		
		{		
			int levelShift = level + MIN_INTERVAL_SHIFT;
			int levelStepSize = 1<<levelShift;
			
			CAT_DEBUG_ENFORCE( (curSortPos & (levelStepSize-1)) == 0 );
			// step from curSortPos to curSortPos+levelStepSize
			
			int levelIndex = curSortPos >> levelShift;
			
			CAT_DEBUG_ENFORCE( levelIndex >= 0 && levelIndex < pIntervalLevels[level].size() );
			
			if ( t_dir == -1 )
			{
				CAT_DEBUG_ENFORCE( curSortPos != 0 );
				CAT_DEBUG_ENFORCE( levelIndex > 0 );
				levelIndex--;
				//if ( levelIndex < 0 ) break;
			}
			
			const IntervalData & cur = pIntervalLevels[level][levelIndex];
			// is target in this interval ?
			
			if ( cur.hi < (pos - window_size) ||
				 cur.lo >= pos )
			{
				// no good, step past it ;
				if (walkingMatchLen > cur.ml) {
					walkingMatchLen = cur.ml;
				}
				if ( walkingMatchLen < MINMATCH ) return 0;
				
				if ( t_dir == 1 )
				{
					curSortPos += levelStepSize;
					if ( curSortPos >= size ) return 0;
				}
				else
				{
					curSortPos -= levelStepSize;
					if ( curSortPos <= 0 ) return 0;
				}
				
				goto step_further; // repeat stepping intervals
			} 
			
			// it must be in the current interval, step down to lower levels
			
			// NOTEZ : almost identical to the above loop
			//	but slightly faster to break it out
								
			while( level > 0 )
			{
				CAT_DEBUG_ENFORCE( LowestBitOn( curSortPos >> MIN_INTERVAL_SHIFT ) >= level );
		
				level--;	
				levelShift--;
				levelStepSize >>= 1;
				
				CAT_DEBUG_ENFORCE( levelShift >= MIN_INTERVAL_SHIFT );
				CAT_DEBUG_ENFORCE( (curSortPos & (levelStepSize-1)) == 0 );
				// step from curSortPos to curSortPos+levelStepSize
				
				int levelIndex = curSortPos >> levelShift;
				
				CAT_DEBUG_ENFORCE( levelIndex >= 0 && levelIndex < pIntervalLevels[level].size() );
				
				if ( t_dir == -1 )
				{
					CAT_DEBUG_ENFORCE( curSortPos != 0 );
					CAT_DEBUG_ENFORCE( levelIndex > 0 );
					levelIndex--;
					//if ( levelIndex < 0 ) break;
				}
				
				const IntervalData & cur = pIntervalLevels[level][levelIndex];
				// is target in this interval ?
				
				if ( cur.hi < (pos - window_size) ||
					 cur.lo >= pos )
				{
					// no good, step past it ;
					if (walkingMatchLen > cur.ml) {
						walkingMatchLen = cur.ml;
					}
					if ( walkingMatchLen < MINMATCH ) return 0;
					
					if ( t_dir == 1 )
					{
						curSortPos += levelStepSize;
						if ( curSortPos >= size ) return 0;
					}
					else
					{
						curSortPos -= levelStepSize;
						if ( curSortPos <= 0 ) return 0;
					}					
				} 
				else
				{
					// descend right here
					// ! this is really the only difference from the above loop					
				}
			}
		}
	
		// it must be in [curSortPos,curSortPos+MIN_INTERVAL]
		if (t_dir == 1) {
			singleStepEnd = curSortPos + MIN_INTERVAL;
		} else {
			singleStepEnd = curSortPos - MIN_INTERVAL;
		}
		goto do_single_step;
	}
}

static void SuffixArray3_BestML(const SuffixArraySearcher * SAS,int pos,
	const vector<IntervalData> * pIntervalLevels,int numLevels,
	int window_size, int &bestoff_n, int &bestoff_p, int &bestml_n, int &bestml_p)
{	
	const int * pSortLookup = SAS->sortIndexInverse.data();
	int sortPos = pSortLookup[pos];
	CAT_DEBUG_ENFORCE( SAS->sortIndex[sortPos] == pos );
    
	const int * pSortIndex = SAS->sortIndex.data();
	const int * pSortSameLen = SAS->pSortSameLen;
	const u8 * ubuf = SAS->ubuf;
	int size = SAS->size;

    bestml_n = t_dir_SuffixArray3_BestML<-1>(pos,sortPos,pSortIndex,pSortLookup,pSortSameLen,ubuf,size,pIntervalLevels,numLevels,window_size, bestoff_n);
    bestml_p = t_dir_SuffixArray3_BestML< 1>(pos,sortPos,pSortIndex,pSortLookup,pSortSameLen,ubuf,size,pIntervalLevels,numLevels,window_size, bestoff_p);
 
    #ifdef CAT_DEBUG
	int bestml;
	if (bestml_p < bestml_n) {
		bestml = bestml_n;
	} else {
		bestml = bestml_p;
	}

    if ( window_size > size )
    {
		int check_bestml_n = t_dir_SuffixArraySearcher_BestML<-1>(pos,sortPos,pSortIndex,pSortLookup,pSortSameLen,ubuf,size);
		int check_bestml_p = t_dir_SuffixArraySearcher_BestML< 1>(pos,sortPos,pSortIndex,pSortLookup,pSortSameLen,ubuf,size);

		CAT_DEBUG_ENFORCE( bestml_n == check_bestml_n );
		CAT_DEBUG_ENFORCE( bestml_p == check_bestml_p );

		int check_bestml = check_bestml_p < check_bestml_n ? check_bestml_n : check_bestml_p;

		CAT_DEBUG_ENFORCE( bestml == check_bestml );
    }
    #endif
}

void cat::SuffixArray3_Init(SuffixArray3_State *state, u8 *ubuf, int size, int window_size) {
	SuffixArraySearcher_Build(&state->SAS, ubuf, size);

	int numLevels = 1;
	while ( (MIN_INTERVAL<<(numLevels+MISSING_TOP_LEVELS)) < size )
		numLevels++;

	state->intervalLevels.resize(numLevels);
	MakeFirstIntervals(&state->intervalLevels[0], state->SAS, size );
	
	for(int level=1;level<numLevels;level++)
	{
		MakeNextIntervals( &state->intervalLevels[level] , state->intervalLevels[level-1] );
	}

	state->numLevels = numLevels;
	state->window_size = window_size;
}

void cat::SuffixArray3_BestML(SuffixArray3_State *state, int pos, int &bestoff_n, int &bestoff_p, int &bestml_n, int &bestml_p) {
	::SuffixArray3_BestML(&state->SAS, pos, state->intervalLevels.data(), state->numLevels, state->window_size, bestoff_n, bestoff_p, bestml_n, bestml_p);
}

