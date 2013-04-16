/***************************************************************************

  This image codec is implemented mainly based on the following paper:

    X. Wu and N.D. Memon. "Context-Based, Adaptive, Lossless Image Coding."
    IEEE TRans. on Comm. 45(4):437-444, April 1997.

  The codec implemented here is not optimized for speed and memory usage.

  Written by Cheng Jiun Yuan
  University of Kentucky

  Oct 20th, 1998

****************************************************************************/

#ifndef __CALIC_H__
#define __CALIC_H__

#include "Arithm.h"
#include "BinaryModeModel.h"
#include "ImageCodec.h"
#include "PGMImageCodec.h"
#include "StatisticalModel.h"

#define DEFAULT_SYMBOL_COUNT	256

// CALIC specific codec model
class CALICEntropyModel: public ImageCodecModel {
public:
  // Adaptive arithmetic coding models for entropy coding of prediction errors.

  // Continuous mode
  int continuousModelCount;		// # of models used in continuous mode
  int *continuousModelSize;		// # of symbols accepted by each model

#define DEFAULT_ADAPTIVE_MODEL
#ifdef DEFAULT_ADAPTIVE_MODEL
  AdaptiveModel *continuousModel;	// Adaptive arithmetic coding models
#else
  ContinuousModeModel *continuousModel;	// Adaptive arithmetic coding models
#endif

  // Binary mode
  int ternaryModelCount;                 // # of models used in binary mode
//  AdaptiveModel *ternaryModel;          // Adaptive arithmetic coding models
  BinaryModeModel *ternaryModel;          // Adaptive arithmetic coding models

public:

  CALICEntropyModel();

  virtual ~CALICEntropyModel() {
    delete [] ternaryModel;
    delete [] continuousModelSize;
    delete [] continuousModel;
  }

  CALICEntropyModel *clone();
  void copy(CALICEntropyModel *model);

  virtual int write(FILE *fp);
  virtual int read(FILE *fp);

  void reset();

};

class CALICImageEncoder : public ImageEncoder, 
                          public ArithmeticEncoder {

private:
  int context;
  bool binaryMode;

protected:

  CALICEntropyModel *model;

public:

  CALICImageEncoder(char *_filename, int w, int h, int ns) : 
    ImageEncoder(_filename, w, h), 
    ArithmeticEncoder() {

    model = NULL;
    init();
  }

  ~CALICImageEncoder() {
  }

  inline void setBinaryMode(bool _binaryMode) { binaryMode = _binaryMode; }
  inline void setContext(int _context) { context = _context; }

  virtual void write(int symbol) {
    if (binaryMode)
      writeBinaryModeSymbol(context, symbol);
    else
      writeContinuousModeSymbol(context, symbol);

    //ASSERT(false, "write(int) not supported; use other write methods instead.");
  }

  void writeBinaryModeSymbol(int context, int symbol) {
    ArithmeticEncoder::writeSymbol(&(model->ternaryModel[context]), symbol);
  }

  void writeContinuousModeSymbol(int modelIndex, int symbol) {
    int escaped = model->continuousModelSize[modelIndex] - 1;

    if (symbol < escaped)
      ArithmeticEncoder::writeSymbol(&(model->continuousModel[modelIndex]), symbol);
    else {
      ArithmeticEncoder::writeSymbol(&(model->continuousModel[modelIndex]), escaped);

      if (modelIndex + 1 < model->continuousModelCount)
        writeContinuousModeSymbol(modelIndex + 1, symbol - escaped);
    }
  }

  void setEntropyModel(CALICEntropyModel *_model) { model = _model; }
  CALICEntropyModel *getEntropyModel() { return model; };

protected:

  virtual void startEncoding() {
  }

  virtual void stopEncoding() {
    ArithmeticEncoder::stop();
  }

  void init();
};

class CALICImageDecoder : public ImageDecoder, 
                          public ArithmeticDecoder {
private:
  bool binaryMode;

protected:
  int context;
  CALICEntropyModel *model;

public:

  CALICImageDecoder(char *_filename, int ns) :
    ImageDecoder(_filename), 
    ArithmeticDecoder() {

    model = NULL;
    init();
  }

  virtual ~CALICImageDecoder() {
  }

  inline void setBinaryMode(bool _binaryMode) { binaryMode = _binaryMode; }
  inline void setContext(int _context) { context = _context; }

  virtual int read() {
    if (binaryMode)
      return readBinaryModeSymbol(context);
    else
      return readContinuousModeSymbol(context);

    // ASSERT(false, "read() not supported; use other read methods instead.");
  }

  int readBinaryModeSymbol(int context) {
     return ArithmeticDecoder::readSymbol(&(model->ternaryModel[context]));
  }

  int readContinuousModeSymbol(int modelIndex) {
    int symbol = ArithmeticDecoder::readSymbol(&(model->continuousModel[modelIndex]));
    int escaped = model->continuousModelSize[modelIndex] - 1;

    if (symbol < escaped || (modelIndex + 1) == model->continuousModelCount)
      return symbol;

    return escaped + readContinuousModeSymbol(modelIndex + 1);
  }

  void setEntropyModel(CALICEntropyModel *_model) { model = _model; }
  CALICEntropyModel *getEntropyModel() { return model; };

protected:

  virtual void startDecoding() { 
  }

  virtual void stopDecoding() {
    ArithmeticDecoder::stop();
  }

  void init();
};

class CALICImageCodec : public ImageCodec {
protected:

  PGMImageEncoder *pgmEnc;		// Arithmetic encoder
  PGMImageDecoder *pgmDec;		// Arithmetic decoder

  CALICImageEncoder *calicEnc;		// Arithmetic encoder
  CALICImageDecoder *calicDec;		// Arithmetic decoder

  StatisticalModel *errorModel;
  CALICEntropyModel *entropyModel;

/*
  int s1[6], s2[6];		// Only used in binary mode.
				// Used temporarily to detect if the surrounding pixels have
				// only two different values.
				// After the detection. s1[0] keeps the west pixel value and
				// s2[0] keeps the other value if there is any.
				// (See pp. 442, eqn. 10, 11, 12)
*/

  bool binaryModeEnabled;	// If true, allow binary mode.
  bool resetModel;

public:

  int binaryModeCount;		// Keep the number of pixels that result in binary mode.
  int regularModeCount; 
  int escapedBinaryModeCount;

  CALICImageCodec() : ImageCodec() {
    binaryModeEnabled = true;
    binaryModeCount = regularModeCount = escapedBinaryModeCount = 0;
    errorModel = new StatisticalModel(4 * 256 + 1, 128);
    entropyModel = new CALICEntropyModel();
    resetModel = true;

//    hist = new Histogram(0, 255);
  }

  virtual ~CALICImageCodec() {
    delete errorModel;
  }

  void setBinaryModeEnabled(bool _mode) { binaryModeEnabled = _mode; }
  bool isBinaryModeEnabled() { return binaryModeEnabled; }
 
  virtual void printStat(void);

  virtual int writeImageCodecModel(char *filename);
  virtual int readImageCodecModel(char *filename);

  inline void setResetModel(bool _resetModel) { resetModel =  _resetModel; }

protected:

  virtual void encInit();
  virtual void decInit();

  virtual int predict_0_0();
  virtual void encode_0_0();
  virtual void decode_0_0();

  virtual int predict_1_0();
  virtual void encode_1_0() { encode_border(1, 0); }
  virtual void decode_1_0() { decode_border(1, 0); }

  virtual int predict_x_0(int x);
  virtual void encode_x_0(int x) { encode_border(x, 0); }
  virtual void decode_x_0(int x) { decode_border(x, 0); }

  virtual int predict_0_1();
  virtual void encode_0_1() { encode_border(0, 1); }
  virtual void decode_0_1() { decode_border(0, 1); }

  virtual int predict_1_1();
  virtual void encode_1_1() { encode_border(1, 1); }
  virtual void decode_1_1() { decode_border(1, 1); }

  virtual int predict_x_1(int x);
  virtual void encode_x_1(int x) { encode_border(x, 1); }
  virtual void decode_x_1(int x) { decode_border(x, 1); }

  virtual int predict_2ndLast_1();
  virtual void encode_2ndLast_1() { encode_border(dec->getWidth()-2, 1); }
  virtual void decode_2ndLast_1() { decode_border(dec->getWidth()-2, 1); }

  virtual int predict_Last_1();
  virtual void encode_Last_1() { encode_border(dec->getWidth()-1, 1); }
  virtual void decode_Last_1() { decode_border(dec->getWidth()-1, 1); }

  virtual int predict_0_y(int y);
  virtual void encode_0_y(int y) { encode_border(0, y); }
  virtual void decode_0_y(int y) { decode_border(0, y); }

  virtual int predict_1_y(int y);
  virtual void encode_1_y(int y) { encode_border(1, y); }
  virtual void decode_1_y(int y) { decode_border(1, y); }

  virtual int predict_x_y(int x, int y) { return 0; }
  virtual void encode_x_y(int x, int y);
  virtual void decode_x_y(int x, int y);

  virtual int predict_Last_y(int y);
  virtual void encode_Last_y(int y) { encode_border(dec->getWidth() - 1, y); }
  virtual void decode_Last_y(int y) { decode_border(dec->getWidth() - 1, y); }

  int predict_LOCO(int x, int y);

  virtual void encode_border(int x, int y);
  virtual void decode_border(int x, int y);

  virtual bool encBinaryMode(int x, int y);
  virtual bool decBinaryMode(int x, int y);

  virtual void encContinuousMode(int x, int y);
  virtual void decContinuousMode(int x, int y);

protected:
  virtual int computeBinaryPattern(ByteImage *img, int x, int y, int *s1, int *s2);

  virtual void gradientAdjustedPrediction(int x, int y, int *dh, int *dv, int *predicted);

  virtual int quantizePattern(int x, int y, int predicted);
  virtual int computeEnergy(int x, int y, int dh, int dv);
  virtual int quantizeEnergy(int energy);

};

#endif
