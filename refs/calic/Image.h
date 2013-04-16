#ifndef __IMAGE_H__
#define __IMAGE_H__

#include <pnm.h>

class ByteImage {
public:
  byte *pixels;
  int w, h;
  bool deleteFlag;

  ByteImage(byte *_pixels, int _w, int _h):pixels(_pixels), w(_w), h(_h) {
    deleteFlag = true; 
  }

  ~ByteImage() {
    if (deleteFlag)
      delete [] pixels; 
  }

  inline int getPixel(int x, int y) { return pixels[y * w + x]; }
  inline void setPixel(int x, int y, int pixel) { pixels[y * w + x] = (byte)pixel; }

  inline int W(int x, int y) { return pixels[y * w + x - 1]; }
  inline int N(int x, int y) { return pixels[(y - 1) * w + x]; }
  inline int NE(int x, int y) { return pixels[(y - 1) * w + x + 1]; }
  inline int NW(int x, int y) { return pixels[(y - 1) * w + x - 1]; }
  inline int WW(int x, int y) { return pixels[y * w + x - 2]; }
  inline int NN(int x, int y) { return pixels[(y  - 2) * w + x]; }
  inline int NNE(int x, int y) { return pixels[(y - 2) * w + x + 1]; }
  inline int NNW(int x, int y) { return pixels[(y - 2) * w + x - 1]; }
  inline int NWW(int x, int y) { return pixels[(y - 2) * w + x - 2]; }
};

class ShortImage {
public:
  short *pixels;
  int w, h;
  bool deleteFlag;

  ShortImage(short *_pixels, int _w, int _h):pixels(_pixels), w(_w), h(_h) {
    deleteFlag = true;
  }

  ~ShortImage() {
    if (deleteFlag)
      delete [] pixels;
  }

  inline int getPixel(int x, int y) { return pixels[y * w + x]; }
  inline void setPixel(int x, int y, int pixel) { pixels[y * w + x] = (short)pixel; }

  inline int W(int x, int y) { return pixels[y * w + x - 1]; }
  inline int N(int x, int y) { return pixels[(y - 1) * w + x]; }
  inline int NE(int x, int y) { return pixels[(y - 1) * w + x + 1]; }
  inline int NW(int x, int y) { return pixels[(y - 1) * w + x - 1]; }
  inline int WW(int x, int y) { return pixels[y * w + x - 2]; }
  inline int NN(int x, int y) { return pixels[(y  - 2) * w + x]; }
  inline int NNE(int x, int y) { return pixels[(y - 2) * w + x + 1]; }
  inline int NNW(int x, int y) { return pixels[(y - 2) * w + x - 1]; }
  inline int NWW(int x, int y) { return pixels[(y - 2) * w + x - 2]; }
};

/*
class BufferedByteImage {
public:
  int bufferedRow;
  byte *pixels;
  int w, h;
  bool deleteFlag;

  BufferedByteImage(byte *_pixels, int _w, int _h, int _bufferedRow):
    pixels(_pixels), w(_w), h(_h), bufferedRow(_bufferedRow) {
    deleteFlag = true; 
  }

  ~BufferedByteImage() {
    if (deleteFlag)
      delete [] pixels; 
  }

  inline int getPixel(int x, int y) { return pixels[(y % bufferedRow) * w + x]; }
  inline void setPixel(int x, int y, int pixel) {
    pixels[(y % bufferedRow) * w + x] = (byte)pixel;
  }

  inline int W(int x, int y) { return pixels[(y % bufferedRow)* w + x - 1]; }
  inline int N(int x, int y) { return pixels[((y - 1 ) % bufferedRow) * w + x]; }
  inline int NE(int x, int y) { return pixels[((y - 1 ) % bufferedRow) * w + x + 1]; }
  inline int NW(int x, int y) { return pixels[((y - 1 ) % bufferedRow) * w + x - 1]; }
  inline int WW(int x, int y) { return pixels[(y % bufferedRow) * w + x - 2]; }
  inline int NN(int x, int y) { return pixels[((y - 2 ) % bufferedRow) * w + x]; }
  inline int NNE(int x, int y) { return pixels[((y - 2 ) % bufferedRow) * w + x + 1]; }
  inline int NNW(int x, int y) { return pixels[((y - 2 ) % bufferedRow) * w + x - 1]; }
  inline int NWW(int x, int y) { return pixels[((y - 2 ) % bufferedRow) * w + x - 2]; }
};

class BufferedShortImage {
public:
  int bufferedRow;
  short *pixels;
  int w, h;
  bool deleteFlag;

  BufferedShortImage(short *_pixels, int _w, int _h, int _bufferedRow): 
    pixels(_pixels), w(_w), h(_h), bufferedRow(_bufferedRow) {
    deleteFlag = true; 
  }

  ~BufferedShortImage() {
    if (deleteFlag)
      delete [] pixels;
  }

  inline int getPixel(int x, int y) { return pixels[(y % bufferedRow) * w + x]; }
  inline void setPixel(int x, int y, int pixel) { 
    pixels[(y % bufferedRow) * w + x] = (short)pixel; 
  }

  inline int W(int x, int y) { return pixels[(y % bufferedRow)* w + x - 1]; }
  inline int N(int x, int y) { return pixels[((y - 1 ) % bufferedRow) * w + x]; }
  inline int NE(int x, int y) { return pixels[((y - 1 ) % bufferedRow) * w + x + 1]; }
  inline int NW(int x, int y) { return pixels[((y - 1 ) % bufferedRow) * w + x - 1]; }
  inline int WW(int x, int y) { return pixels[(y % bufferedRow) * w + x - 2]; }
  inline int NN(int x, int y) { return pixels[((y - 2 ) % bufferedRow) * w + x]; }
  inline int NNE(int x, int y) { return pixels[((y - 2 ) % bufferedRow) * w + x + 1]; }
  inline int NNW(int x, int y) { return pixels[((y - 2 ) % bufferedRow) * w + x - 1]; }
  inline int NWW(int x, int y) { return pixels[((y - 2 ) % bufferedRow) * w + x - 2]; }
};

*/

ByteImage *readPGMImage(char *filename);
int writePGMImage(char *filename, ByteImage *img);

#endif
