#ifndef __PGMIMAGECODEC_H__
#define __PGMIMAGECODEC_H__

#include <pnm.h>
#include "ImageCodec.h"

class PGMImageEncoder : public ImageEncoder {

protected:
  PGMImage *img;
  byte *pixels;
  int idx;

public:

  PGMImageEncoder(char *_filename, int w, int h) : ImageEncoder(_filename, w, h) {
    init();
  }

  ~PGMImageEncoder() {
    delete img;
  }

  void startEncoding() {
    idx = 0;
  }

  void stopEncoding() {
    img->write(filename);
    delete img;
    pixels = NULL;
    img = NULL;
  }

  inline void write(int pixel) {
    pixels[idx++] = (byte)pixel;
  }

protected:
  virtual void init() {
    ASSERT(width != 0, "Image width is 0");
    ASSERT(height != 0, "Image height is 0");
    img = new PGMImage(width, height);
    pixels = img->pixels;
    idx = 0;
  }
};

class PGMImageDecoder: public ImageDecoder {

protected:
  PGMImage *img;
  byte *pixels;
  int idx;

public:

  PGMImageDecoder(char *_filename) : ImageDecoder(_filename) {
    init();
  }

  virtual ~PGMImageDecoder() {
    delete img;
  }

  void startDecoding() {
    idx = 0;
  }

  void stopDecoding() {
    delete img;
    pixels = NULL;
    img = NULL;
  }

  inline int read() {
    return pixels[idx++];
  }

protected:
  virtual void init() {
    img = new PGMImage();
    if (img->read(filename) == -1) {
      fprintf(stderr, "%s: fails to read pgm file.", filename);
      return;
    }
    pixels = img->pixels;
    width = img->width;
    height = img->height;
    idx = 0;
  }
};

#endif

