#ifndef FILTER_SCORER_HPP
#define FILTER_SCORER_HPP

#include "Platform.hpp"

namespace cat {


//// FilterScorer

class FilterScorer {
public:
	struct Score {
		int score;
		int index;
	};

protected:
	Score *_list;
	int _count;

	CAT_INLINE void swap(int a, int b) {
		Score temp = _list[a];
		_list[a] = _list[b];
		_list[b] = temp;
	}

	int partitionTop(int left, int right, int pivotIndex);

	void clear();

public:
	CAT_INLINE FilterScorer() {
	}
	CAT_INLINE virtual ~FilterScorer() {
		clear();
	}

	void init(int count);

	void reset();

	CAT_INLINE void add(int index, int error) {
		_list[index].score += error;
	}

	Score *getLowest();

	Score *getTop(int k);
};


} // namespace cat

#endif // FILTER_SCORER_HPP

