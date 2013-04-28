/*
 *  bitMatrix.h
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

class bitMatrix {
	void setVal() {
		width = 0;
		height = 0;
		count = 0;
		wroteBits = 0;
		lasty = -1;
		lastyres = -1;
		lastVal = -1;
		lastPoint = -1;
	}
	long width;
	long height;
	long count;
	long wroteBits;
	int lasty;
	int lastyres;
	int lastVal;
	int lastPoint;
public:
	int sizeMatrix;
	char *matrix;
	bitMatrix(long w, long h) {
		setVal();
		width = w;
		height = h;
		sizeMatrix = (((width + 1) * (height + 1)) >> 3) + 1;
		matrix = (char *) calloc (sizeMatrix,sizeof(char));
	}
	bitMatrix(ifstream *fin, int numBytes) {
		setVal();
		matrix = (char *) calloc (numBytes,sizeof(char));
		fin->read(matrix,numBytes*sizeof(char));
		width = numBytes << 3;
	}
	
	bitMatrix(bitMatrix *source) {
		setVal();
		width = source->width;
		height = source->height;
		count = source->count;
		wroteBits = source->wroteBits;
		int sizeMatrix = source->sizeMatrix;
		matrix = (char *) calloc(sizeMatrix,sizeof(char));
		memcpy(matrix,source->matrix,sizeMatrix);
	}
	
	bitMatrix(bitMatrix *source, bitMatrix *source2) {           // Xor of the two matrices
		setVal();
		width = source->width;
		height = source->height;
		count = source->count;
		wroteBits = source->wroteBits;
		matrix = (char *) calloc (source->sizeMatrix,sizeof(char));
		int minLen = source->sizeMatrix;
		if(source->sizeMatrix>source2->sizeMatrix) minLen = source2->sizeMatrix;
		for (int i = 0; i < minLen; i++) {
			matrix[i] = (char)(source->matrix[i] ^ source2->matrix[i]);
		}
	}
	
	void check(int x, int y) {
		if (x > width || y > height || x < 0 || y < 0) {
			cerr << "Index out of range: " << x << "," << y << " on " << width << "," << height;
			throw;
			exit(1);
		}
	}
	
	int hashCode() {
		int res = 0;
		int forlim = (int)(((width + 1) * (height + 1)) >> 3) + 1;
		for (int i = 0; i < forlim; i ++) {
			int v = matrix[i];
			if(v<0) v += 256;
			res = mod256(res + (v << (i & 15)), 30) ;
		}
		return res;
	}
	
	void setBit(int x, int y, int val) {
		//check(x,y);        // No check done for efficiency
		if (y != lasty) { lasty = y; lastyres = (int)(width * y); }
		int insPoint = (lastyres + x) >> 3;
		int insOffset = (lastyres + x) & 7;
		if (val == 0) {
			matrix[insPoint] = (char)(matrix[insPoint] & ((1 << insOffset) ^ 255));
		}
		else {
			matrix[insPoint] = (char)(matrix[insPoint] | (1 << insOffset));
		}
		
	}
	
	void setBit(int x, int y, char val) {
		//check(x,y);        // No check done for efficiency
		if (y != lasty) { lasty = y; lastyres = (int)(width * y); }
		int insPoint = (lastyres + x) >> 3;
		int insOffset = (lastyres + x) & 7;
		if (val == 0) {
			matrix[insPoint] = (char)(matrix[insPoint] & ((1 << insOffset) ^ 255));
		}
		else {
			matrix[insPoint] = (char)(matrix[insPoint] | (1 << insOffset));
		}
	}
	
	void setLine(int minx, int maxx, int y, char val) {
		for (int i = minx; i < maxx + 1; i++) {
			setBit(i, y, val);
		}
	}
	
	void xorBit(int x, int y, int val) {
		//check(x,y);        // No check done for efficiency
		if (val > 0) {
			if (y != lasty) {
				lasty = y;
				lastyres = (int) (width * y);
			}
			int insPoint = (lastyres + x) >> 3;
			int insOffset = (lastyres + x) & 7;
			matrix[insPoint] = (char) (matrix[insPoint] ^ (1 << insOffset));
		}
	}
	
	void xorFunction(bitMatrix *bm, int minx, int maxx, int miny, int maxy) {
		for (int i2 = miny; i2 < maxy + 1; i2 ++) {
			int i = minx;
			if (i2 != lasty) { lasty = i2; lastyres = (int)(width * i2); }
			long insPoint = (lastyres + minx) >> 3;
			long insOffset = (lastyres + minx) & 7;
			long finInsPoint = (lastyres + maxx) >> 3;
			int mxpu = maxx + 1;
			while (i < mxpu) {
				if ((insOffset & 7) == 0 && insPoint < finInsPoint) {
					matrix[ (int) insPoint] = (char)(matrix[ (int) insPoint] ^ bm->matrix[ (int) insPoint]);
					i += 8;
					insPoint++;
				} else {
					insOffset++;
					xorBit(i, i2, bm->getBit(i, i2));
					i++;
					insPoint += insOffset >> 3;
					insOffset = insOffset & 7;
				}
			}
		}
	}
	
	void xorFunction(bitMatrix *bm) {
		int forlim = (int)(((width + 1) * (height + 1)) >> 3) + 1;
		char *omatrix = bm->matrix;
		for (int i = 0; i < forlim; i ++) {
			matrix[i] = (char)(matrix[i] ^ omatrix[i]);
		}
	}
	
	int getEqBits(bitMatrix *bm, int minx, int maxx, int miny, int maxy, int p) {
		int res = 0;
		for (int i2 = miny; i2 <= maxy; i2 ++) {
			for (int i = minx + mod256((i2 - miny), 3); i <= maxx; i += p) {
				if (getBit(i, i2) == bm->getBit(i, i2)) { res ++; }
			}
		}
		return res * p;
	}
	
	int getZeroBits(int minx, int maxx, int miny, int maxy, int p) {
		int res = 0;
		for (int i2 = miny; i2 <= maxy; i2 ++) {
			int forlim = 0;
			if (p > 1) { forlim = minx + mod256((i2 - miny), 3); } else { forlim = minx; }
			for (int i = minx + mod256((i2 - miny), 3); i <= maxx; i += p) {
				if (getBit(i, i2) == 0) { res ++; }
			}
		}
		return res * p;
	}
	
	char getBit(int x, int y) {
		//check(x,y);        // No check done for efficiency
		if (lasty != y) {
			lasty = y;
			lastyres = (int)(y * width);
		}
		long insPoint = (lastyres + x) >> 3;
		long insOffset = (lastyres + x) & 7;
		lastPoint = (int)insPoint;
		lastVal = matrix[lastPoint];
		char r = (char)((lastVal >> insOffset) & 1);
		return r;
	}
	
	static char exBit(int source, int bit) {
		if ((source & (1 << bit)) == 0) { return 0; } else { return 1; }
	}
	
	int mod256(int source, int bits) {
		return (source << (32 - bits) >> (32 - bits));
	}
	
	int mod256(long source, int bits) {
		return (int)(source << (64 - bits) >> (64 - bits));
	}
	
	int mod256(char source, int bits) {
		if(source < 0) source = source + 256;
		return (mod256((int)source, bits));
	}
	
	char exBit(char source, char bit) {
		if ((source & (1 << bit)) == 0) { return 0; } else { return 1; }
	}
	
	char bitNum(char source) {
		char res = 0;
		for (int i = 0; i < 8; i++) {
			if ((source & 1) != 0) { res ++; }
			source = (char)(source >> 1);
		}
		return res;
	}
	
	void invert(int minx, int maxx, int miny, int maxy) {
		for (int i2 = miny; i2 < maxy + 1; i2 ++) {
			int i = minx;
			if (i2 != lasty) { lasty = i2; lastyres = (int)(width * i2); }
			long insPoint = (lastyres + minx) >> 3;
			long insOffset = (lastyres + minx) & 7;
			long finInsPoint = (lastyres + maxx) >> 3;
			int mxpu = maxx + 1;
			while (i < mxpu) {
				if ((insOffset & 7) == 0 && insPoint < finInsPoint) {
					matrix[ (int) insPoint] = (char)(matrix[ (int) insPoint] ^ 255);
					i += 8;
					insPoint++;
				} else {
					insOffset++;
					xorBit(i, i2, 1);
					i++;
					insPoint += insOffset >> 3;
					insOffset = insOffset & 7;
				}
			}
		}
	}
	
	void invert() {
		int r = (int)((width + 1) * (height + 1) / 8) + 1;
		for (int i = 0; i < r; i++) {
			matrix[i] = (char)(255 ^ matrix[i]);
		}
	}
	
	void writeImage(bitMatrix *bm) {
		for (int i = 0; i < width; i++) {
			for (int i2 = 0; i2 < height; i2 ++) {
				bm->writeBit(getBit(i, i2));
			}
		}
	}
	
	bitMatrix readImage(bitMatrix *bm, long width, long height) {
		bitMatrix *res = new bitMatrix(width, height);
		for (int i = 0; i < res->width; i++) {
			for (int i2 = 0; i2 < height; i2 ++) {
				res->setBit(i, i2, bm->readBit());
			}
		}
		return res;
	}
	
	int writeStream(ofstream *fout) {
		int writeSize = 0;
		writeSize = (int)(count >> 3);
		if ((count & 7) > 0) { writeSize ++; }
		fout->write(matrix,writeSize*sizeof(char));
		return writeSize;
	}
	
	 long read(int range) {
		return read((long)range);
	}
	
	long read(long range) {
		int res = 0;
		int exp = 0;
		long irange = range;
		while (range > 0 && irange - res >= 1 << exp) {
			res = res + (getBit((int)count,0) << exp);
			exp ++;
			range = range >> 1;
			count ++;
		}
		return res;
	}
	
	 int* read(int *range,int length) {
		int	*res = (int *) calloc (length,sizeof(int));
		long trange = 1;
		long tnum = 0;
		for (int i = 0; i < length && range[i] > 0; i++) {
			trange = trange * (range[i] + 1);
		}
		tnum = read(trange);
		for (int i = 0; tnum > 0; i++) {
			res[i] = (int) (tnum % (range[i] + 1));
			tnum = tnum / (range[i] + 1);
		}
		return res;
	}
	
	 void write(int* num, int* range, int length) {
		long tnum = num[0];
		long trange = (range[0] + 1);
		for (int i = 1; i < length && range[i] > 0; i++) {
			tnum = tnum + num[i] * trange;
			trange = trange * (range[i] + 1);
		}
		write (tnum,trange);
	}
	
	 void write(int num, int range) {
		write((long) num, (long) range);
	}
	
	 void write(long num, long range) {
		int bit = 0;
		int mul = 0;
		long rep = 0;
		long irange = range;
		while (range > 0 && irange - rep >= 1 << mul) {
			bit = (char)(num & 1);
			setBit((int)count, 0, bit);
			count ++;
			rep = rep + (bit << mul);
			mul ++;
			num = num >> 1;
			range = range >> 1;
		}
	}
	
	 void fwrite(int num, int bitNum) {
		fwrite((long) num, bitNum);
	}
	
	 void fwrite(long num, int bitNum) {
		while (bitNum > 0) {
			setBit((int)count, 0, (int)num & 1);
			count ++;
			num = num >> 1;
			bitNum--;
		}
	}
	
	 long fread(int bitNum) {
		long res = 0;
		for (int i = 0; i < bitNum; i++) {
			res = res | (readBit() << i);
		}
		return res;
	}
	
	void writeBits(string bits) {
		char *b = (char *)bits.c_str();
		for (unsigned int i = 0; i < bits.size(); i++) {
			if (b[i] == 48) { writeBit(0); } else { writeBit(1); }
		}
	}
	
	 void writeBit(int bit) {
		setBit((int)count,0,bit);
		count ++;
	}
	
	char readBit() {
		count ++;
		return getBit((int)(count - 1),0);
	}
	
	void writeVbit(long num) {
		writeVbit(num, 0);
	}

	void writeVbit(long num, int initbits) {
		int bitNum = initbits;
		long repnum = (1 << bitNum) - 1;
		long precrep = -1;
		while (num > repnum) {
			bitNum ++;
			precrep = repnum;
			repnum = repnum + (1 << bitNum);
		}
		for (int i = initbits; i < bitNum; i++) {
			writeBit(1);
		}
		writeBit(0);
		fwrite(num - precrep - 1, bitNum);
    }
	
	long readVbit(int initBits) {
		int bitNum = initBits;
		long precRep = -1;
		long rep = (1 << initBits) - 1;
		while (readBit() == 1) {bitNum ++; precRep = rep; rep = (rep + (1 << bitNum));}
		return precRep + 1 + fread(bitNum);
    }
	
	long readVbit() {
		return readVbit(0);
	}
	string getString() {
		return getString(0, (int)(count - 1), 0, 0);
	}
	
	string getString(int minx, int maxx, int miny, int maxy) {
		string res = "";
		for (int i2 = maxy; i2 > miny - 1; i2 --) {
			for (int i = minx; i < maxx + 1; i++) {
				res += getBit(i, i2);
			}
			res += "\n";
		}
		return res;
	}
	
	void reset() {
		count = 0;
		wroteBits = 0;
	}
	
	long getSize() {
		return count;
	}
	
	void goTo(long where) {
		count = where;
		wroteBits = where;
	}
	
	char* getMatrix() {
		return matrix;
	}
	
	void setMatrix(char *m) {
		matrix = m;
	}
	
	long getWidth() {
		return width;
	}
	
	long Height() {
		return height;
	}
	
	long getWroteBits() {
		long temp = wroteBits;
		wroteBits = count;
		return count - temp;
	}
	
	long seeWroteBits() {
		return count - wroteBits;
	}
	
	void roundToByte() {
		if (count % 8 != 0) {
			count += 8 - count % 8;
		}
	}
	
	static int log2(int arg) {
		int res = 0;
		while (arg > 1) { arg = arg >> 1; res ++; }
		return res;
	}
};

