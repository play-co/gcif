/*
 *  binaryWrite.cpp
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
#include "binaryWrite.h"

void binaryWrite::writeBit(int bit) {
	buffer = (buffer >> 1) + (bit << 7);
	bitCount++;	
	if (bitCount == 8) {
		if(currentSizeBuffer == dimBuffer) {
			fout->write(arrBuffer, dimBuffer * sizeof(char));
			currentSizeBuffer = 0;
		}
		
		arrBuffer[currentSizeBuffer] = buffer;
		currentSizeBuffer ++;
		
		bufChar = 0;
		bitCount = 0;
	}
	wroteBits ++;
}

void binaryWrite::close() {
	
	while (bitCount > 0) {
		writeBit(0);
	}
	
	if (currentSizeBuffer > 0) {
		fout->write(arrBuffer,currentSizeBuffer*sizeof(char));
	}
	
	fout->close();
}
