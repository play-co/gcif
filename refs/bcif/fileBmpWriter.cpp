/*
 *  fileBmpWriter.cpp
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

#include <stdlib.h> 
#include <string.h> 
#include <stdlib.h> 
#include <string.h> 
#include <iostream>
#include <fstream>
 
using namespace std;

#include "bmpWriter.h"
#include "fileBmpWriter.h"
/*
  Create BMP header
 */
void fileBmpWriter::createIntest(int imageWidth,int imageHeight)
{
	int width=imageWidth*3;
	if (width % 4 > 0) { width += 4 - width % 4; }
	
	fileHeader.bfType1='B';
	fileHeader.bfType2='M';
    int adl = (4 - biWidth * 3 % 4) % 4;                     // Padding to make rows word-aligned
    int linewidth = biWidth * 3 + adl;	                     
	fileHeader.bfSize = linewidth * imageHeight + 54;        // Write header info
	fileHeader.bfReserved1 = fileHeader.bfReserved2 = 0;
	fileHeader.bfOffBits = 54;	
	fileInfoHeader.biSize = 40;                              // Write header info
	fileInfoHeader.biWidth = imageWidth;
	fileInfoHeader.biHeight = imageHeight;
	fileInfoHeader.biPlanes = 1;
	fileInfoHeader.biBitCount = 24;
	fileInfoHeader.biCompression = 0;
    fileInfoHeader.biSizeImage = fileHeader.bfSize - 54;
	fileInfoHeader.biClrUsed = 0;
	fileInfoHeader.biClrImportant = 0;
}

void fileBmpWriter::setDims(int width, int height) {
	biWidth = width;
	biHeight = height;
}

void fileBmpWriter::writeHeader() {
	createIntest(biWidth,	biHeight);
	adl = (int)(4 - (int)(biWidth * 3) % 4) % 4;
	fileOut.write((char *) &fileHeader, sizeof(fileHeader));
	fileOut.write((char *) &fileInfoHeader, sizeof(fileInfoHeader));
	dimBuffer = biWidth * 3 + adl;
	buffer = (char *) calloc (dimBuffer,sizeof(char));	
	currentBufferPoint = buffer;
}

int fileBmpWriter::getHeight() {
	return biHeight;
}

int fileBmpWriter::getWidth() {
	return biWidth;
}

void fileBmpWriter::setRes(int resx, int resy) {
	fileInfoHeader.biXPelsPerMeter = resx;
	fileInfoHeader.biYPelsPerMeter = resy;
}

// Not actually used
void fileBmpWriter::writeTriplet(char b, char g, char r) {
	fileOut.put(b);
	fileOut.put(g);
	fileOut.put(r);
	wrote += 3;
	xaxis++;
	if (xaxis == biWidth) {
		xaxis = 0;
		yaxis++;
		if(adl > 0) {
			char *x;
			x = (char *) calloc(adl,sizeof(char *));
			fileOut.write(x,adl*sizeof(char *));
		}
	}
}

// Not actually used
void fileBmpWriter::writeTriplet(char *triplet) {
	fileOut.write(triplet,3*sizeof(char));	
	wrote += 3;
	xaxis++;
	if (xaxis == biWidth) {
		xaxis = 0;
		yaxis++;
		char *x;
		x = (char *) calloc(adl,sizeof(char));
		fileOut.write(x,adl*sizeof(char));
	}	
}

inline void fileBmpWriter::write(char v) {
	currentBufferPoint[0] = v;
	currentSizeBuffer ++;
	currentBufferPoint ++;
}

inline void fileBmpWriter::flushBuffer() {
	fileOut.write(buffer, dimBuffer);
	currentSizeBuffer = 0;
	currentBufferPoint = buffer;
}
 
int fileBmpWriter::getBytesWritten() {
	return wrote;
}

void fileBmpWriter::close() {
	if(fileOut.is_open()) {
		if (currentSizeBuffer > 0) {
			fileOut.write(buffer,currentSizeBuffer);
		}
		fileOut.close();
	}
}
