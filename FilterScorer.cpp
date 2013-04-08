#include "FilterScorer.hpp"
using namespace cat;


//// FilterScorer

int FilterScorer::partitionTop(int left, int right, int pivotIndex) {
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

void FilterScorer::clear() {
	if (_list) {
		delete []_list;
		_list = 0;
	}
}

void FilterScorer::init(int count) {
	clear();

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
	if (k > _count) {
		k = _count;
	}

	int pivotIndex = k;
	int left = 0;
	int right = _count - 1;

	for (;;) {
		int pivotNewIndex = partitionTop(left, right, pivotIndex);

		int pivotDist = pivotNewIndex - left + 1;
		if (pivotDist == k) {
			return _list;
		} else if (k < pivotDist) {
			right = pivotNewIndex - 1;
		} else {
			k -= pivotDist;
			left = pivotNewIndex + 1;
		}
	}
}

