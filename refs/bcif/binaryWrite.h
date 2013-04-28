/*
 *  binaryWrite.h
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

class binaryWrite{
	ofstream *fout;
	int buffer;
	char *bufChar;
	int bitCount;
	int wroteBits;
	char *arrBuffer;
	int dimBuffer;
	int currentSizeBuffer;
public:
	
	binaryWrite(ofstream *fp) {
		fout = fp;
		buffer = 0;
		bitCount = 0;
		wroteBits = 0;
		bufChar = 0;
		
		dimBuffer = 4096;
		currentSizeBuffer = 0;
		arrBuffer = (char *) calloc (dimBuffer,sizeof(char));		
	}
	
	void writeBit(int bit);	
	
	inline void fwrite(int num, int bitNum) {
		while (bitNum > 0) {
			writeBit(num & 1);
			num = num >> 1;
			bitNum--;
		}
	}
	
	void writeVbit(int num) {
		writeVbit(num, 0);
	}
	
	void writeOnef(int num) {
		for (int i = 0; i < num; i++) {
			writeBit(1);
		}
		writeBit(0);
	}
	
	void close();
	
	void writeVbit(int num, int initbits) {
		int bitNum = initbits;
		int repnum = (1 << bitNum) - 1;
		int precrep = -1;
		while (num > repnum) {
			bitNum++;
			precrep = repnum;
			repnum = repnum + (1 << bitNum);
		}
		for (int i = initbits; i < bitNum; i++) {
			writeBit(1);
		}
		writeBit(0);
		fwrite(num - precrep - 1, bitNum);
	}
	
	void writeBits(string bits) {
		int length = strlen(bits.c_str());
		for (int i = 0; i < length; i++) {
			if (bits[i] == 48) {
				writeBit(0);
			}
			else {
				writeBit(1);
			}
		}
	}
	
	inline int getWroteBits() {
		return wroteBits;
	}
};
