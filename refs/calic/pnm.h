#ifndef __PNM_H__
#define __PNM_H__

#include <stdio.h>
#include <stdlib.h>

typedef unsigned char byte;
typedef unsigned short int16;

// Represent a PPM image
class PPMImage {

public:
  int *pixels;
  int width, height, maxIntensity;

public:
  PPMImage(int, int);
  PPMImage();
  ~PPMImage();

  int read(char *);
  int write(char *);

  int getPixel(int, int);
  void setPixel(int, int, int);
  PPMImage *clone();

  void drawRectangle(int, int, int, int, int);
  void fillRectangle(int, int, int, int, int);
  PPMImage * copyArea(int, int, int, int);
};

// Represent a PGM image
class PGMImage {

public:
  byte *pixels;
  int width, height, maxIntensity;

public:
  PGMImage(int, int);
  PGMImage();
  ~PGMImage();

  int read(char *);
  int write(char *);
  byte getPixel(int, int);
  void setPixel(int, int, byte);
  PGMImage *clone();

  void drawRectangle(int, int, int, int, byte);
  void fillRectangle(int, int, int, int, byte);
  PGMImage * copyArea(int, int, int, int);
  void equalize();
  
};

class ExtPGMImage {

public:
  int16 *pixels;
  int width, height, maxIntensity;

public:
  ExtPGMImage(int, int, int);
  ExtPGMImage();
  ~ExtPGMImage();

  int read(char *, int);
  int write(char *, int);
  int16 getPixel(int, int);
  void setPixel(int, int, int16);
  ExtPGMImage *clone();

  PGMImage *toPGMImage();
  void swapByteOrder();
};

void pnmProcessComment(FILE *);
void pnmProcessUntilEOL(FILE *);
int pnmReadInt(FILE *);
int isWhiteSpace(char);

#endif 
