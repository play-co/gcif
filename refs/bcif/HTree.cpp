/*
 *  HTree.cpp
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
using namespace std;
#include "binaryWrite.h"
#include "newBitReader.h"
#include "HTree.h"

int* HTree::getLookupLeft() {
	return lleft;
}

int* HTree::getLookupRight() {
	return lright;
}

int* HTree::getLookupValues() {
	return lkey;
}
	
HTree* HTree::readHTreeFromBitsBR(newBitReader *br, int valNum )
{
	int *b;
	b = (int *) calloc(valNum,sizeof(int));
	HTree ht;
	ht.readBits(br, valNum, b);
	return HTree::buildHTreeFromBits(b,valNum);
}

HTree* HTree::buildHTreeFromBits (int *codeBits,int codeBitsLength,newBitReader *br) {
    int n = codeBitsLength;
    HTree **hts;
	 hts = new HTree *[n];

	 for (int i=0; i<n; i++) {
	 hts[i]=NULL;
	 }
    int *b;
	b=(int *) calloc (n,sizeof(int));
	int treeNum = 0;
    int maxBits = 0;
    bool exitf = false;
	for (int i = 0; i < n && ! exitf; i++) {
		if (codeBits[i] > 0) {
			hts[i]= new HTree(i, 1 << codeBits[i]);
			treeNum ++;
			if (codeBits[i] > maxBits) { maxBits = codeBits[i]; }
		} else if (codeBits[i] == 0) {
			exitf = true;
			hts[0]= new HTree(i, 1);
		}
		b[i] = codeBits[i];
    }
	
    if (exitf) {
		hts[0]->codeBits = codeBits;
		hts[0]->setkeyN(n);
		hts[0]->createdByBits = true;
		return hts[0];
    } else {
		for (int i = maxBits; i >= 0 && treeNum > 0; i--) {
			HTree *first;
			first=NULL;
			int firstInd = 0;
			for (int i2 = 0; i2 < n; i2 ++) {
				if ((hts[i2] != NULL) & (b[i2] == i)) {
					if (first == NULL) {
						first = hts[i2];
						firstInd = i2;
					} else {
						hts[firstInd] = new HTree(first, hts[i2]);
						hts[i2] = NULL;
						b[firstInd] --;
						b[i2] = -1;
						first = NULL;
					}
				}
			}
		}
        
    }
    int firstTree = 0;
    if (treeNum == 0) {
		hts[0] = new HTree(0,1);
    }
    while (hts[firstTree] == NULL) { firstTree++; }
	hts[firstTree]->setkeyN(n);
    hts[firstTree]->createdByBits = true;
	hts[firstTree]->setCodeBits(codeBits);
    return hts[firstTree];
}

void HTree::createLookup(newBitReader *br) {
	lleft  = (int*) calloc (1024, sizeof(int));
	lright = (int*) calloc (1024, sizeof(int));
	lkey   = (int*) calloc (1024, sizeof(int));
	int *x = new int[2];
	recCreateLookup(lkey,lleft,lright, 0,x,br);
}

void HTree::createLookup() {
	lleft  = (int*) calloc (1024, sizeof(int));
	lright = (int*) calloc (1024, sizeof(int));
	lkey   = (int*) calloc (1024, sizeof(int));
	int *x = new int[2];
	recCreateLookup(lkey,lleft,lright, 0,x);
}

void HTree::recCreateLookup(int *k, int *l, int *r, int free, int *res, newBitReader *br) {
	
  	int myKey = free;
  	free ++;
  	if (left == NULL) {  		
  		k[myKey] = val;
  	} else {
		int newPtr[2];
		newPtr[0]=0;
		newPtr[1]=0;
  		k[myKey] = -1;
  		left->recCreateLookup(k, l, r, free,newPtr,br);
  		l[myKey] = newPtr[0];
  		free = newPtr[1];
  		right->recCreateLookup(k, l, r, free,newPtr,br);
  		r[myKey] = newPtr[0];
  		free = newPtr[1];
  	}
	res[0]=myKey;
	res[1]=free;
}


HTree* HTree::readHTreeFromBits(newBitReader *br, int valNum ) {
	int *b;
	b = (int *) calloc(valNum,sizeof(int));
	HTree ht;
	ht.readBits(br, valNum, b);
	return HTree::buildHTreeFromBits(b,valNum);
}


HTree* HTree::buildHTreeFromBits (int *codeBits,int codeBitsLength) {
    int n = codeBitsLength;
    HTree **hts;
	hts = (HTree **) calloc (n,sizeof(HTree *));
    int *b;
	b=(int *) calloc (n,sizeof(int));
	int treeNum = 0;
    int maxBits = 0;
    bool exitf = false;
    
	 for (int i = 0; i < n && ! exitf; i++) {
		if (codeBits[i] > 0) {
			hts[i]= new HTree(i, 1 << codeBits[i]);
			treeNum ++;
			if (codeBits[i] > maxBits) { maxBits = codeBits[i]; }
		} else if (codeBits[i] == 0) {
			exitf = true;
			hts[0]= new HTree(i, 1);
		}
		b[i] = codeBits[i];
    }
	
    if (exitf) {
		hts[0]->codeBits = codeBits;
		hts[0]->setkeyN(n);
		hts[0]->createdByBits = true;
		return hts[0];
    } else {
		for (int i = maxBits; i >= 0 && treeNum > 0; i--) {
			HTree *first;
			first=NULL;
			int firstInd = 0;
			for (int i2 = 0; i2 < n; i2 ++) {
				if ((hts[i2] != NULL) & (b[i2] == i)) {
					if (first == NULL) {
						first = hts[i2];
						firstInd = i2;
					} else {
						hts[firstInd] = new HTree(first, hts[i2]);
						hts[i2] = NULL;
						b[firstInd] --;
						b[i2] = -1;
						first = NULL;
					}
				}
			}
		}
        
    }
    int firstTree = 0;
    if (treeNum == 0) {
		hts[0] = new HTree(0,1);
    }
    while (hts[firstTree] == NULL) { firstTree++; }
	hts[firstTree]->setkeyN(n);
    hts[firstTree]->createdByBits = true;
	
	hts[firstTree]->setCodeBits(codeBits);
	
    return hts[firstTree];
}

void HTree::recCreateLookup(int *k, int *l, int *r, int free, int *res) {

  	int myKey = free;
  	free ++;
  	if (left == NULL) {  		
  		k[myKey] = val;
  	} else {
		int newPtr[2];
		newPtr[0]=0;
		newPtr[1]=0;
  		k[myKey] = -1;
  		left->recCreateLookup(k, l, r, free,newPtr);
  		l[myKey] = newPtr[0];
  		free = newPtr[1];
  		right->recCreateLookup(k, l, r, free,newPtr);
  		r[myKey] = newPtr[0];
  		free = newPtr[1];
  	}
	res[0]=myKey;
	res[1]=free;
}

int HTree::readEfVal(newBitReader *bm) {
	int cur = 0;
	while (lkey[cur] == -1) {
  		if (bm->readBit() == 0 ) {
  			cur = lleft[cur];
  		} else {
  			cur = lright[cur];
  		}
  	}
	return lkey[cur];
}

HTree* HTree::buildHTree(int length,int *f) {

	return buildHTree(length,f, 0);
}

HTree* HTree::buildHTree (int length,int *f, int minFreq) {
    int n = length;
    int *v = (int *) calloc(n,sizeof(int));
    for (int i = 1; i < n; i++) {
		v[i] = i;
    }
    HTree **res = new HTree *[n];
    for (int i = 0; i < n; i++) {
		if (f[i] < minFreq) { f[i] = minFreq; }
		res[i] = new HTree(v[i], f[i]);
    }
	
	for (int i = 0; i < n; i++) {
		//cout << i<< "\t" <<f[i]<<endl;
	}
	
	
    int min1 = 0;
    int min2 = 0;
    int minInd1 = 0;
    int minInd2 = 0;
    int mergeInd = 0;
    int lastInd = 0;
    for (int i = n - 1; i > 0; i--) {		
		if (res[0]->getFreq() < res[1]->getFreq()) {
			min1 = res[0]->getFreq();
			min2 = res[1]->getFreq();
			minInd1 = 0;
			minInd2 = 1;
		} else {
			min1 = res[1]->getFreq();
			min2 = res[0]->getFreq();
			minInd1 = 1;
			minInd2 = 0;
		}
		for (int i2 = 2; i2 < i + 1; i2 ++) {
			if (res[i2]->getFreq() < min1) {
				min2 = min1;
				minInd2 = minInd1;
				min1 = res[i2]->getFreq();
				minInd1 = i2;
			} else if (res[i2]->getFreq() < min2) {
				min2 = res[i2]->getFreq();
				minInd2 = i2;
			}
		}
		mergeInd = min(minInd1, minInd2);
		lastInd = max(minInd1, minInd2);
		res[mergeInd] = new HTree(res[minInd1], res[minInd2]);
		res[lastInd] = res[i];
    }
    res[0]->setkeyN(n);
    return res[0];
}

HTree* HTree::buildHTree (int *f) {
	return HTree::buildHTree(f, 0);
}

HTree* HTree::buildHTree (int *f, int minFreq/*, int flength = 12*/) {
	int n = 12;
    int *v = (int *) calloc(n,sizeof(int));
    for (int i = 1; i < n; i++) {
		v[i] = i;
    }
    HTree **res = new HTree *[n];
    for (int i = 0; i < n; i++) {
		if (f[i] < minFreq) { f[i] = minFreq; }
		res[i] = new HTree(v[i], f[i]);
    }
    int min1 = 0;
    int min2 = 0;
    int minInd1 = 0;
    int minInd2 = 0;
    int mergeInd = 0;
    int lastInd = 0;
    for (int i = n - 1; i > 0; i--) {
		if (res[0]->getFreq() < res[1]->getFreq()) {
			min1 = res[0]->getFreq();
			min2 = res[1]->getFreq();
			minInd1 = 0;
			minInd2 = 1;
		} else {
			min1 = res[1]->getFreq();
			min2 = res[0]->getFreq();
			minInd1 = 1;
			minInd2 = 0;
		}
		for (int i2 = 2; i2 < i + 1; i2 ++) {
			if (res[i2]->getFreq() < min1) {
				min2 = min1;
				minInd2 = minInd1;
				min1 = res[i2]->getFreq();
				minInd1 = i2;
			} else if (res[i2]->getFreq() < min2) {
				min2 = res[i2]->getFreq();
				minInd2 = i2;
			}
		}
		mergeInd = min(minInd1, minInd2);
		lastInd = max(minInd1, minInd2);
		res[mergeInd] = new HTree(res[minInd1], res[minInd2]);
		res[lastInd] = res[i];
    }
    res[0]->setkeyN(n);
    return res[0];
}

void HTree::readBits(newBitReader *br,int valNum, int *res)
{
	int prec = 0;
	bool exitf = false;
	int cur = 0;
	int max = br->readVbit(3);
	for (int i = 0; i < valNum; i++) {
		if (! exitf) {
			cur = br->readVbit();
			if (cur > 0 && br->readBit() == 1) { cur = - cur; }
			cur = prec + cur;
			if (cur > max) { res[i] = -1; } else { res[i] = cur; }
			prec =  cur;
		} else {
			res[i] = -1;
		}
		if (cur == 0) {
			exitf = true;
		}
	}
}

void HTree::writeBits(binaryWrite *bw) {
	int prec = 0;
	int min = 0;
	bool exitf = false;
	int max = -1;
	for (int i = 0; i < keyN && ! exitf; i++) {
		if (codeBits[i] > max) { max = codeBits[i]; }
	}
	bw->writeVbit(max, 3);
	for (int i = 0; i < keyN && ! exitf; i++) {         // KeyN is at least equal to 1 !
		int cur = codeBits[i];
		if (cur == 0) { exitf = true; } else if (cur == -1) { cur = max + 1; }
		int diff = cur - prec;
		if (diff < 0) { diff = - diff; min = 1; } else { min = 0; }
		bw->writeVbit(diff);
		if (diff > 0) { bw->writeBit(min); }
		prec = cur;
	}
}

void HTree::cut()
{
	codes = NULL;
    while (left != NULL && (left->freq == 0 || right->freq == 0) ) {
		if (left->freq == 0) {
			this->freq = right->freq;
			this->val = right->val;
			this->left = right->left;
			this->right = right->right;
		} else if (right->freq == 0) {
			this->freq = left->freq;
			this->val = left->val;
			this->left = left->left;
			this->right = left->right;
		}
    }
    if (left != NULL) { left->cut(); }
    if (right != NULL) { right->cut(); }
}

void HTree::writeVal(binaryWrite *bm, int val) {
    if (codes == NULL) {codes = this->getCodes();}
	bm->fwrite(codes[val], codeBits[val]);
}

void HTree::writeVal(binaryWrite *bm, int val, bool boolean) {
    if (codes == NULL) {codes = getCodes();}
	bm->fwrite(codes[val], codeBits[val]);
}

