/*
 *  BmpImage.cpp
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

#include <stdlib.h> 
#include <string.h> 
#include <iostream>
#include <fstream>
#include <string>
#include <math.h>
using namespace std;
#include "readHeader.h"
#include "bitMatrix.h"
#include "byteMatrix.h"
#include "costEvaluator.h"
#include "BmpImage.h"

void BmpImage::filterDeterminate() {
	if (cost == NULL) {
		costEvaluator evCost;
		cost = evCost.getCosts();
	}
	int val = 0;
	int best = 0;
	int bestInd = 0;
	int zoneNum = lengthlineFilter = (int)(((info.biWidth - 1) / filterZoneDim + 1) * ((info.biHeight - 1) / filterZoneDim + 1));
	lineFilter = (char *) calloc(lengthlineFilter,sizeof(char));
	int minx = 0;
	int miny = 0;
	int maxx = filterZoneDim - 1;
	int maxy = filterZoneDim - 1;
	int maxFilterUsed = 0;
	char left = 0;
	char low = 0;
	char lowLeft = 0;
	char upLeft = 0;
	char rightDown = 0;
	costEvaluator **ce = new costEvaluator *[3];
	for (int i = 0; i < 3; i ++) {
		ce[i] = new costEvaluator(filterNum);
		ce[i]->getCosts();
	}
	int *filterErrs;
	filterErrs = (int *) calloc(filterNum,sizeof(int));
	int *vectorError = (int *) calloc(filterNum,sizeof(int));
	for (int i5 = 0; i5 < zoneNum; i5 ++) {
		memset(vectorError, 0, filterNum*sizeof(int));
		for (int i3 = miny; i3 <= maxy; i3 ++) {
			int newErr = 0;
			for (int i2 = minx; i2 <= maxx; i2 += filterStep) {
				for (int i4 = 0; i4 < 3; i4++) {
					image->setPoint(i2, i3, i4);
					val = image->getCurVal();
					if (i2 > 0) { left = image->getLeftVal(); } else { left = 0; }
					if (i3 > 0) { low = image->getLowVal(); } else { low = 0; }
					if (i2 > 0 && i3 > 0) { lowLeft = image->getLowLeftVal(); } else { lowLeft = 0; }
					if (i3 > 0 && i2 < info.biWidth - 1) { rightDown = (int)image->getRightDownVal(); } else { upLeft = 0; }
					calcFilErrs(val, (int)left, (int)low, (int)lowLeft, (int)rightDown, i2, i3, filterErrs);
					for (int i = 0; i < filterNum; i++) {
						newErr = filterErrs[i];
						if(newErr < 0){newErr = -newErr;}
						if(newErr > 128) newErr = 256 - newErr;
						int sumVal = cost[newErr];
						vectorError[i] += sumVal;
					}
				}
			}
		}
		bestInd = 0;
		if (newCM) {
			best = ce[0]->getFilCost(0) + ce[1]->getFilCost(0) + ce[2]->getFilCost(0);
		} else {
			best = vectorError[0];
		}
		for (int i = 1; i < filterNum; i ++) {
			int filCost = 0;
			if (newCM) {
				filCost = ce[0]->getFilCost(i) + ce[1]->getFilCost(i) + ce[2]->getFilCost(i);
			} else {
				filCost = vectorError[i];
			}
			if (filCost < best) { best = filCost; bestInd = i; }
		}
		lineFilter[i5] = bestInd;
		if (newCM) { ce[0]->signalSel(bestInd); ce[1]->signalSel(bestInd); ce[2]->signalSel(bestInd); }
		maxFilterUsed = maxFilterUsed >= bestInd + 1 ? maxFilterUsed: bestInd + 1;
		minx += filterZoneDim;
		if (minx > info.biWidth - 1) {
			minx = 0;
			miny += filterZoneDim;
		}
		maxx = (int)min(minx + filterZoneDim - 1, info.biWidth - 1);
		maxy = (int)min(miny + filterZoneDim - 1, info.biHeight - 1);
	}
	if (maxFilterUsed < filterNum) { setFilterNum(maxFilterUsed); }
}

void BmpImage::colorFilterDeterminate() {
	//int *e = (int *) calloc(colorFilterNum,sizeof(int));
	int val = 0;
	int best = 0;
	int bestInd = 0;
	int zoneNum = (int)(((info.biWidth - 1) / colorFilterZoneDim + 1) * ((info.biHeight - 1) / colorFilterZoneDim + 1));
	colorLineFilter = (char *) calloc(zoneNum,sizeof(char));
	if (colorLineFilter == NULL) cout << "Error on calloc" << endl;
	int minx = 0;
	int miny = 0;
	int maxx = colorFilterZoneDim - 1;
	int maxy = colorFilterZoneDim - 1;
	int maxColorFilterUsed = 0;
	costEvaluator **ce = new costEvaluator *[3];
	for (int i = 0; i < 3; i ++) {
		ce[i] = new costEvaluator(colorFilterNum);
		ce[i]->getCosts();
	}
	int  **filterErrs = (int **) calloc(3,sizeof(int*));
	filterErrs[0] = (int *) calloc(6,sizeof(int));
	filterErrs[1] = (int *) calloc(6,sizeof(int));
	filterErrs[2] = (int *) calloc(6,sizeof(int));	
	
	for (int i5 = 0; i5 < zoneNum; i5 ++) {
		for (int i3 = miny; i3 <= maxy; i3 ++) {
			int newErr = 0;
			for (int i2 = minx; i2 <= maxx; i2 += filterStep) {
				image->setPoint(i2, i3, 0);
				calcFilColErrs((int)image->getCurVal(), (int)image->getCurVal(1), (int)image->getCurVal(2), filterErrs);
				for (int i4 = 0; i4 < 3; i4++) {
					image->setPoint(i2, i3, i4);
					val = (int)image->getCurVal();					
					for (int i = 0; i < colorFilterNum; i++) {
						newErr = absValue[filterErrs[i4][i] + 256]; 
						ce[i4]->putVal(i, newErr);
					}
					image->nextVal();
				}
			}
		}
		bestInd = 0;
		best = ce[0]->getFilCost(0) + ce[1]->getFilCost(0) + ce[2]->getFilCost(0);
		for (int i = 0; i < colorFilterNum; i ++) {
			int filCost = ce[0]->getFilCost(i) + ce[1]->getFilCost(i) + ce[2]->getFilCost(i);			
			if (filCost < best) { best = filCost; bestInd = i; }			
		}
		colorLineFilter[i5] = (char)bestInd;
		ce[0]->signalSel(bestInd);
		ce[1]->signalSel(bestInd);
		ce[2]->signalSel(bestInd);
		if (maxColorFilterUsed < bestInd + 1) { maxColorFilterUsed = bestInd + 1; }
		minx += colorFilterZoneDim;
		if (minx > info.biWidth - 1) {
			minx = 0;
			miny += colorFilterZoneDim;
		}
		maxx = (int)min(minx + colorFilterZoneDim - 1, info.biWidth - 1);
		maxy = (int)min(miny + colorFilterZoneDim - 1, info.biHeight - 1);
	}
	if (maxColorFilterUsed < colorFilterNum) { setColorFilterNum(maxColorFilterUsed); }
}

void BmpImage::applyColFilter() {
	int forlim = (int)info.biWidth - 1;
	int forlim2 = (int)info.biHeight - 1;
	image->lastVal();
	for (int i2 = forlim2; i2 > -1; i2--) {
		for (int i = forlim; i > -1; i --) {
			for (int i3 = 2; i3 > -1; i3--) {
				image->sumCurVal( - colorFilter(i, i2, i3));
				image->precVal();
			}
		}
	}
}

void BmpImage::applyFilter() {
	image->lastVal();
	for (int i2 = info.biHeight - 1; i2 > -1; i2--) {
		for (int i = info.biWidth - 1; i > -1; i --) {
			for (int i3 = 2; i3 > -1; i3--) {
				image->sumCurVal(- filter(i, i2, i3));
				
				image->precVal();
			}
		}
	}
}

void BmpImage::applyFilter(bool boolean) {
	image->lastVal();
	for (int i2 = info.biHeight - 1; i2 > -1; i2--) {
		for (int i = info.biWidth - 1; i > -1; i --) {
			for (int i3 = 2; i3 > -1; i3--) {
				int x = - filter(i, i2, i3);
				image->sumCurVal(x);
				image->precVal();
			}
		}
	}
}

// Not used in BCIF decompression, filter removal is in bcif.cpp

void BmpImage::removeColFilter() {
	cout << "Removing color filters ..."<<endl;
	
	image->firstVal();
	int curcFil = 0;
	int curcInd = 0;
	for (int i2 = 0; i2 < info.biHeight; i2 ++) {
		curcFil = colorFilterOfZone(0, i2);
		curcInd = 0;
		for (int i = 0; i < info.biWidth; i ++) {
			if (curcInd == colorFilterZoneDim) {
				curcInd = 0;
				curcFil = colorFilterOfZone(i, i2);
			}
			curcInd ++;
			
			for (int i3 = 0; i3 < 3; i3++) {
				image->sumCurVal(colorFilter(i, i2, i3, curcFil));
				image->nextVal();
			}
			
		}
	}
}

// Not used in BCIF decompression, filter removal is in bcif.cpp

void BmpImage::removeBothFilters() {
	cout << "Removing standard and color filters ..."<<endl;
	image->firstVal();
	int curcFil = 0;
	int curcInd = 0;
	int curFil = 0;
	int curInd = 0;
	for (int i2 = 0; i2 < info.biHeight; i2 ++) {
		curcFil = colorFilterOfZone(0, i2);
		curcInd = 0;
		curFil = filterOfZone(0, i2);
		curInd = 0;
		for (int i = 0; i < info.biWidth; i ++) {
			if (curcInd == colorFilterZoneDim) {
				curcInd = 0;
				curcFil = colorFilterOfZone(i, i2);
			}
			if (curInd == filterZoneDim) {
				curInd = 0;
				curFil = filterOfZone(i, i2);
			}
			curcInd ++;
			curInd ++;
			for (int i3 = 0; i3 < 3; i3++) {
				image->sumCurVal(colorFilter(i, i2, i3, curcFil));
				image->nextVal();
			}
			image->precVal(3);
			for (int i3 = 0; i3 < 3; i3++) {
				image->sumCurVal(filter(i, i2, i3, curFil));
				image->nextVal();
			}
		}
	}
}

// Not used in BCIF decompression, filter removal is in bcif.cpp

void BmpImage::removeFilter() {
	cout << "Removing filters ..."<<endl;
	image->firstVal();
	int curFil = 0;
	int curInd = 0;
	for (int i2 = 0; i2 < info.biHeight; i2 ++) {
		curFil = filterOfZone(0, i2);
		curInd = 0;
		for (int i = 0; i < info.biWidth; i ++) {
			if (curInd == filterZoneDim) {
				curInd = 0;
				curFil = filterOfZone(i, i2);
			}
			curInd ++;
			for (int i3 = 0; i3 < 3; i3 ++) {
				image->sumCurVal(filter(i, i2, i3, curFil));
				image->nextVal();
			}
		}
	}
}

// Not used in BCIF decompression, filter removal is in bcif.cpp

void BmpImage::removeAllFilters() {
	cout << "Removing all filters ..."<<endl;
	image->firstVal();
	int curFil = 0;
	int curInd = 0;
	int curcFil = 0;
	int curcInd = 0;
	for (int i = 0; i < info.biWidth; i ++) {
		curFil = filterOfZone(i, 0);
		curInd = 0;
		curcFil = colorFilterOfZone(i, 0);
		curcInd = 0;
		for (int i2 = 0; i2 < info.biHeight; i2 ++) {
			if (curInd == filterZoneDim) {
				curInd = 0;
				curFil = filterOfZone(i, i2);
			}
			curInd ++;
			if (curcInd == colorFilterZoneDim) {
				curcInd = 0;
				curcFil = colorFilterOfZone(i, i2);
			}
			curcInd ++;
			for (int i3 = 0; i3 < 3; i3 ++) {
				int cur = image->getCurVal();
				image->setCurVal((char)invmap[cur >= 0 ? cur : cur + 256]);
				image->nextVal();
			}
			if (curcInd != 0 && curcFil != 1) {
				image->precVal(2);
				for (int i3 = 1; i3 < 3; i3++) {
					image->sumCurVal(colorFilter(i, i2, i3, curcFil));
					image->nextVal();
				}
			}
			image->precVal(3);
			for (int i3 = 0; i3 < 3; i3 ++) {
				image->sumCurVal(filter(i, i2, i3, curFil));
				image->nextVal();
			}
		}
	}
}

void BmpImage::reportDist(int bitNum) {
	int eNum = 1 << bitNum;
	int * d = new int[eNum];
	int val;
	for (int i = 0; i < info.biWidth; i ++) {
		for (int i2 = 0; i2 < info.biHeight; i2 ++) {
			for (int i3 = 0; i3 < 3; i3 ++) {
				val = image->getVal(i, i2, i3);
				if (val < 0) { val += 256; }
				val = val % eNum;
				d[val] ++;
			}
		}
	}
	long total = info.biWidth * info.biHeight * 3;
	for (int i = 0; i < eNum; i ++) {
		cout << i << " values are " << d[i] << " (" << (float)d[i] * 100 / total << "%)" <<endl;
	}
	long var = 0;
	for (int i = 0; i < eNum; i++) {
		var += abs256((total / eNum) - d[i]);
	}
	cout << "Medium difference from average is " << var / eNum << " (" << ((float)var * 100 / total) << "%)" <<endl;
}

void BmpImage::bitprint(char b) {
	if (b >= 0 && b < 10) { cout <<" "; }
	if (b >= 0 && b < 100) {cout <<" "; }
	for (int i = 0; i < 8; i++) {
		b = (char)(b >> 1);
	}
}


