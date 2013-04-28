/*
 *  newBitReader.cpp
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

newBitReader::~newBitReader() {
	if(in->is_open()) in->close();
}

inline void newBitReader::close() {
	if(in->is_open())
	{
		in->close();
	}
}


int newBitReader::fread(int bitNum)
{
	int res1=0;
	for(int i=0;i<bitNum;i++)
	{
		int rb=readBit();
		res1=res1+(rb<<i);
	}
	return res1;
}

int newBitReader::readOnef() {
    int res = 0;
    while (readBit() == 1) { res ++; }
    return res;
}

int newBitReader::readVbit(int initBits) {
    int bitNum = initBits;
    int precRep = -1;
    int rep = (1 << initBits) - 1;
    while (readBit() == 1) 
	{
		bitNum ++; 
		precRep = rep; 
		rep = (rep + (1 << bitNum));
	}
    return precRep + 1 + fread(bitNum);
}

int newBitReader::getFileSize()
{
	long begin,end;
	begin = in->tellg();
	in->seekg (0, ios::end);
	end = in->tellg();
	in->seekg (0, ios::beg);
	long fileSize=end-begin;
	return fileSize;
}

void newBitReader::readFile() {	
	in->read((char *)bufferInt, dimBuffer * sizeof(char));
}

