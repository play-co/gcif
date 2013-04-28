/*
 *  standardHuffmanReader.cpp
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
#include "standardHuffmanReader.h"

int standardHuffmanReader::readVal(newBitReader *br) {
	return ht->readEfVal(br);
}
int standardHuffmanReader::readValnPrint(newBitReader *br) {
	return ht->readEfVal(br);
}
