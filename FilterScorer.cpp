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

#include "FilterScorer.hpp"
#include "Log.hpp"
using namespace cat;


//// FilterScorer

int FilterScorer::partitionTop(int left, int right, int pivotIndex) {
	CAT_DEBUG_ENFORCE(left >= 0 && right >= 0 && left <= right);

	int pivotValue = _list[pivotIndex].score;

	// Move pivot to end
	swap(pivotIndex, right);

	int storeIndex = left;

	for (int ii = left; ii < right; ++ii) {
		if (_list[ii].score < pivotValue) {
			swap(storeIndex, ii);

			++storeIndex;
		}
	}

	// Move pivot to its final place
	swap(right, storeIndex);

	return storeIndex;
}

void FilterScorer::quickSort(int left, int right) {
	if (left < right) {
		int pivotIndex = left + (right - left) / 2;
		int pivotNewIndex = partitionTop(left, right, pivotIndex);
		quickSort(left, pivotNewIndex - 1);
		quickSort(pivotNewIndex + 1, right);
	}
}

void FilterScorer::clear() {
	if (_list) {
		delete []_list;
		_list = 0;
	}
}

void FilterScorer::init(int count) {
	clear();

	CAT_DEBUG_ENFORCE(count > 0);

	_list = new Score[count];
	_count = count;
}

void FilterScorer::reset() {
	for (int ii = 0, count = _count; ii < count; ++ii) {
		_list[ii].score = 0;
		_list[ii].index = ii;
	}
}

FilterScorer::Score *FilterScorer::getLowest() {
	Score *lowest = _list;
	int lowestScore = lowest->score;

	for (int ii = 1; ii < _count; ++ii) {
		int score = _list[ii].score;

		if (lowestScore > score) {
			lowestScore = score;
			lowest = _list + ii;
		}
	}

	return lowest;
}

FilterScorer::Score *FilterScorer::getTop(int k) {
	CAT_DEBUG_ENFORCE(k >= 1);

	if (k >= _count) {
		k = _count;
	}

	//const int listSize = k;
	int left = 0;
	int right = _count - 1;
	int pivotIndex = _count / 2;

	for (;;) {
		int pivotNewIndex = partitionTop(left, right, pivotIndex);

		int pivotDist = pivotNewIndex - left + 1;
		if (pivotDist == k) {
			// Sort the list we are returning
			//quickSort(0, listSize - 1);

			return _list;
		} else if (k < pivotDist) {
			right = pivotNewIndex - 1;
		} else {
			k -= pivotDist;
			left = pivotNewIndex + 1;
		}
	}
}

