/*
 *  oneValHuffmanReader.h
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

class HTree;
class oneValHuffmanReader : public huffmanReader {
	int val;
public:
	oneValHuffmanReader(int val2) {
		val = val2;
	}
	int readVal(newBitReader *br) {
		return val;
	}
	int readValnPrint(newBitReader *br) {
		return val;
	}
	HTree * getTree() {
		return NULL;
	}
	HTree* getAuxTree() {
		return NULL;
	}
};
