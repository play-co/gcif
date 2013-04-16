#ifndef __ARITHMIMAGECODEC_H__
#define __ARITHMIMAGECODEC_H__

#include <Arithm.h>
#include "ImageCodec.h"
#include "BinaryModeModel.h"

#define DEFAULT_ADAPTIVE_MODEL
//#undef DEFAULT_ADAPTIVE_MODEL

class ArithmImageEncoder : public ImageEncoder, 
                           public ArithmeticEncoder {

protected:

#ifdef DEFAULT_ADAPTIVE_MODEL
  AdaptiveModel model;
#else
  ContinuousModeModel model;
#endif

public:

  ArithmImageEncoder(char *_filename, int w, int h) : ImageEncoder(_filename, w, h), 
                                                      ArithmeticEncoder() {
    init();
  }

  ~ArithmImageEncoder() {
  }

  void startEncoding() {
    model.init(256);
  }

  void stopEncoding() {
    ArithmeticEncoder::stop();
  }

  void write(int pixel) {
    ArithmeticEncoder::writeSymbol(&model, pixel);
  }

protected:
  virtual void init() {
    ArithmeticEncoder::start(filename);
    ArithmeticEncoder::writeBits(16, width);
    ArithmeticEncoder::writeBits(16, height);
  }
};

class ArithmImageDecoder: public ImageDecoder, 
                          public ArithmeticDecoder {

protected:
#ifdef DEFAULT_ADAPTIVE_MODEL
  AdaptiveModel model;
#else
  ContinuousModeModel model;
#endif

public:

  ArithmImageDecoder(char *_filename) : ImageDecoder(_filename), 
                                        ArithmeticDecoder() {
    init();
  }

  ~ArithmImageDecoder() {
  }

  void startDecoding() {
    model.init(256);
  }

  void stopDecoding() {
    ArithmeticDecoder::stop();
  }

  int read() {
    return ArithmeticDecoder::readSymbol(&model);
  }

protected:
  virtual void init() {
    ArithmeticDecoder::start(filename);
    width = readBits(16);
    height = readBits(16);
  }
};

#endif

