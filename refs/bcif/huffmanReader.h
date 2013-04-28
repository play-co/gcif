/*
 *  huffmanReader.h
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

class HTree;
class newBitReader;
class huffmanReader{
public:
	virtual int readVal(newBitReader *br) = 0;
	virtual int readValnPrint(newBitReader *br) = 0;
	virtual HTree* getTree()=0;
	virtual HTree* getAuxTree()=0;
};
