/*
 *  HTree.h
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

class newBitReader;
class binaryWrite;
class HTree{
	HTree *left;
	HTree *right;
	int val;
	int freq;
	int keyN;
	int *codes;
	int *codeBits;
	bool createdByBits;
	int *lkey;
	int *lleft;
	int *lright;
	
	int bufferDimInt;
	int *bufferInt;
	int *curIntBuffer;
	int *bufint;
	int *dimBuffer;
	int *bitCount;
	
public:
	HTree() {
		left = NULL;
		right = NULL;
		codes = NULL;
		codeBits = NULL;
		lkey = NULL;
		lleft = NULL;
		lright = NULL;
		val = 0;
		freq = 0;
		keyN = 1;
		createdByBits = false;
	};

	HTree(int v, int f) {
		left = NULL;
		right = NULL;
		codes = NULL;
		codeBits = NULL;
		lkey = NULL;
		lleft = NULL;
		lright = NULL;		
		val = v;
		freq = f;
		keyN = 1;
		createdByBits = false;
	};

	HTree(HTree *l, HTree *r) {

		left = l;
		right = r;
		codes = NULL;
		codeBits = NULL;		
		lkey = NULL;
		lleft = NULL;
		lright = NULL;		
		val = 0;
		freq = 0;

		createdByBits = false;
		freq = l->getFreq() + r->getFreq();
		keyN = l->getKeyN() + r->getKeyN();
	};
	
	int* getLookupLeft();
	int* getLookupRight();
	int* getLookupValues();
	
	static HTree* readHTreeFromBits(newBitReader *br, int valNum );
	void readBits(newBitReader *br,int valNum,int *res);
	static HTree* buildHTreeFromBits (int *codeBits,int codeBitsLength);
	void createLookup();
	void recCreateLookup(int *k, int *l, int *r, int free, int* res);
	int readEfVal(newBitReader *bm);
	
	bool leaf() {
		return (left == NULL && right == NULL);
	}
	
	int getVal() {
		return val;
	}
	
	int readEfVal2(newBitReader *bm);
	
	inline void setCodeBits(int *ptr) {
		codeBits = ptr;
	}
	
	inline int getFreq() {
		return freq;
	}
	
	inline void setkeyN(int k) {
		keyN = k;
	}
	
	inline int getKeyN() {
		return keyN;
	}
	
	static HTree* buildHTree(int length,int *f);
	static HTree* buildHTree (int length,int *f, int minFreq);
	
	static HTree* buildHTree (int *f);	
	static HTree* buildHTree(int *f,int minFreq);
	
	int* getCodeBits() {
		if (codes == NULL) {
			codes = (int *) calloc(keyN,sizeof(int));
			codeBits = (int *) calloc(keyN,sizeof(int));
			for (int i = 0; i < keyN; i++) {
				codeBits[i] = -1;
			}
			fillCodes(codes, codeBits, 0, 0);
		}
		return codeBits;
	}
	
	void fillCodes(int *rc, int *rcb, int bit, int pVal) {
		if (left == NULL) {
			rc[val] = pVal;
			rcb[val] = bit;
		} else {
			left->fillCodes(rc, rcb, bit + 1, pVal);
			right->fillCodes(rc, rcb, bit + 1, pVal + (1 << bit));
		}
	}
	
	void writeHTreeFromBits(binaryWrite *bw) {
		if (! createdByBits) { cout << "Huffman tree not created by bits of values ! " << endl; exit(1); } else {
			writeBits(bw);
		}
	}
	
	void writeBits(binaryWrite *bw);	
	
	void cut();
	
	void writeVal(binaryWrite *bm, int val);
	
	int *getCodes() {
		if (codes == NULL) {
			codes = (int *) calloc(keyN,sizeof(int));
			codeBits = (int *) calloc(keyN,sizeof(int));
			for (int i = 0; i < keyN; i++) {
				codeBits[i] = -1;
			}
			fillCodes(codes, codeBits, 0, 0);
		}		
		return codes;
	}
	
	//static HTree* buildHTree(int length,int *f,bool boolean);
	//static HTree* buildHTree (int length,int *f, int minFreq,bool boolean);
	
	void writeVal(binaryWrite *bm, int val, bool boolean);	
	int readEfVal3(newBitReader *bm);
	void createLookup(newBitReader *br);
	void recCreateLookup(int *k, int *l, int *r, int free, int* res, newBitReader *br);
	static HTree* readHTreeFromBitsBR(newBitReader *br, int valNum );
	static HTree* buildHTreeFromBits (int *codeBits, int codeBitsLength, newBitReader *br);
};
