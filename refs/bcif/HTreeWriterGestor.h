/*
 *  HTreeWriterGestor.h
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

class HTreeWriterGestor{

	int tot;
	int zeros;
	int curZeros;

	int zsp;
	static const int simbolNum = 256;
	int zeroSeqNum;
	int zeroPercLim;
	int keyNum;
	HTree *ht ;
	HTree *aux ;
	HTree *curht ;
	binaryWrite *bw ;
	bool after;
	bool afterZero;
	int index;
	int sizef;
	int *nf;
	int *f;
	int *faz;
	vector<int> zs;
public:
	HTreeWriterGestor() {
		
		tot = 0;
		zeros = 0;
		curZeros = 0;
		zsp = 0;
		zeroSeqNum = 8;
		zeroPercLim = 30;
		keyNum = 0;
		ht = NULL;
		aux = NULL;
		curht = NULL;
		bw = NULL;
		after = false;
		afterZero = false;
		index = 0;
		sizef = simbolNum + zeroSeqNum;
		f = (int *) calloc(sizef,sizeof(int));
		faz = (int *) calloc(simbolNum,sizeof(int));
		nf = (int *) calloc(simbolNum,sizeof(int));
	}
	
	int getcurZeros() {
		return curZeros;
	}
	
	inline void setIndex(int i){index = i;}
	
	inline void getIndex() {		
		cout <<"hrg: "<<index<<" ";
	}
	
	void putVal(int v);
	
	inline int getfSize() {
		return sizef;
	}
	
	inline int getfazSize() {
		return simbolNum;
	}
	
	void endGathering();
	
	double entropy(int *simbolsFreq, int length) {
		double e = 0;
		int total = 0;
		for (int i = 0; i < length; i ++) {
			total += simbolsFreq[i];
		}
		double *relFreq = (double *) calloc(length,sizeof(double));
		for (int i = 0; i < length; i++) {
			relFreq[i] = (double)simbolsFreq[i] / total;
		}
		for (int i = 0; i < length; i ++) {
			if (relFreq[i] > 0) {
				e -= relFreq[i] * (log(relFreq[i]) / log(2));
			}
		}
		return e;
	}
	
	double HuffmanEntropy(int *simbolsFreq, int length);
	
	int loga2(int arg) {
		int res = 0;
		while (arg > 1) { arg = arg >> 1; res ++; }
		return res;
	}
	
	void setBitWriter(binaryWrite *bw2) {
		bw = bw2;
	}
	
	void getBitWriter()
	{
		cout << bw <<endl;
	}
	
	void writeTree() {
		int l = loga2(keyNum - simbolNum);
		bw->fwrite(l, 8);
		ht->writeHTreeFromBits(bw);
		if (l > 1) {
			aux->writeHTreeFromBits(bw);
		}
	}
	
	void writeTree(bool boolean) {
		int l = loga2(keyNum - simbolNum);
		bw->fwrite(l, 8);
		ht->getCodeBits();
		ht->writeHTreeFromBits(bw);
		if (l > 1) {
			aux->writeHTreeFromBits(bw);
		}
	}
	
	HTree* getTree() {
		return ht;
	}
	
	void writeVal(int v) {
		if (keyNum == simbolNum) {
			ht->writeVal(bw, v);
		} else {
			if (curZeros > 0) {
				curZeros --;
			} else {
				if (v == 0) {
					curZeros = zs.at(zsp);
					zsp ++;
					if (curZeros < zeroSeqNum) {
						ht->writeVal(bw, curZeros + simbolNum - 1);
						curht = aux;
					} else {
						ht->writeVal(bw, zeroSeqNum + simbolNum - 1);
						bw->writeVbit(curZeros - zeroSeqNum, 8);
					}
					curZeros --;
				} else {
					curht->writeVal(bw, v);
					curht = ht;
				}
			}
		}
	}
	
};
