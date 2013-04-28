/*
 *  zeroHuffmanReader.cpp
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

#include <stdlib.h> 
#include <string.h> 
#include <iostream>
using namespace std;
#include "newBitReader.h"
#include "HTree.h"
#include "huffmanReader.h"
#include "zeroHuffmanReader.h"

int zeroHuffmanReader::readVal(newBitReader *br) {
	if (zeros > 0) {
		zeros --;
		return 0;
	 } else {
		
		int cur = curht->readEfVal(br);
		curht = ht;
		 if (cur < 256) {
			 return cur;
		 } else {
			 zeros = cur - 255;
			 if (zeros == maxZeroSeq){
				 zeros += br->readVbit(8);
			 } else {
				 curht = aux;
			 }
			 zeros --;
			 after = true;
			 return 0;
		 }
	 }	 
	}
	
int zeroHuffmanReader::readValnPrint(newBitReader *br) {
	return readVal(br);
}
