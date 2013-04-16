// Programmer: CJ Yuan
// Implementation of routines for manipulating PPM and PGM image.
//

#include "pnm.h"

char _ch;		// temp variable

PPMImage::PPMImage(int w, int h) {
  pixels = new int [w * h];
  width = w; height = h;
  maxIntensity = 255;
}

PPMImage::PPMImage() {
  pixels = NULL;
  width = 0; height = 0;
  maxIntensity = 0;
}

PPMImage::~PPMImage() {
  delete [] pixels;
}

// Read a PPM image (for binary format only)
int PPMImage::read(char *filename) {
 
  FILE *fp;
  if ((fp = fopen(filename, "r+b")) == NULL) {
    perror(filename);
    return -1;
  }

  char header[2];
  header[0] = fgetc(fp); header[1] = fgetc(fp);
  if (header[0] != 'P' || header[1] != '6') {
    fprintf(stderr, "%s: not a P6 PPM file.\n", filename);
    return -1;
  }
//  pnmProcessComment(fp);
  if ((width = pnmReadInt(fp)) == 0 || (height = pnmReadInt(fp)) == 0) {
    fprintf(stderr, "%s: Warning! Image dimension is 0.\n", filename);
    pixels = NULL;
    return 0;
  }

  if ((maxIntensity = pnmReadInt(fp)) == 0) {
    fprintf(stderr, "%s: Warning! Largest intensity value is 0.\n", filename);
    pixels = NULL;
    return 0;
  }
  pnmProcessUntilEOL(fp);

  pixels = new int [width * height];

  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      if (fread(&pixels[i*width + j], sizeof(char), 3, fp) != 3) {
        fprintf(stderr, "%s: short file.\n", filename);
        return -1;
      }
      pixels[i*width + j] >>= 8;
    }
  }

  fclose(fp);
  return 0;
}

int PPMImage::getPixel(int x, int y) {
  if (x >= width || x < 0)
    return -1;
  if (y >= height || y < 0)
    return -1;

  if (pixels != NULL)
    return  pixels[y * width + x];
  else
    return -1;
}

// pixel: the least significat 24 bits represent the R,G,B value.
void PPMImage::setPixel(int x, int y, int pixel) {
  if (x >= width || x < 0)
    return;
  if (y >= height || y < 0)
    return;

  if (pixels != NULL) 
    pixels[y * width + x] = pixel;
}

// Write the image to a PPM file
int PPMImage::write(char *filename) {
  FILE *fp;
  int tmp;

  if ((fp = fopen(filename, "wb")) == NULL) {
    perror(filename);
    return -1;
  }

  fprintf(fp, "P6\n%d %d\n%d\n", width, height, maxIntensity);

  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      tmp = pixels[i*width + j] << 8;
      fwrite(&tmp, sizeof(char), 3, fp);
    }
  }

  fclose(fp);
  return 0;
}

// Local routines: Implemented according to the PNM format.
// This routine is used to get rid of white space.
void pnmProcessUntilEOL(FILE *fp) {

  while(!feof(fp)) {
    _ch = fgetc(fp);
    if (_ch == '\n')
      break;
  }
}

// Local routines: Implemented according to the PNM format.
// This routine is used to get rid of comment.
void pnmProcessComment(FILE *fp) {

  while (!feof(fp)) {
    _ch = fgetc(fp);
    if (_ch != '#') {
      ungetc(_ch, fp);
      return;
    }
    pnmProcessUntilEOL(fp);
  }
}

// Draw a rectangle in the current image.
// x,y: upper left corner of the rectangle.
// w,h: dimension of the rectangle.
// color: rectangle border color.
void PPMImage::drawRectangle(int loc_x, int loc_y, int w, int h, int color) {

  register int i, j;

  if (loc_x + w > width)
    w = width - loc_x;
  if (loc_y + h > height)
    h = height - loc_y;

  for (i = 0; i < h; i++) {
    if (i == 0 || i == (h-1)) {
      for (j = 0; j < w; j++)
        setPixel(loc_x + j, loc_y + i, color);
    }
    else {
      setPixel(loc_x, loc_y + i, color);
      setPixel(loc_x + w - 1, loc_y + i, color);
    }
  }
}

void PPMImage::fillRectangle(int loc_x, int loc_y, int w, int h, int color) {

  register int i, j;

  if (loc_x + w > width)
    w = width - loc_x;
  if (loc_y + h > height)
    h = height - loc_y;

  for (i = 0; i < h; i++) 
    for (j = 0; j < w; j++)
       setPixel(loc_x + j, loc_y + i, color);
}

// Copy the pixels from a rectangular region.
PPMImage *PPMImage::copyArea(int x, int y, int w, int h) {

  PPMImage *newimg = new PPMImage(w, h);
  register int i, j, imgsize;

  imgsize = w * h;
  for (i = 0; i < imgsize; i++) 
    newimg->pixels[i] = 0;

  int max_w, max_h;
  if (x + w > width)
    max_w = width - x;
  else
    max_w = w;

  if (y + h >= height)
    max_h = height - y;
  else
    max_h = h;
 
  for (i = 0; i < max_h; i++)
    for (j = 0; j < max_w; j++) 
      newimg->setPixel(j, i, getPixel(j+x, i+y));

  return newimg;
}

PGMImage::PGMImage(int w, int h) {
  pixels = new byte [w * h];
  width = w; height = h;
  maxIntensity = 255;
}

PGMImage::PGMImage() {
  pixels = NULL;
  width = 0; height = 0;
  maxIntensity = 0;
}

PGMImage::~PGMImage() {
  delete [] pixels;
}

int PGMImage::read(char *filename) {
 
  FILE *fp;
  if ((fp = fopen(filename, "r+b")) == NULL) {
    perror(filename);
    return -1;
  }

  char header[2];
  header[0] = fgetc(fp); header[1] = fgetc(fp);
  if (header[0] != 'P' || header[1] != '5') {
    fprintf(stderr, "%s: not a P5 PGM file.\n", filename);
    return -1;
  }
  pnmProcessComment(fp);
  if ((width = pnmReadInt(fp)) == 0 || (height = pnmReadInt(fp)) == 0) {
    fprintf(stderr, "%s: Warning! Image dimension is 0.\n", filename);
    pixels = NULL;
    return 0;
  }

  if ((maxIntensity = pnmReadInt(fp)) == 0) {
    fprintf(stderr, "%s: Warning! Largest intensity value is 0.\n", filename);
    pixels = NULL;
    return 0;
  }

  pnmProcessUntilEOL(fp);

  pixels = new byte [width * height];
  if (fread(pixels, sizeof(byte), width * height, fp) != (unsigned int)(width * height)) {
    fprintf(stderr, "%s: short file.\n", filename);
    return -1;
  }

  fclose(fp);
  return 0;
}

byte PGMImage::getPixel(int x, int y) {
  if (x >= width || x < 0)
    return -1;
  if (y >= height || y < 0)
    return -1;

  if (pixels != NULL)
    return  pixels[y * width + x];
  else
    return -1;
}

void PGMImage::setPixel(int x, int y, byte pixel) {
  if (x >= width || x < 0)
    return;
  if (y >= height || y < 0)
    return;

  if (pixels != NULL) 
    pixels[y * width + x] = pixel;
}

int PGMImage::write(char *filename) {
  FILE *fp;

  if ((fp = fopen(filename, "wb")) == NULL) {
    perror(filename);
    return -1;
  }

  fprintf(fp, "P5\n%d %d\n%d\n", width, height, maxIntensity);

  fwrite(pixels, sizeof(byte), width*height, fp);

  fclose(fp);
  return 0;
}

int pnmReadInt(FILE *fp) {
  int retValue = 0;
  int startFlag = 0;

  while (!feof(fp)) {
    _ch = fgetc(fp);
    if (_ch == '#') {
      ungetc(_ch, fp);
      pnmProcessComment(fp);
      continue;
    }
    if (_ch >= '0' && _ch <= '9') {
      startFlag = 1;
      retValue = retValue * 10 + (_ch - '0');
    }
    else {
      if (startFlag == 1) {
        ungetc(_ch, fp);
        return retValue;
      }
    }
  }
  return 0;
}

// Draw a rectangle in the current image.
// x,y: upper left corner of the rectangle.
// w,h: dimension of the rectangle.
// color: rectangle border color.
void PGMImage::drawRectangle(int loc_x, int loc_y, int w, int h, byte color) {

  register int i, j;

  if (loc_x + w > width)
    w = width - loc_x;
  if (loc_y + h > height)
    h = height - loc_y;

  for (i = 0; i < h; i++) {
    if (i == 0 || i == (h-1)) {
      for (j = 0; j < w; j++)
        setPixel(loc_x + j, loc_y + i, color);
    }
    else {
      setPixel(loc_x, loc_y + i, color);
      setPixel(loc_x + w - 1, loc_y + i, color);
    }
  }
}

void PGMImage::fillRectangle(int loc_x, int loc_y, int w, int h, byte color) {

  register int i, j;

  if (loc_x + w > width)
    w = width - loc_x;
  if (loc_y + h > height)
    h = height - loc_y;

  for (i = 0; i < h; i++) 
    for (j = 0; j < w; j++)
       setPixel(loc_x + j, loc_y + i, color);
}

PGMImage *PGMImage::copyArea(int x, int y, int w, int h) {

  PGMImage *newimg = new PGMImage(w, h);
  register int i, j, imgsize;

  imgsize = w * h;
  for (i = 0; i < imgsize; i++) 
    newimg->pixels[i] = 0;

  int max_w, max_h;
  if (x + w > width)
    max_w = width - x;
  else
    max_w = w;

  if (y + h >= height)
    max_h = height - y;
  else
    max_h = h;
 
  for (i = 0; i < max_h; i++)
    for (j = 0; j < max_w; j++) 
      newimg->setPixel(j, i, getPixel(j+x, i+y));

  return newimg;
}

// equalize the histogram of this image
void PGMImage::equalize() {

  register int *histogram = new int [256];
  register int i, imgsize = width * height;

  for (i = 0; i < 256; i++)
    histogram[i] = 0;

  for (i = 0; i < imgsize; i++) 
    histogram[pixels[i]]++;

  register int acc = 0;
  for (i = 0; i < 256; i++) {
    acc += histogram[i]; 
    histogram[i] = (acc * 255) / imgsize;
  }

  for (i = 0; i < imgsize; i++)
    pixels[i] = histogram[pixels[i]];
  
  delete [] histogram;
}

// Make a copy of this image
PGMImage * PGMImage::clone() {

  PGMImage *newimg = new PGMImage(width, height);
  newimg->width = width;
  newimg->height = height;
  newimg->maxIntensity = maxIntensity;

  int imgsize = width * height;
  for (int i=0; i < imgsize; i++)
    newimg->pixels[i] = pixels[i];

  return newimg;
}

// Make a copy of this image
PPMImage * PPMImage::clone() {

  PPMImage *newimg = new PPMImage(width, height);
  newimg->width = width;
  newimg->height = height;
  newimg->maxIntensity = maxIntensity;

  int imgsize = width * height;
  for (int i=0; i < imgsize; i++)
    newimg->pixels[i] = pixels[i];

  return newimg;
}

