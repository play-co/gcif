/*
 *  HTreeWriterGestor.cpp
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
#include <typeinfo>
#include <string>
#include <math.h>
#include <vector>
using namespace std;
#include "binaryWrite.h"
#include "HTree.h"

#include "HTreeWriterGestor.h"

void HTreeWriterGestor::endGathering() {
	if (curZeros > 0) {
		zs.push_back(curZeros);
		if (curZeros == 0) { f[0] ++; } else {
			if (curZeros >= zeroSeqNum) {
				f[simbolNum + zeroSeqNum - 1] ++;
			} else {
				f[simbolNum + curZeros - 1] ++;
			}
		}
		curZeros = 0;
	}
	int zeroPerc = 0;
	
	HTree htree;
	
	if (tot > 0) { zeroPerc = zeros * 100 / tot; }
	//if (zeros < tot && (zeroPerc > zeroPercLim)) {
		keyNum = simbolNum + zeroSeqNum;
		aux = htree.buildHTree(getfazSize(),faz);
		aux->cut();
		int *codeBits2 = aux->getCodeBits();
		aux = htree.buildHTreeFromBits(codeBits2,getfazSize());
/*	} else {
		
		keyNum = simbolNum;
		nf[0] = zeros;
		for (int i = 1; i < simbolNum; i ++) {
			nf[i] = f[i] + faz[i];
		}
		f = nf;
		sizef = simbolNum;
	}
*/	
		
	ht = htree.buildHTree(sizef,f);
	ht->cut();
	int *codeBits = ht->getCodeBits();	
	ht = htree.buildHTreeFromBits(codeBits,sizef);
	curht = ht;
}

double HTreeWriterGestor::HuffmanEntropy(int *simbolsFreq, int length) {
	HTree chtree;
	ht = chtree.buildHTree(simbolsFreq,length);
	int *cod = ht->getCodeBits();
	long totBits = 0;
	long totVals = 0;
	for (int i = 0; i < length; i++) {
		totVals += simbolsFreq[i];
		totBits += simbolsFreq[i] * cod[i];
	}
	if (totVals == 0) { return 0; } else {
		return (double) totBits / totVals;
	}
}

void HTreeWriterGestor::putVal(int v) {
	tot ++;
	if (v == 0) {
		zeros ++;
		curZeros ++;
	} else {
		if (curZeros > 0) {
			zs.push_back(curZeros);
			if (curZeros >= zeroSeqNum) {
				f[simbolNum + zeroSeqNum - 1] ++;
			} else {
				f[simbolNum + curZeros - 1] ++;
			}
			curZeros = 0;
			faz[v] ++;
		} else {
			f[v]++;
		}
	}
}

