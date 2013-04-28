/*
 *  readHeader.cpp
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
#include "readHeader.h"
using namespace std;

int readHeaderFIle(char *file) {
	BitmapFileHeader h;
	BitmapInfoHeader info;
	ifstream inStream(file,ios::in|ios::binary);
	inStream.read((char *) &h, sizeof h);
	inStream.read((char *) &info, sizeof info);
	cout << "h.bfType1 "<< (char)h.bfType1<<"\n";
	cout << "h.bfType2 "<< (char)h.bfType2<<"\n";
	cout << "h.bfSize "<< h.bfSize<<"\n";
	cout << "h.bfReserved1 "<< h.bfReserved1<<"\n";
	cout << "h.bfReserved2 "<< h.bfReserved2<<"\n";
	cout << "h.bfOffBits "<< h.bfOffBits<<"\n\n";
	
	cout << "info.biSize "<< info.biSize<<"\n";
	cout << "info.biWidth "<< info.biWidth<<"\n";
	cout << "info.biHeight "<< info.biHeight<<"\n";
	cout << "info.biPlanes "<< info.biPlanes<<"\n";
	cout << "info.biBitCount "<< info.biBitCount<<"\n";
	cout << "info.biCompression "<< (int)info.biCompression<<"\n";
	cout << "info.biSizeImage "<< info.biSizeImage<<"\n";
	cout << "info.biXPelsPerMeter "<< info.biXPelsPerMeter<<"\n";
	cout << "info.biYPelsPerMeter "<< info.biYPelsPerMeter<<"\n";
	cout << "info.biClrUsed "<< info.biClrUsed<<"\n";
	cout << "info.biClrImportant "<< info.biClrImportant<<"\n\n";

	return 0;
}
