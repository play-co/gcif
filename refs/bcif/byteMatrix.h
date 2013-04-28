/*
 *  byteMatrix.h
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

class byteMatrix{
private:	
	char *matrix;
	int *myAbs;
	int width;
	int height;
	int depth;
	int dpw;
	int cur;
	int lengthMatrix;
public:
	byteMatrix(int w, int h, int d) {
		width = w;
		height = h;
		depth = d;
		dpw = depth * width;
		lengthMatrix = width * height * depth;
		matrix = (char *) calloc (lengthMatrix , sizeof(char));
		myAbs = (int *) calloc(512, sizeof(int));
		for (int i = 0; i < 512; i++) {
			myAbs[i] = i & 255;
		}
		cur = 0;
	}
	inline char getVal(int w, int h, int d){ return matrix[h * dpw + w * depth + d];}
	inline int getIntVal(int w, int h, int d) {
		return myAbs[matrix[h * dpw + w * depth + d] + 256];
	}
	inline void setVal(int w, int h, int d, int val) {
		matrix[h * dpw + w * depth + d] = (char)val;
	}
	inline void setVal(int w, int h, int d, char val) {
		matrix[h * dpw + w * depth + d] = val;
	}
	inline void sumVal(int w, int h, int d, int val) {
		int insp = h * dpw + w * depth + d;
		int cval = matrix[insp];
		matrix[insp] = (char)(cval + val);
	}
	inline void sumCurVal(int val) {
		matrix[cur] = (char)(matrix[cur] + val);
	}
	inline void sumCurVal(int val, int n) {
		matrix[cur + n] = (char)((int)matrix[cur + n] + val);
	}
	inline void setPoint(int w, int h, int d) {
		cur = h * dpw + w * depth + d;
	}
	inline int getCurVal() {
		return (int)matrix[cur];
	}
	inline char getByteCurVal() {
		return matrix[cur];
	}  
	
	inline char getByteCurVal(int i) {
		return matrix[cur + i];
	}  
	
	inline char getNextByte() {
		return matrix[cur++];
	}
	
	inline int toInt(char b) {
		if (b < 0) { b+=256; }
		return b;
	}
	
	inline char getLowVal() {
		return matrix[cur - dpw];
	}
	
	inline char getLeftVal() {
		return matrix[cur - depth];
	}
	
	inline char getLowLeftVal() {
		return matrix[cur - depth - dpw];
	}
	
	
	inline int getLowLeftVal(bool boolean) {
		return cur;
	}
	
	inline char getRightDownVal() {
		return matrix[cur + depth - dpw];
	}
	
	inline char getUpLeftVal() {
		return matrix[cur - depth + dpw];
	}
	
	inline int getIntCurVal() {
		return myAbs[matrix[cur] + 256];
	}
	
	inline int getCurVal(int n) {
		return (int)matrix[cur + n];
	}
	
	inline void setCurVal(int v)
	{
		matrix[cur] = (char)v;
	}
	
	inline void setCurVal(char v) {
		matrix[cur] = v;
	}
	
	inline void setCurVal(char v, int n) {
		matrix[cur + n] = v;
	}
	
	inline void nextVal() {
		cur ++;
	}
	
	inline void precVal() {
		cur --;
	}
	
	inline void nextVal(int i) {
		cur += i;
	}
	
	inline void precVal(int i) {
		cur -= i;
	}
	
	inline void firstVal() {
		cur = 0;
	}
	
	inline void lastVal() {
		cur = lengthMatrix - 1;
	}
	
	inline int getWidth() {
		return width;
	}
	
	inline int getHeight() {
		return height;
	}
	
	inline int getDepth() {
		return depth;
	}
	inline void printMatrix() {
		for(int i=0; i<lengthMatrix; i++) {
			cout << i << ":" << (int)matrix[i] << "\t";
			if(i > 0 && i % 1000 == 0) cout << endl;
		}
	}
};
