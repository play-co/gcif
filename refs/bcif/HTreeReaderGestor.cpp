/*
 *  HTreeReaderGestor.cpp
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
#include "newBitReader.h"
#include "HTree.h"
#include "huffmanReader.h"
#include "oneValHuffmanReader.h"
#include "zeroHuffmanReader.h"
#include "standardHuffmanReader.h"
#include "HTreeReaderGestor.h"

HTreeReaderGestor::HTreeReaderGestor(int hr, newBitReader *br) {
	hrnum = hr;
	readers= new huffmanReader*[hr];
	types = (int*)calloc(hrnum * 6, sizeof(int));
	zzeros =      types + hrnum;
	zmaxzeroseq = types + hrnum * 2;
	zafter =      types + hrnum * 3;
	zseq =        types + hrnum * 4;
	keyNum =      types + hrnum * 5;
	lvalues = (int**)calloc(hrnum * 6, sizeof(int*));
	alvalues =    lvalues + hrnum;
	lright =      lvalues + hrnum * 2;
	lleft =       lvalues + hrnum * 3;
	alright =     lvalues + hrnum * 4;
	alleft =      lvalues + hrnum * 5;
	for (int i = 0; i < hr; i ++) {
		int bitNm=br->fread(8);
		zseq[i] = (1 << bitNm);
		if (zseq[i] > 1) {
			HTree *ht;
			ht=HTree::readHTreeFromBitsBR(br, 256 + zseq[i]);
			keyNum[i] = ht->getKeyN();
			HTree *aux;
			aux=HTree::readHTreeFromBitsBR(br, 256);
			zeroHuffmanReader *zhr = new zeroHuffmanReader(ht, aux, zseq[i], br);
			types[i] = 2;
			readers[i] = zhr;
			lleft[i] = zhr->getTree()->getLookupLeft();
			lright[i] = zhr->getTree()->getLookupRight();
			lvalues[i] = zhr->getTree()->getLookupValues();		
			alleft[i] = zhr->getAux()->getLookupLeft();
			alright[i] = zhr->getAux()->getLookupRight();
			alvalues[i] = zhr->getAux()->getLookupValues();
			zzeros[i] = 0;
			zmaxzeroseq[i] = zhr->maxZeroSeq;
			zafter[i] = 0;
		} else {
			HTree *ht;
			ht=HTree::readHTreeFromBits(br, 256);
			if (ht->leaf() && ht->getVal() == 0) {            // !! Changed: added val == 0
				int val=ht->getVal();
				oneValHuffmanReader *one = new oneValHuffmanReader(val);
				types[i] = 0;
				readers[i] = one;
				keyNum[i] = 1;
			} else {
				//ht->getValsFromBitReader(br);
				standardHuffmanReader *st= new standardHuffmanReader(ht);
				types[i] = 1;
				readers[i] = st;
				lleft[i] = st->getTree()->getLookupLeft();
				lright[i] = st->getTree()->getLookupRight();
				lvalues[i] = st->getTree()->getLookupValues();		
				keyNum[i] = ht->getKeyN();		
			}
		}
	}
}

int* HTreeReaderGestor::getTypes() {
	return types;
}

int** HTreeReaderGestor::getLookupsLeft() {
	return lleft;
}

int** HTreeReaderGestor::getLookupsRight() {
	return lright;	
}

int** HTreeReaderGestor::getLookupsValues() {
	return lvalues;
}

int** HTreeReaderGestor::getLookupsAuxLeft() {
	return alleft;	
}

int** HTreeReaderGestor::getLookupsAuxRight() {
	return alright;	
}

int** HTreeReaderGestor::getLookupsAuxValues() {
	return alvalues;	
}

int * HTreeReaderGestor::getzzeros() {
	return zzeros;
}

int * HTreeReaderGestor::getzmaxzeroseq() {
	return zmaxzeroseq;
}

int * HTreeReaderGestor::getzafter() {
	return zafter;
}

int * HTreeReaderGestor::getKeyNum() {
	return keyNum;
}


