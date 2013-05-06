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

#ifndef FILTER_SCORER_HPP
#define FILTER_SCORER_HPP

#include "../decoder/Platform.hpp"
#include "../decoder/SmartArray.hpp"

namespace cat {


//// FilterScorer

class FilterScorer {
public:
	struct Score {
		int score;
		int index;
	};

protected:
	SmartArray<Score> _list;		// List of scores

	CAT_INLINE void swap(int a, int b) {
		Score temp = _list[a];
		_list[a] = _list[b];
		_list[b] = temp;
	}

	int partitionHigh(int left, int right, int pivotIndex);
	void quickSortHigh(int left, int right);

	int partitionLow(int left, int right, int pivotIndex);
	void quickSortLow(int left, int right);

public:
	void init(int count);

	void reset();

	CAT_INLINE void add(int index, int error) {
		_list[index].score += error;
	}

	Score *getLowest();

	Score *getHigh(int k, bool sorted);
	Score *getLow(int k, bool sorted);
};


} // namespace cat

#endif // FILTER_SCORER_HPP

