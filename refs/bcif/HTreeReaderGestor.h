/*
 *  HTreeReaderGestor.h
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

class HTree;
class huffmanReader;
class zeroHuffmanReader;
class oneValHuffmanReader;
class standardHuffmanReader;
class HTreeReaderGestor {
	
huffmanReader **readers;
public:
	int *zseq;
	int *keyNum;
	int * types;
	int **lleft, **lright, **lvalues, **alleft, **alright, **alvalues;
	int *zzeros, *zmaxzeroseq, *zafter;
	int hrnum;
	HTreeReaderGestor(int hr, newBitReader *br);	
	huffmanReader** getReaders() {
		return readers;
	}
	int* getTypes();
	int** getLookupsLeft();
	int** getLookupsRight();
	int** getLookupsValues();
	int** getLookupsAuxLeft();
	int** getLookupsAuxRight();
	int** getLookupsAuxValues();
	int * getzzeros();
	int * getzmaxzeroseq();
	int * getzafter();
	int * getKeyNum();
};


