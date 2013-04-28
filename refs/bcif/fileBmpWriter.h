/*
 *  fileBmpWriter.h
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */


class fileBmpWriter:public bmpWriter{
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
	ofstream fileOut;
	int biWidth;
	int biHeight;
	int caxis, xaxis, yaxis, wrote;
	int adl;
	int cxaxis;
	char *buffer;
	char *currentBufferPoint;
	int dimBuffer;
	int written;
	int currentSizeBuffer;
	BitmapFileHeader fileHeader;
	BitmapInfoHeader fileInfoHeader;
public:
	fileBmpWriter(string dest){
		fileOut.open(dest.c_str(),ios::out|ios::binary|ios::trunc);
		if(!fileOut.is_open())
		{
			exit(1);
		}
		currentSizeBuffer = written = 0;
		caxis = 0;
		xaxis = 0;
		yaxis = 0;
		wrote = 0;
		written = 0;
		biWidth = 0;
		biHeight = 0;
	};
	
	//Destructor
	~fileBmpWriter(){
		close();
	};
	void createIntest(int imageWidth,int imageHeight);
	void setDims(int width, int height);
	void writeTriplet(char g,char b,char r);
	void writeTriplet(char *triplet);
	void write(char v);
	void close();
	int getBytesWritten();
	int getBufferWritten(){return written;}
	int getBufferSize(){return currentSizeBuffer;}
	void flushBuffer();
	void writeHeader();
	int getHeight();
	int getWidth();
	void setRes(int resx, int resy);
};
