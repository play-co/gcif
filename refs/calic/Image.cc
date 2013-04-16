#include "Image.h"

#include <pnm.h>

ByteImage *readPGMImage(char *filename) {

  PGMImage pgmImg;

  if (pgmImg.read(filename) == -1) {
    fprintf(stderr, "Error: fails to read image from %s.\n", filename);
    return NULL;
  }

  ByteImage *img = new ByteImage (pgmImg.pixels, 
                                      pgmImg.width, 
                                      pgmImg.height);

  pgmImg.pixels = NULL;

  return img; 
}

int writePGMImage(char *filename, ByteImage *img) {

  if (img == NULL) {
    fprintf(stderr, "writePGMImage(): image is NULL.\n");
    return -1;
  }

  PGMImage *pgmImg = new PGMImage();
  pgmImg->pixels = img->pixels;
  pgmImg->width = img->w;
  pgmImg->height = img->h;
  pgmImg->maxIntensity = 255;

  if (pgmImg->write(filename) == -1) {
    fprintf(stderr, "Error: fails to write image to %s.\n", filename);
    pgmImg->pixels = NULL;
    delete pgmImg;
    return -1; 
  }

  pgmImg->pixels = NULL;
  delete pgmImg;
  return 0;
}

/*
ShortImage *readShortImage(char *filename) {
  fprintf(stderr, "readShortImage has not yet been implemented.\n");
  return NULL;
}
*/
