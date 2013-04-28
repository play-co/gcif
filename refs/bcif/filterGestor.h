/*
 *  filterGestor.h
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

class filterer;
class colorFilterer;
class filterGestor{
private:
	filterer **filters;
	colorFilterer **cfilters;
public:
	filterGestor()
	{
		filters=new filterer *[12];
		cfilters=new colorFilterer *[6];
		
		filters[0] = new filter0;
		filters[1] = new filter1;
		filters[2] = new filter2;
		filters[3] = new filter3;
		filters[4] = new filter4;
		filters[5] = new filter5;
		filters[6] = new filter6;
		filters[7] = new filter7;
		filters[8] = new filter8;
		filters[9] = new filter9;
		filters[10] = new filter10;
		filters[11] = new filter11;
		
		cfilters[0] = new colorFilter0;
		cfilters[1] = new colorFilter1;
		cfilters[2] = new colorFilter2;
		cfilters[3] = new colorFilter3;
		cfilters[4] = new colorFilter4;
		cfilters[5] = new colorFilter5;
	}
	filterer* getFilter(int f) {
		return filters[f];
	}
	colorFilterer* getColFilter(int f) {
		return cfilters[f];
	}
	filterer** getFilter() {
		return filters;
	}
	colorFilterer** getColFilter() {
		return cfilters;
	}
};
