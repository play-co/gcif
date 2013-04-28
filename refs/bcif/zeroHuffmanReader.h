/*
 *  zeroHuffmanReader.h
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

class newBitReader;
class HTree;
class zeroHuffmanReader:public huffmanReader{
	HTree *ht;
	HTree *aux;
	HTree *curht;
public:
	int zeros;
	int maxZeroSeq;
	bool after;
	zeroHuffmanReader(){
		zeros = 0;
		maxZeroSeq = 128;
		after = false;
		ht=NULL;
		aux=NULL;
		curht=NULL;
	};
	zeroHuffmanReader(HTree *tree, HTree *haux, int mzs) {
		zeros = 0;
		maxZeroSeq = 128;
		after = false;
		ht = tree;
		aux = haux;
		maxZeroSeq = mzs;
		curht = ht;
		ht->createLookup();
		aux->createLookup();
	};
	zeroHuffmanReader(HTree *tree, HTree *haux, int mzs,newBitReader *br) {
		zeros = 0;
		maxZeroSeq = 128;
		after = false;
		ht = tree;
		aux = haux;
		maxZeroSeq = mzs;
		curht = ht;
		ht->createLookup(br);
		aux->createLookup(br);
	};
	HTree* getTree() {
		return ht;
	}	
	HTree* getAux() {
		return aux;
	}		
	HTree* getAuxTree() {
		return aux;
	}			
	int readVal(newBitReader *br);	
	int readValnPrint(newBitReader *br);
};
