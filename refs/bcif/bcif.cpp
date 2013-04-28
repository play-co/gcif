/*
 *  bcif.cpp
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
#include <typeinfo>
#include <string>
#include <math.h>
#include <vector>
using namespace std;

#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#include "bmpWriter.h"
#include "newBitReader.h"
#include "fileBmpWriter.h"
#include "binaryWrite.h"
#include "HTree.h"
#include "huffmanReader.h"
#include "zeroHuffmanReader.h"
#include "oneValHuffmanReader.h"
#include "standardHuffmanReader.h"
#include "HTreeReaderGestor.h"

#include "HTreeWriterGestor.h"

#include "filterer.h"
#include "colorFilterer.h"
#include "filterGestor.h"
#include "newFilterGestor.h"

#include "readHeader.h"
#include "bitMatrix.h"
#include "byteMatrix.h"
#include "costEvaluator.h"
#include "BmpImage.h"
#include "bcif.h"

void bcif::decompress(string source)
{
	string dest = *(new string(source));
	if (dest.rfind(".bcif") == dest.length() - 5) {
		dest = dest.substr(0, source.length() - 5);	
	}	
	dest.append(".bmp");	
	decompress(source, dest);
}

void bcif::decompress(string source, string dest) {
	cout << "Decompressing " << source << endl << "           to " << dest << endl;
	ifstream in(source.c_str(),ios::in|ios::binary);
	if (!in.is_open()) { cout << "Invalid file name, or unable to read file. " << endl; exit(0); }
	newBitReader *bitReader  = new newBitReader(in);
	fileBmpWriter *bmpOut  = new fileBmpWriter(dest.c_str());
	readBcifIntest(bitReader, bmpOut);
	hdecompressBetter(bitReader,bmpOut);
}

void bcif::readBcifIntest(newBitReader *br, fileBmpWriter *bmpOut) {
	int t1 = br->fread(8); 
	int t2 = br->fread(8);
	int t3 = br->fread(8);
	int t4 = br->fread(8);
	if (t1 != (int)'B' || t2 != (int)'C' || t3 != (int)'I' || t4 != (int)'F') {
		printf("Error: BCIF marker not found ! \n");
		printf("Input file may not be a BCIF file. \n");
		exit(1);
	}
	int readVersion = br->fread(16);
	int readSubVersion = br->fread(16);
	/*int readBeta =*/ br->fread(8);
	
    if ((readVersion > version) || ((readVersion == version) && (readSubVersion > subVersion))) {
    	printf("Error: BCIF file version (%d.%d) is newer than decoder version (%d.%d).\n", 
    	        readVersion, readSubVersion, version, subVersion);
		printf("Cannot decompress file ! Program terminating.\n");
		exit(1);
    }	

    int extraDataLength = br->fread(24);
	  
	for (int i = 0; i < extraDataLength; i++) {    // Read auxiliary data
		br->fread(8);                              // Unused for now
	}
	  
	int width=br->fread(32);
	int height=br->fread(32);
	bmpOut->setDims(width, height);        
	
	int resX=br->fread(32);
	int resY=br->fread(32);
	bmpOut->setRes(resX, resY);	
}

void bcif::fillFLookups(HTree *curht, int* curend, int FLsize, int i, int nowaux) {
	//int *codes = curht->getCodes();
	//int *codeBits = curht->getCodeBits();	
	for (int v = 0; v < FLsize; v++) {
		int curv = v;
		int curbitnum = 0;
		int* lkey;
		if (nowaux == 0) {
			lkey = HTRLookupsValues[i];
		} else {
			lkey = HTRLookupsAuxValues[i];			
		}
		int myval = lkey[0];
		int curp = 0;
		int prevp = 0;
		while (myval < 0 && curbitnum <= FLbitnum) {
		int res = curv & 1;
			curv = curv >> 1;
			curbitnum ++;
			if (res == 0 ) {
				prevp = curp;
				curp = (-myval) & 4095;
				myval = lkey[curp];
  			} else {
				prevp = curp;
				curp = (-myval) >> 12;
  				myval = lkey[curp];  			
  			}
		}
		if (curbitnum <= FLbitnum) {            // Value found directly
			curend[v] = myval + (curbitnum << 12);
		} else {                                // Jump
			curend[v] = prevp + (curbitnum << 12);
		}
	}
}

void bcif::createFastLookups(int num, huffmanReader **hreaders) {
	FLbitnum = 8;
	int FLsize = 1 << FLbitnum;
	FLEnd = (int**)calloc(num,sizeof(int*));
	FLAuxEnd = (int**)calloc(num,sizeof(int*));
	int* FLEndPoint = (int*)calloc(FLsize * num,sizeof(int));
	for (int i = 0; i < num; i++) {
		FLEnd[i] = FLEndPoint;
		FLEndPoint += FLsize;
		int* curend = FLEnd[i];
		if (HTRtypes[i] == 0) {
			for (int v = 0; v < FLsize; v++) {
				curend[v] = 0;            // Value = 0, bits = 0
			}
		} else {
			HTree *curht = hreaders[i]->getTree();
			fillFLookups(curht, curend, FLsize, i, 0);
			if (HTRtypes[i] == 2) {
				FLAuxEnd[i] = (int*)calloc(FLsize,sizeof(int));
				curht = hreaders[i]->getAuxTree();
				fillFLookups(curht, FLAuxEnd[i], FLsize, i, 1);
			}
		}
	}
}

inline int bcif::readHVal(newBitReader *bm, int *lkey, int start) {
	int myval = lkey[start];
	while (myval < 0) {
  		if (bm->readBit() == 0 ) {
	  		myval = lkey[(-myval) & 4095];
  		} else {
  			myval = lkey[(-myval) >> 12];  			
  		}
	}
	return myval;
}

inline int bcif::readFLHVal(newBitReader *bm, int *lkey, int* end, int* bits) {
	int curval = end[bm->unsafeGetByte()];
	if ((curval >> 12) <= FLbitnum) {
		bm->discardBits(curval >> 12);
		return curval & 4095;
	} else {
		bm->discardBits(FLbitnum);
		int res = readHVal(bm, lkey, curval & 4095);
		bm->fillBuffer();
		return res;
	}
}

inline int bcif::readZHVal(newBitReader *br, int *lkey, int *alkey,
                                    int &after, int &zeros, int maxZeroSeq, int where) {
	if (zeros > 0) {
		zeros --;
		return 0;
	 } else {
		int cur = 0;
		if (after == 0) { cur = readFLHVal(br, lkey, FLEnd[where], FLBits[where]); } 
		else { cur = readFLHVal(br, alkey, FLAuxEnd[where], FLAuxBits[where]); }
		after = 0;
		 if (cur < 256) {			 
			 return cur;
		 } else {
			 zeros = cur - 255;
			 if (zeros == maxZeroSeq){
				 zeros += br->readVbit(8);
				 br->fillBuffer();
			 } else {
				 after = 1;
			 }
			 zeros --;			 
			 return 0;
		 }
	 }
}
	
int bcif::readVal(newBitReader *br, int where) {
	if (HTRtypes[where] == 0) {
		return 0;
	} else if (HTRtypes[where] == 1) {
		return readFLHVal(br, HTRLookupsValues[where], FLEnd[where], FLBits[where]);
		//return r1;
	} else if(HTRzzeros[where] > 0) {
		HTRzzeros[where]--;
		return 0;
	} else {
		return readZHVal(br, HTRLookupsValues[where], HTRLookupsAuxValues[where],
		                       HTRzafter[where], HTRzzeros[where], HTRzmaxzeroseq[where], where);
	}	
}

void bcif::genFastLookup(int **left, int **right, int **vals, int lnum, int *zseq, int *types, int isaux) {
	//int lim = 4096;
	int limbits = 12;
	if (isaux == 0) {
		cout << "Generating fast lookup of standard trees \n";
	} else {
		cout << "Generating fast lookup of auxiliary trees \n";
	}
	for (int i = 0; i < lnum; i++) {
		int curseq = 256;
		if (isaux == 0 && types[i] == 2) {
			curseq = curseq + zseq[i];
		}
		cout << "Tree " << i << " of type " << types[i] << ", value number: " << curseq << "\n";
		if (types[i] > 0 && (isaux == 0 || types[i] == 2)) {
			for (int i2 = 0; i2 < curseq; i2 ++) {
				if (vals[i][i2] < 0) {
					vals[i][i2] = -((left[i][i2] + (right[i][i2] << limbits)));
				}
			}
		}
	}
}

void bcif::printHTree(HTree * ht, int keynum, int ind) {
	int* codes = ht->getCodes();
	int* codeBits = ht->getCodeBits();
	for (int i = 0; i < keynum; i++) {
		if (codeBits[i] != -1)  {
			cout << i << " - " << codes[i] << " " << codeBits[i] << "\n";
		}
	}
	for (int i = 0; i < (1 << FLbitnum); i++) {
		cout << ind << " " << i << " - " << (FLEnd[ind][i] >> 12) << " " << (FLEnd[ind][i] & 4095) << "\n";
	}
}

void bcif::hdecompressBetter(newBitReader *br, bmpWriter *bmpOut){
	long int inittimeclock = clock();
	unsigned long time_msecs = 0;
	long int finaltimeclock = inittimeclock;	
	
	int width = bmpOut->getWidth();
	int height = bmpOut->getHeight();
	
	bmpOut->writeHeader();
	
	int filterDim = (((width - 1) / filterZoneDim + 1) * ((height - 1) / filterZoneDim + 1));
	if (verbose) { cout << "Reading spatial and color filters..."; }
	char *filters = (char *) calloc(filterDim,sizeof(char));
	readFilters(br, filters,filterDim);		
	char *colorFilters = readColorFilters(br, width, height, colorFilterZoneDim);
	if (verbose) { cout << " done." << endl; }
	
	if (verbose) { cout << "Reading and rebuilding Huffman trees..."; }
	HTreeReaderGestor hrg(maxDecisions, br);	
	hreaders=hrg.getReaders();
	if (verbose) { cout << " done." << endl; }
	
	// Creation of optimized lookup tables for Huffman trees
	/////////////////////////////////////////////////////// ------------------------------------------------------
	
	if (verbose) { cout << "Generating optimized structures for Huffman code reading..."; }
	HTRtypes = hrg.getTypes();
	HTRKeyNum = hrg.getKeyNum();
	HTRLookupsLeft = hrg.getLookupsLeft();
	HTRLookupsRight = hrg.getLookupsRight();
	HTRLookupsValues = hrg.getLookupsValues();
	for (int i2 = 0; i2 < maxDecisions; i2 ++) {
		if (HTRtypes[i2] > 0) {
			int valnum = 1024;
			for (int i = 0; i < valnum; i++) {
					if (HTRLookupsValues[i2][i] == -1) {
						HTRLookupsValues[i2][i] = - (HTRLookupsLeft[i2][i] + (HTRLookupsRight[i2][i] << 12));    
					}
			}
		}
	}
	
	HTRLookupsAuxLeft = hrg.getLookupsAuxLeft();
	HTRLookupsAuxRight = hrg.getLookupsAuxRight();
	HTRLookupsAuxValues = hrg.getLookupsAuxValues();
	for (int i2 = 0; i2 < maxDecisions; i2 ++) {
		if (HTRtypes[i2] == 2) {
			for (int i = 0; i < 1024; i++) {
				if (HTRLookupsAuxValues[i2][i] == -1) {
					HTRLookupsAuxValues[i2][i] = - (HTRLookupsAuxLeft[i2][i] + (HTRLookupsAuxRight[i2][i] << 12));	               
				}
			}
		}
	}
			
	HTRzzeros = hrg.getzzeros();
	HTRzmaxzeroseq = hrg.getzmaxzeroseq();
	HTRzafter = hrg.getzafter();

	createFastLookups(maxDecisions, hreaders);
		
	if (verbose) { cout << " done." << endl; }
	/////////////////////////////////////////////////////// ------------------------------------------------------
	
	
	filterGestor fg;	
	filterer *fil=NULL;													// Current filter
    colorFilterer *cfil=NULL;											// Current color filter
	int *precInfo0 = (int *) calloc(width * 9,sizeof(int));             // 'k' parameter for the previous row
	int *precInfo1 = precInfo0 + width; 
	int *precInfo2 = precInfo1 + width;
	
	int *info0 = precInfo2 + width;                                     // 'k' parameter for the current row
	int *info1 = info0 + width;
	int *info2 = info1 + width;
	
    int curFil = 0;															// Current filter index
    int curColFil = 0;														// Current color filter index
	    												    
	int *precLow0 = info2 + width;                                          // Decompressed values for previous row
	int *precLow1 = precLow0 + width;
	int *precLow2 = precLow1 + width;
		
	char *precLowFiltered0 = (char *) calloc(width * 6,sizeof(char));       // Reverse filtered values in previous row
	char *precLowFiltered1 = precLowFiltered0 + width; 
	char *precLowFiltered2 = precLowFiltered1 + width; 
											              
	char *lowFiltered0 = precLowFiltered2 + width;                          // Reverse filtered values in current row
	char *lowFiltered1 = lowFiltered0 + width;
	char *lowFiltered2 = lowFiltered1 + width;
	
	int left0 = 0, left1 = 0, left2 = 0;                        // Current/previous decompressed values
	int cfval0 = 0, cfval1 = 0, cfval2 = 0;                     // Current/previous decompressed and de-(spatial)filtered values
	int finalval0 = 0, finalval1 = 0, finalval2 = 0;            // Current/previous decompressed and de-(spatial+color)filtered values
	
	int i = 0;
	int i2 = 0;
	//int where = 0;
	
	int xZones = (int)(((width - 1) >> filterZoneDimBits) + 1);
	
	finaltimeclock = clock();
	time_msecs = (finaltimeclock - inittimeclock) * 1000 / CLOCKS_PER_SEC;
	cout << "Initialization of data structures time: "<< time_msecs << " ms." << endl;
		
	for (i2 = 0; i2 < height; i2 ++)                            // Row cicle
	{
		left0 = 0; left1 = 0; left2 = 0;

		int *tempInfo = precInfo0; precInfo0 = info0; info0 = tempInfo;
		tempInfo = precInfo1; precInfo1 = info1; info1 = tempInfo;
		tempInfo = precInfo2; precInfo2 = info2; info2 = tempInfo;
		
		char *tempFiltered = precLowFiltered0; precLowFiltered0 = lowFiltered0; lowFiltered0 = tempFiltered;
		tempFiltered = precLowFiltered1; precLowFiltered1 = lowFiltered1; lowFiltered1 = tempFiltered;
		tempFiltered = precLowFiltered2; precLowFiltered2 = lowFiltered2; lowFiltered2 = tempFiltered;
				
    	int zoneRef = (int)(xZones * (i2 >> filterZoneDimBits));
    	curFil = filters[zoneRef];
    	curColFil = colorFilters[zoneRef];
		fil = fg.getFilter(curFil);
		cfil = fg.getColFilter(curColFil);
    	
		for (i = 0; i < width; i ++) {
			
			if ((i & (filterZoneDim - 1)) == 0 && i > 0) {
				zoneRef ++;
		    	curFil = filters[zoneRef];
    			curColFil = colorFilters[zoneRef];
				fil = fg.getFilter(curFil);
				cfil = fg.getColFilter(curColFil); 				
			}
			
			br->fillBuffer();                                   // At least 24 bits in buffer
			
			if (i == 0) {
				left0 = readVal(br, decided(0, i2, 0, precInfo0, info0, info0, left0, precLow0[i]));				
				left1 = readVal(br, decided(0, i2, 1, precInfo1, info1, info0, left1, precLow1[i]));				
				left2 = readVal(br, decided(0, i2, 2, precInfo2, info2, info1, left2, precLow2[i]));							
			} else {								
				left0 = readVal(br, decideSafe0(i, i2, precInfo0, info0, left0, precLow0[i]));				
				left1 = readVal(br, decideSafe12(i, i2,1, precInfo1, info1, info0, left1, precLow1[i]));				
				left2 = readVal(br, decideSafe12(i, i2,2, precInfo2, info2, info1, left2, precLow2[i]));
			}
			
			precLow0[i] = left0;
			precLow1[i] = left1;
			precLow2[i] = left2;
						
			cfval0 = (left0 + cfil->colFilter(left0, left1, left2, 0));      // Color filter removal
			cfval1 = (left1 + cfil->colFilter(cfval0, left1, left2, 1));
			cfval2 = (left2 + cfil->colFilter(cfval0, cfval1, left2, 2));
						
			if (i > 0 && i < width - 1 && i2 > 0 && i2 < height-1) {         // Spatial filter removal		
				finalval0 = (char) (cfval0 + fil->filter(finalval0, precLowFiltered0[i], precLowFiltered0[i-1],precLowFiltered0[i+1]));
				finalval1 = (char) (cfval1 + fil->filter(finalval1, precLowFiltered1[i], precLowFiltered1[i-1],precLowFiltered1[i+1]));
				finalval2 = (char) (cfval2 + fil->filter(finalval2, precLowFiltered2[i], precLowFiltered2[i-1],precLowFiltered2[i+1]));			
			} else {
				finalval0 = (char) (cfval0 + safeFilter(i, i2, finalval0, precLowFiltered0, curFil, fil, width));
				finalval1 = (char) (cfval1 + safeFilter(i, i2, finalval1, precLowFiltered1, curFil, fil, width));
				finalval2 = (char) (cfval2 + safeFilter(i, i2, finalval2, precLowFiltered2, curFil, fil, width));				
			}
			
			bmpOut->write(finalval0);
			lowFiltered0[i] = finalval0;
			bmpOut->write(finalval1);
			lowFiltered1[i] = finalval1;
			bmpOut->write(finalval2);
			lowFiltered2[i] = finalval2;
							
		} // End of columns cicle
		bmpOut->flushBuffer();
	} // End of row cicle
	bmpOut->close();
	finaltimeclock = clock();
	time_msecs = (finaltimeclock - inittimeclock) * 1000 / CLOCKS_PER_SEC;
	cout << "Decompression finished; total decompression time: "<< time_msecs << " ms." << endl;
	
}

void bcif::readFilters(newBitReader *br, char *res, int filterDim)
{
	HTree *ht;
	ht=HTree::readHTreeFromBits(br, 12);
    ht->createLookup();
    for (int i = 0; i < filterDim; i ++) {
		res[i] = ht->readEfVal(br);
    }
}

void bcif::readFilters(newBitReader *br, int *res, int filterDim)
{
	HTree *ht;
	ht=HTree::readHTreeFromBits(br, 12);
    ht->createLookup();
    for (int i = 0; i < filterDim; i ++) {
		res[i] = ht->readEfVal(br);
    }
}


char* bcif::readAdvancedColorFilters(newBitReader *br, int w, int h, int filterZoneDim)
{
	return NULL;
}

char* bcif::readColorFilters(newBitReader *br, int w, int h, int filterZoneDim)
{
	char *res;
	int type = br->readBit();
    if (type == 1) {
		res = NULL;            // Advanced filters not implemented
    } else {
		int filterDim = (int) ( ( (w - 1) / colorFilterZoneDim + 1) *( (h - 1) / colorFilterZoneDim + 1));
		res = (char *) calloc(filterDim,sizeof(char));
		HTree *ht;
		ht=HTree::readHTreeFromBits(br, 64);
		ht->createLookup();
		int cf = -1;
		for (int i = 0; i < filterDim; i++) {
			if (cf == -1) {
				cf = ht->readEfVal(br);
				res[i] =  (cf >> 3);
			} else {
				res[i] = (cf & 7);
				cf = -1;
			}
		}
    }
	return res;
}

int* bcif::readColorFiltersInt(newBitReader *br, int w, int h, int filterZoneDim)
{
	int *res;
	int type = br->readBit();
    if (type == 1) {
		res = NULL;
    } else {
		int filterDim = (int) ( ( (w - 1) / colorFilterZoneDim + 1) *( (h - 1) / colorFilterZoneDim + 1));
		res = (int *) calloc(filterDim,sizeof(int));
		HTree *ht;
		ht=HTree::readHTreeFromBits(br, 64);
		ht->createLookup();
		int cf = -1;
		for (int i = 0; i < filterDim; i++) {
			if (cf == -1) {
				cf = ht->readEfVal(br);
				res[i] =  (cf >> 3);
			} else {
				res[i] = (cf & 7);
				cf = -1;
			}
		}
    }
	return res;
}

char bcif::filterOfZone(int i, int i2, int width, char *lineFilter) {
    int xZones = (int)(((width - 1) >> filterZoneDimBits) + 1);
    int zoneRef = (int)((i >> filterZoneDimBits) + xZones * (i2 >> filterZoneDimBits));
    return lineFilter[zoneRef];
}

int bcif::filterOfZoneInt(int i, int i2, int width, int *lineFilter) {
    int xZones = ((width - 1) >> filterZoneDimBits) + 1;
    int zoneRef = (i >> filterZoneDimBits) + xZones * (i2 >> filterZoneDimBits);
    return lineFilter[zoneRef];
}

char bcif::colorFilterOfZone(int i, int i2, int width, char *colorLineFilter) {
    int xZones = (int)(((width - 1) >> colorFilterZoneDimBits) + 1);
    int zoneRef = (int)((i >> colorFilterZoneDimBits) + xZones * (i2 >> colorFilterZoneDimBits));
    return colorLineFilter[zoneRef];
}

int bcif::colorFilterOfZoneInt(int i, int i2, int width, int *colorLineFilter) {
    int xZones = ((width - 1) >> colorFilterZoneDimBits) + 1;
    int zoneRef = (i >> colorFilterZoneDimBits) + xZones * (i2 >> colorFilterZoneDimBits);
    return colorLineFilter[zoneRef];
}

int bcif::decide(int x, int y, int col, int **precInfo, int **info, int curFil, int curColFil, int left, int low) {
	int leftInfo = 0;
    if (x > 0) {
		leftInfo = info[col][x - 1];
    } else {
		leftInfo = precInfo[col][x];
    }
    int lowInfo = precInfo[col][x];
    int caos = loga2[(mod256(left) + mod256(low)) << 1] + 1;
    if (caos > 7) {
		caos = 7;
    }
    int caos2 = 0;
    if (col > 0 ) {
		caos2 = info[col - 1][x];
    } else {
		caos2 = (leftInfo + lowInfo) >> 1;
    }
    int curCaos = (((caos << 2) + (leftInfo + lowInfo + caos + caos2)) >> 2);
    info[col][x] = (curCaos >> 1);
	return curCaos + (col << 4);
}

int bcif::decided(int x, int y, int col, int *precInfo, int *info, int *pcinfo, /*int curFil, int curColFil,*/ int left, int low) {
    if (x > 0) {
		leftInfo = info[x - 1];
    } else {
		leftInfo = precInfo[x];
    }
    lowInfo = precInfo[x];
    caos = loga2[(mod256(left) + mod256(low)) << 1] + 1;
    if (caos > 7) {
		caos = 7;
    }
    caos2 = 0;
    if (col > 0 ) {
		caos2 = pcinfo[x];
    } else {
		caos2 = (leftInfo + lowInfo) >> 1;
    }
    curCaos = (((caos << 2) + (leftInfo + lowInfo + caos + caos2)) >> 2);
    info[x] = (curCaos >> 1);
	return curCaos + (col << 4);
}

char bcif::safeFilter(int x, int y, char left, char low, char ll, char lr, int curFil, filterer *fil, int width) {
    char res = 0;
    if (x > 0 && x < width - 1 && y == 0) {     // On the lower side
		if (curFil != 1) {
			res = left;
		} else {
			res = 0;
		}
    } else if (x == 0 && y > 0) {               // On the left side
		if (curFil != 1 && curFil != 4 && curFil != 7 && curFil != 11) {
			res = (curFil == 1) ? 0 : low;
		} else {
			res = fil->filter(left, low, ll, lr);
		}
    } else if (x == width - 1 && y > 0) {       // On the right side
		if (curFil == 7 || curFil == 10 || curFil == 11) {
			res = (curFil == 1) ? 0 : left;
		} else {
			res = fil->filter(left, low, ll, lr);
		}
    } else if (x == 0 && y == 0) {              // Lower left corner
        res = 0;
    } else if (x == width - 1 && y == 0) {      // Lower right corner
		if (curFil != 1) {
			res = left;
		} else {
			res = 0;
		}
    } else {                                    // Other locations
		res = fil->filter(left, low, ll, lr);
    }
    return res;
}

int bcif::safeFilter(int x, int y, int left, int low, int ll, int lr, int curFil, filterer *fil, int width) {
    int res = 0;
    if (x > 0 && x < width - 1 && y == 0) {     // On the lower side
		if (curFil != 1) {
			res = left;
		} else {
			res = 0;
		}
    } else if (x == 0 && y > 0) {               // On the left side
		if (curFil != 1 && curFil != 4 && curFil != 7 && curFil != 11) {
			res = (curFil == 1) ? 0 : low;
		} else {
			res = fil->filter(left, low, ll, lr);
		}
    } else if (x == width - 1 && y > 0) {       // On the right side
		if (curFil == 7 || curFil == 10 || curFil == 11) {
			res = (curFil == 1) ? 0 : left;
		} else {
			res = fil->filter(left, low, ll, lr);
		}
    } else if (x == 0 && y == 0) {              // Lower left corner
        res = 0;
    } else if (x == width - 1 && y == 0) {      // Lower right corner
		if (curFil != 1) {
			res = left;
		} else {
			res = 0;
		}
    } else {                                    // Other locations
		res = fil->filter(left, low, ll, lr);
    }
    return res;
}

char bcif::safeFilter(int x, int y, char left, char low[], int curFil, filterer *fil, int width) {
    char ll = 0;
    char lr = 0;
    if (x == 0) {
		ll = 0;
    } else {
		ll = low[x - 1];
    }
    if (x == width - 1) {
		lr = 0;
    } else {
		lr = low[x + 1];
    }
    return safeFilter(x, y, left, low[x], ll, lr, curFil, fil, width);
}

int bcif::safeFilter(int x, int y, int left, int low[], int curFil, filterer *fil, int width) {
    int ll = 0;
    int lr = 0;
	if(x != 0) {
		ll = low[x - 1];
    }
	if (x != width - 1) {
		lr = low[x + 1];
    }
    return safeFilter(x, y, left, low[x], ll, lr, curFil, fil, width);
}




inline int bcif::mod256(int val) {
    return mod256a[val];
}

inline int bcif::decideSafe0(int x, int y, int *precInfo, int *info, /*int curFil, int curColFil,*/ int left, int low) {
	leftInfo = info[x - 1];
    lowInfo = precInfo[x];
    caos = loga2d[(mod256a[left] + mod256a[low]) << 1] + 1;
    //if (caos > 7) { caos = 7; }  // Included in lookup table loga2d
    caos2 = (leftInfo + lowInfo) >> 1;
    curCaos = (((caos << 2) + (leftInfo + lowInfo + caos + caos2)) >> 2);
    info[x] = (curCaos >> 1);
	return curCaos;
}

inline int bcif::decideSafe12(int x, int y, int col, int *precInfo, int *info, int *pcinfo, /*int curFil, int curColFil,*/ int left, int low) {
	leftInfo = info[x - 1];
    lowInfo = precInfo[x];
    caos = loga2d[(mod256a[left] + mod256a[low]) << 1] + 1;
    //if (caos > 7) { caos = 7; }  // Included in lookup table loga2d
    caos2 = pcinfo[x];
    curCaos = (((caos << 2) + (leftInfo + lowInfo + caos + caos2)) >> 2);
    info[x] = (curCaos >> 1);
    return curCaos + (col << 4);
}

void bcif::writeBcifHeader(BmpImage *si, binaryWrite *bw) {
	bw->fwrite((int)'B', 8);
	bw->fwrite((int)'C', 8);
	bw->fwrite((int)'I', 8);
	bw->fwrite((int)'F', 8);
	bw->fwrite(version, 16);
	bw->fwrite(subVersion, 16);
	bw->fwrite(beta, 8);
	int extraDataLength = 0;
	bw->fwrite(extraDataLength, 24);
    for (int i = 0; i < extraDataLength; i++) {    // Write auxiliary data
		bw->fwrite(0, 8);                          // Unused for now
	}	
	bw->fwrite((int)si->getWidth(), 32);
	bw->fwrite((int)si->getHeight(), 32);
	bw->fwrite(si->getResX(), 32);
	bw->fwrite(si->getResY(), 32);	
}

// -- Compressione --

void bcif::compress(string source) {
	string dest = *(new string(source));
	if (dest.rfind(".bmp") == dest.length() - 4) {
		dest = dest.substr(0, source.length() - 4);	
	}	
	dest.append(".bcif");
	compress(source, dest);
}
 
void bcif::compress(string source, string dest) {
	
	unsigned long inittimeclock, finaltimeclock, medtimeclock, time_msecs;
	inittimeclock = clock();
	cout << "Compressing " << source << endl << "         to " << dest << endl << endl;
	
    BmpImage *si = new BmpImage(source);
	si->setZoneDim(filterZoneDim);
    si->setColorZoneDim(colorFilterZoneDim);
    if (verbose) { cout << "Determinating spatial filters..."; }
    si->filterDeterminate();
    if (verbose) { cout << "done. \nApplying spatial filters..."; }
	si->applyFilter(true);
	
	if (verbose) { cout << "done. \n"; }
	medtimeclock = clock();
	time_msecs = (medtimeclock - inittimeclock) * 1000 / CLOCKS_PER_SEC;
	cout << "Filter determination and appliance time       : "<< time_msecs << " ms." << endl; 
	finaltimeclock = medtimeclock;
		
	if (verbose) { cout << "Determinating color filters..."; }	
	si->colorFilterDeterminate();
	if (verbose) { cout << "done. \nApplying color filters..."; }
	si->applyColFilter();
	if (verbose) { cout << "done. \n"; }

	medtimeclock = clock();
	time_msecs = (medtimeclock - finaltimeclock) * 1000 / CLOCKS_PER_SEC;
	cout << "Color filter determination and appliance time : "<< time_msecs << " ms." << endl;
	finaltimeclock = medtimeclock;
		
	ofstream fileOut;
	fileOut.open(dest.c_str(),ios::out|ios::binary);
	binaryWrite *bw = new binaryWrite(&fileOut);
	writeBcifHeader(si, bw);
	writeFilters(si->getZoneFilters(),si->getlengthlineFilter(), bw);
	writeColFilters(si->getColorZoneFilters(),si->getZoneNum(), bw);
	byteMatrix *image = si->getImage();
	
	compressMatrix(image, bw, si->getZoneFilters(), si->getColorZoneFilters());
	
	bw->close();
	if(fileOut.is_open())
	{
		fileOut.close();
	}

	medtimeclock = clock();
	time_msecs = (medtimeclock - finaltimeclock) * 1000 / CLOCKS_PER_SEC;
	cout << "Prediction errors compression time            : "<< time_msecs << " ms." << endl;
	finaltimeclock = clock();
	time_msecs = (finaltimeclock - inittimeclock) * 1000 / CLOCKS_PER_SEC;
	cout << "Total encoding time                           : "<< time_msecs << " ms." << endl;
	
	ifstream s,d;
	s.open(source.c_str(), ios::in);
	d.open(dest.c_str(), ios::in);
	s.seekg (0, ios::end);
	d.seekg (0, ios::end);
	long end1,end2;
	end1 = s.tellg();
	end2 = d.tellg();
	double ratio = (double)end1 / end2;
	cout << "File compressed from "<< round((double)(end1/1024)) << " to " << round((double)(end2/1024)) << " KB";
	printf(", compression ratio: %4.2f", ratio);
	cout << endl << endl;
	
}

void bcif::writeFilters(char *filters,int length, binaryWrite *bw) {
	int freqLength = 12;
	int *freqs = (int *) calloc(freqLength,sizeof(int));
    for (int i = 0; i < length; i ++) {
		freqs[(int)filters[i]] ++;
    }
	HTree htree;
    HTree *ht = htree.buildHTree(freqLength,freqs);
    ht->cut();
    ht = htree.buildHTreeFromBits(ht->getCodeBits(),freqLength);
	ht->writeHTreeFromBits(bw);
    for (int i = 0; i < length; i ++) {
		ht->writeVal(bw, filters[i]);
	}
}

void bcif::writeColFilters(char *colorFilters,int length, binaryWrite *bw) {
    if (advancedColorFilter) {
		cout << "AdvancedColorFilter not implemented" << endl;
		exit(1);
    } else {
		bw->writeBit(0);
		int cfreqLength = 64;
		int *cfreqs = (int *) calloc(cfreqLength,sizeof(int));;
		int cf = -1;
		for (int i = 0; i < length; i++) {
			if (cf == -1) { cf = colorFilters[i]; } else {
				cfreqs[ (cf << 3) + colorFilters[i]]++;
				cf = -1;
			}
		}
		if (cf > -1) {
			cfreqs[ (cf << 3)]++;
			cf = -1;
		}
		HTree chtree;
		HTree *cht = chtree.buildHTree(cfreqLength,cfreqs);
		cht->cut();
		int *codeBits = cht->getCodeBits();
		cht = chtree.buildHTreeFromBits(codeBits,cfreqLength);
		cht->writeHTreeFromBits(bw);
		int val =0;
		for (int i = 0; i < length - 1; i += 2) {
			int x = colorFilters[i + 1];
			int y = colorFilters[i];
			val = (y << 3) + x;
			cht->writeVal(bw, val,true);
		}
		if ( (length % 2) == 1) {
			cht->writeVal(bw, (colorFilters[length - 1] << 3),true);
		}
	}
}


static int chaosScore(unsigned char p) {
	if (p < 128) {
		return p;
	} else {
		return 256 - p;
	}
}


static const unsigned char CHAOS_TABLE[512] = {
	0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
};



int bcif::decideMatrix(int x, int y, int col, int **precInfo, int **info, int curFil, int curColFil, int left, int low) {
	return CHAOS_TABLE[chaosScore(left) + chaosScore(low)];
/*	int leftInfo = 0;
	int lowInfo = precInfo[col][x];
    if (x > 0) {
		leftInfo = info[col][x - 1];
    } else {
		leftInfo = lowInfo;
    }
	int index = (mod256(left) + mod256(low)) << 1;
	int caos = loga2[index] + 1;
    if (caos > 7) {
		caos = 7;
    }
	int caos2 = 0;
    if (col > 0 ) {
		caos2 = info[col - 1][x];
    } else {
		caos2 = (leftInfo + lowInfo) >> 1;
    }
	int shiftCaos = caos << 2;
	int somma = leftInfo + lowInfo + caos + caos2;
	int somma2 = shiftCaos + somma;
	int curCaos = somma2 >>2;
	info[col][x] = (curCaos >> 1);
	return curCaos + (col << 4);*/
}


void bcif::compressMatrix(byteMatrix *image, binaryWrite *out, char *filters, char *colorFilters) {
	
	//struct timeval t,f;
	//gettimeofday(&t, NULL);
    int filterWroteBits = out->getWroteBits();
    if (verbose) {
	    cout << "Filter and intestation size is " << filterWroteBits << " bits (" 
	         << (filterWroteBits >> 3) << " bytes)" << endl;
    }
	HTreeWriterGestor **htg = new HTreeWriterGestor *[maxDecisions];
    for (int i3 = 0; i3 < maxDecisions; i3 ++) {
		htg[i3] = new HTreeWriterGestor();
    }
    image->firstVal();
    int w = image->getWidth();
    int h = image->getHeight();
    int where = 0;
	
    int *allmemory = (int*) calloc(w * 18,sizeof(int));
    int *curmemory = allmemory;
        
	int **info = (int **) calloc (3,sizeof(int *));
	info[0] = curmemory; curmemory += w; 
	info[1] = curmemory; curmemory += w; 
	info[2] = curmemory; curmemory += w; 
    int **precInfo = (int **) calloc (3,sizeof(int *));
	precInfo[0] = curmemory; curmemory += w;
	precInfo[1] = curmemory; curmemory += w;
	precInfo[2] = curmemory; curmemory += w;
    
    int curFil = 0;
    int curColFil = 0;
	
    int **low = (int **) calloc (3,sizeof(int *));
	low[0] = curmemory; curmemory += w;
	low[1] = curmemory; curmemory += w;
	low[2] = curmemory; curmemory += w;
    int **precLow = (int **) calloc (3,sizeof(int *));
	precLow[0] = curmemory; curmemory += w;
	precLow[1] = curmemory; curmemory += w;
	precLow[2] = curmemory; curmemory += w;
	
	int **tempInfo;
	tempInfo	= (int **) calloc(3,sizeof(int *));
	tempInfo[0] = curmemory; curmemory += w;
	tempInfo[1] = curmemory; curmemory += w;
	tempInfo[2] = curmemory; curmemory += w;
	int **tempLow=NULL;
	tempLow	   = (int **) calloc(3,sizeof(int *));
	tempLow[0] = curmemory; curmemory += w;
	tempLow[1] = curmemory; curmemory += w;
	
	int *left = (int *) calloc (3,sizeof(int));

	if (verbose) { cout << "Data gathering for compression start..."; }
	
	image->firstVal();
    for (int i2 = 0; i2 < h; i2 ++) {
	    left[0] = 0; left[1] = 0; left[2] = 0;
		
		tempLow = precLow;
		precLow = low;
		low = tempLow;
		
		tempInfo = info;
		info = precInfo;
		precInfo = tempInfo;
		
		for (int i = 0; i < w; i ++) {
			for (int i3 = 0; i3 < 3; i3 ++) {
				where = decideMatrix(i, i2, i3, precInfo, info, curFil, curColFil, left[i3], precLow[i3][i]);
				int val = (int)image->getIntCurVal();
				htg[where]->putVal(val);
				left[i3] = val;
				low[i3][i] = val;
				image->nextVal();
			}
		}
		
    }
	
	if (verbose) { cout << " done." << endl << "Encoding Huffman trees..."; }
    for (int i3 = 0; i3 < maxDecisions; i3 ++) {
		htg[i3]->endGathering();
		htg[i3]->setBitWriter(out);
		htg[i3]->writeTree(true);
    }
    if (verbose) { 
	    cout << " done." << endl;
    	cout << "Huffman tree used bits are " << out->getWroteBits() << " (" << ((out->getWroteBits() - filterWroteBits) >> 3) << " bytes) for " << maxDecisions << " trees." <<endl;
    	cout << "Starting Huffman coding...";
	}
	
	memset(allmemory, 0, w * 18 * sizeof(int));
	
	image->firstVal();
    for (int i2 = 0; i2 < h; i2 ++) {
	    left[0] = 0; left[1] = 0; left[2] = 0;
		
		tempLow = precLow;
		precLow = low;
		low = tempLow;
		
		tempInfo = info;
		info = precInfo;
		precInfo = tempInfo;
		
		for (int i = 0; i < w; i ++) {
			for (int i3 = 0; i3 < 3; i3 ++) {
				where = decideMatrix(i, i2, i3, precInfo, info, curFil, curColFil, left[i3], precLow[i3][i]);
				int val = image->getIntCurVal();
				htg[where]->writeVal(val);
				left[i3] = val;
				low[i3][i] = val;
				image->nextVal();
			}
		}
    }
    if (verbose) { cout << " done." << endl; }
}
