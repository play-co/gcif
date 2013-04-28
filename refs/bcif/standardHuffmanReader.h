/*
 *  standardHuffmanReader.h
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

class HTree;
class newBitReader;
class standardHuffmanReader : public huffmanReader {
	HTree *ht;
	bool setVars;
public:
	standardHuffmanReader(HTree *tree) {
		setVars = false;
		ht=tree;
		ht->createLookup();
	}
	HTree* getTree() {
		return ht;
	}
	int readVal(newBitReader *br);
	int readValnPrint(newBitReader *br);
	int* getTypes();
	HTree* getAuxTree() {
		return NULL;
	}	
};
