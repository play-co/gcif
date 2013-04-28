/*
 *  bmpWriter.h
 *
 *  Created by Stefano Brocchi and Gabriele Nencini
 *  Version 1.0 beta
 *  License: GPL
 *  Website: http://www.researchandtechnology.net/bcif/
 */

class bmpWriter{
public:
	virtual void setDims(int width, int height)=0;
	virtual void write(char v)=0;
	virtual void writeTriplet(char g,char b,char r)=0;
	virtual void close()=0;
	virtual int getBytesWritten()=0;
	virtual void writeTriplet(char *triplet)=0;
	virtual int getBufferSize()=0;
	virtual int getBufferWritten()=0;
	virtual void flushBuffer()=0;
	virtual void writeHeader()=0;
	virtual int getHeight()=0;
	virtual int getWidth()=0;	
	virtual void setRes(int resx, int resy)=0;
};
