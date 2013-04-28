/*
 *  readHeader.h
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

#pragma pack(push, 1)
typedef struct tagBitmapFileHeader 
	{
		unsigned char bfType1;
		unsigned char bfType2;
		unsigned int bfSize;
		unsigned short bfReserved1;
		unsigned short bfReserved2;
		unsigned int bfOffBits;
	} BitmapFileHeader;

typedef struct tagBitmapInfoHeader 
	{
		unsigned int biSize;
		int biWidth;
		int biHeight;
		unsigned short biPlanes;
		unsigned short biBitCount;
		unsigned int biCompression;
		unsigned int biSizeImage;
		int biXPelsPerMeter;
		int biYPelsPerMeter;
		unsigned int biClrUsed;
		unsigned int biClrImportant;
	} BitmapInfoHeader;

#pragma pack(pop)
typedef union bitWriterUnion
{
	char byte1;
	char byte2;
	int integer;
} bitUnion;
int readHeaderFIle(char *file);
