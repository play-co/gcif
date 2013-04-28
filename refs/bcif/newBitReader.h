/*
 *  newBitReader.h
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */


class newBitReader {
	ifstream *in;
	int dimBuffer;
	int res;

	unsigned char *bufferInt;
	int bufferDimInt;
	int curIntBuffer;
	
	int *newbufint;
	
	int bitCount;
	int currentBuffer;
	unsigned int bufint;	
	int readBits;
	
	int mbitCount;
	int mcurrentBuffer;
	int mbufint;	
	int mreadBits;	
	
public:
	
	
	newBitReader(ifstream &i) {
		in=&i;
		bitCount=0;
		readBits=0;
		
		dimBuffer = getFileSize();
		currentBuffer = 0;
		bufferInt = (unsigned char *) calloc (dimBuffer + 4, sizeof(unsigned char));
				
		readFile();
		bufint = 0;
		
	}
	~newBitReader();
	void readFile();
	int getFileSize();
	
	inline int readBit() {
		if (bitCount == 0) {
			bufint = bufferInt[currentBuffer] +
			         (bufferInt[currentBuffer + 1] << 8) +
			         (bufferInt[currentBuffer + 2] << 16) +
			         (bufferInt[currentBuffer + 3] << 24);

			bitCount = 32;
			currentBuffer += 4;
		}
		bitCount--;
		res = bufint & 1;
		bufint = bufint >> 1;
		readBits++;
		return res;		
	}
	
	void mark() {
		mbitCount = bitCount;
		mcurrentBuffer = currentBuffer;
		mbufint = bufint;
		mreadBits = readBits;
	}
	
	void reset() {
		bitCount = mbitCount;
		currentBuffer = mcurrentBuffer;
		bufint = mbufint;
		readBits = mreadBits;
	}	
	
	int getReadBits(){return readBits;};
	int fread(int bitNum);
	int readOnef();
	void close();
	int readVbit(int initBits=0);
	
	inline int getByte() {
		if (bitCount < 8) {
			bufint = bufint + (bufferInt[currentBuffer] << bitCount);
			currentBuffer++;
			bitCount = bitCount + 8;
		}
		return bufint & 255;
	}
	
	inline int unsafeGetByte() {
		return bufint & 255;
	}
	
	inline void fillBuffer() {
		while (bitCount <= 24) {
			bufint = bufint + (bufferInt[currentBuffer] << bitCount);
			currentBuffer++;
			bitCount = bitCount + 8;
		}
	}	
		
	inline void discardBits(int n) {
		bufint = bufint >> n;
		bitCount = bitCount - n;
		readBits = readBits + n;
	}
	
	inline int getbufferDimInt()
	{
		return bufferDimInt;
	}
	
	inline int* getcurIntBuffer()
	{
		return &curIntBuffer;
	}
	
	inline int* getdimBuffer()
	{
		return &dimBuffer;
	}
	
	inline int* getbitCount()
	{
		return &bitCount;
	}
	
};
