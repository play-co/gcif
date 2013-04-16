/***********************************************************************
***********************************************************************/

#ifndef __IMAGECODEC_H__
#define __IMAGECODEC_H__

#include <pnm.h>
#include "Arithm.h"
#include "Image.h"

#define DEFAULT_SIGNATURE 	"cj08"
#define DEFAULT_SYMBOL_COUNT	256

#undef ASSERT
#define ASSERT(flag, msg)   if (!(flag)) {\
                              fprintf(stderr, "%s (%d): %s\n",  __FILE__, __LINE__, (msg));\
                              exit(1);\
                            }


/*
   Image codec model is a data structure used by the image codec to describe
   image property. The description (side-information) might or might not
   need by the codec in the future.

   This abstract super class makes sure the subclass can save and restore
   the 'description'. 
*/
class ImageCodecModel {

public:
  virtual int write(char *filename) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
      perror(filename);
      return -1;
    }
    int ret = write(fp);
    fclose(fp); 
    return ret;
  }

  virtual int read(char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
      perror(filename);
      return -1;
    }
    int ret = read(fp);
    fclose(fp); 
    return ret;
  }

  virtual int write(FILE *fp) = 0;
  virtual int read(FILE *fp) = 0;
};

// Base class of all the encoders.
// This class simply encode one symbol at a time.
class ImageEncoder {

protected:

  int width, height;		// Image width and height
  char *filename;		// Output filename

public:

  ImageEncoder(char *_filename, int w, int h) {
    ASSERT(_filename != NULL, "NULL pointer!");

    filename = new char [strlen(_filename) + 1];
    strcpy(filename, _filename); 

    width = w;
    height = h;
  }

  virtual ~ImageEncoder() { delete [] filename; }

  char *getFilename() { return filename; }
  int getWidth() { return width; }
  int getHeight() { return height; }

  virtual void startEncoding() = 0;	// Initialize encoder (open file, etc.)
  virtual void stopEncoding() = 0;	// Finalize encoder (close file, etc.)
  virtual void write(int symbol) = 0;	// Encode a symbol/pixel
};

// Base class of all the decoder.
// This class simply decode one symbol at a time.
class ImageDecoder {

protected:

  int width, height;		// Image width and height
  char *filename;		// Input filename

public:

  ImageDecoder(char *_filename) {
    ASSERT(_filename != NULL, "NULL pointer!");

    filename = new char [strlen(_filename) + 1];
    strcpy(filename, _filename);
  }

  virtual ~ImageDecoder() { delete [] filename; }

  char *getFilename() { return filename; }
  int getWidth() { return width; }
  int getHeight() { return height; }

  virtual void startDecoding() = 0;	// Initialize decoder (open file, etc.)
  virtual void stopDecoding() = 0;	// Finalize decoder (close file, etc.)
  virtual int read() = 0;		// Read and return the next symbol/pixel.
};

// Base class for all the lossless Image Compression/Decompressor
class ImageCodec {

protected:

  ImageEncoder *enc;		// Image encoder
  ImageDecoder *dec;		// Image decoder

  char *inFilename;		// Source (input) image filename
  char *outFilename;		// Target (output) filename

  bool debug;			// If true, print debugging messages.
  bool verbose;			// If true, print stat.

  char *refFilename;		// Reference image filename (if there is any).

public:

  ByteImage *srcImg;		// Buffer to keep the pixels
  ShortImage *errImg;		// Buffer to keep the prediction errors
  ByteImage *refImg;		// Reference image
 
  int symbolCount;		// # of symbols in the source image.

public:
  ImageCodec();
  virtual ~ImageCodec();

  void setInputImageFilename(char *filename) {
    ASSERT(filename != NULL, "NULL pointer!");

    delete [] inFilename;
    inFilename = new char [strlen(filename) + 1];
    strcpy(inFilename, filename);
  }

  void setOutputImageFilename(char *filename) {
    ASSERT(filename != NULL, "NULL pointer!");

    delete [] outFilename;
    outFilename = new char [strlen(filename) + 1];
    strcpy(outFilename, filename);
  }

  void setReferenceImageFilename(char *filename) {
    ASSERT(filename != NULL, "NULL pointer!");

    delete [] refFilename;
    refFilename = new char [strlen(filename) + 1];
    strcpy(refFilename, filename);
  }

  virtual int encode();		// Perform image encoding
  virtual int decode();		// Perform image decoding 

  void setDebug(bool _debug) { debug = _debug; }
  bool isDebug() { return debug; }

  void setVerbose(bool _verbose) { verbose = _verbose; }
  bool isVerbose() { return verbose; }

  virtual void printStat(void) {}	// Print the information collected 
					// during encoding/decoding process

  ShortImage * getErrorImage() { return errImg; }
  ByteImage * getSourceImage() { return srcImg; }
  ByteImage * getReferenceImage() { return refImg; }

  // Codec specific.
  // Write/read the encoding/decoding model(s) associated with 
  // current image
  virtual int writeImageCodecModel(char *filename) { 
    fprintf(stderr, "Subclass needs to implement writeImageCodecModel(char *)\n"); 
    return -1;
  }

  virtual int readImageCodecModel(char *filename) { 
    fprintf(stderr, "Subclass needs to implement readImageCodecModel(char *)\n"); 
    return -1;
  }

private:
  int predicted;

protected:

  void readReferenceImage();
  
  inline int getPredicted() { return predicted; }
  inline void setPredicted(int _predicted) { predicted = ensureBounds(_predicted); }
   

  // Subclass need to implement these two methods to 
  // initialize encoding/decoding process.
  // (Initialize encoder/decoder, initialize codec specific models, etc.)
  virtual void encInit() {};
  virtual void decInit() {};

  virtual void encFinalize() {};
  virtual void decFinalize() {};

  // start the actual encoding/decoding process.
  // Subclass probably does not need to overwrite this method.
  virtual void startEncode();
  virtual void startDecode();


  // --------- Treat first row of pixels in special way. ---------
  virtual int predict_0_0() { return predict_x_0(0); }
  virtual void encode_0_0() { encode_x_0(0); }
  virtual void decode_0_0() { encode_x_0(0); }

  virtual int predict_1_0() { return predict_x_0(1); }
  virtual void encode_1_0() { encode_x_0(1); }
  virtual void decode_1_0() { encode_x_0(1); }

  virtual int predict_x_0(int x) { return predict_x_y(x, 0); }
  virtual void encode_x_0(int x) { encode_x_y(x, 0); }
  virtual void decode_x_0(int x) { encode_x_y(x, 0); }

  // --------- Treat second row of pixels in special way. ---------
  virtual int predict_0_1() { return predict_x_1(0); }
  virtual void encode_0_1() { encode_x_1(0); }
  virtual void decode_0_1() { decode_x_1(0); }

  virtual int predict_1_1() { return predict_x_1(1); }
  virtual void encode_1_1() { encode_x_1(1); }
  virtual void decode_1_1() { decode_x_1(1); }

  virtual int predict_x_1(int x) { return predict_x_y(x, 1); }
  virtual void encode_x_1(int x) { encode_x_y(x, 1); }
  virtual void decode_x_1(int x) { decode_x_y(x, 1); }

  virtual int predict_2ndLast_1() { return predict_x_1(dec->getWidth() - 2); }
  virtual void encode_2ndLast_1() { encode_x_1(dec->getWidth() - 2); }
  virtual void decode_2ndLast_1() { decode_x_1(dec->getWidth() - 2); }

  virtual int predict_Last_1() { return predict_x_1(dec->getWidth() - 1); }
  virtual void encode_Last_1() { encode_x_1(dec->getWidth() - 1); }
  virtual void decode_Last_1() { decode_x_1(dec->getWidth() - 1); }


  // --------- Treat first column of pixels in special way. --------------
  virtual int predict_0_y(int y) { return predict_x_y(0, y); }
  virtual void encode_0_y(int y) { encode_x_y(0, y); }
  virtual void decode_0_y(int y) { decode_x_y(0, y); }


  // --------- Treat second column of pixels in special way. --------------
  virtual int predict_1_y(int y) { return predict_x_y(1, y); }
  virtual void encode_1_y(int y) { encode_x_y(1, y); }
  virtual void decode_1_y(int y) { decode_x_y(1, y); }

  /* 
     --------- Treatment of all other pixels -------------
     Subclass must implement these!
  */ 
  virtual int predict_x_y(int x, int y) = 0;
  virtual void encode_x_y(int x, int y) = 0;
  virtual void decode_x_y(int x, int y) = 0;

  // --------- Treat second last column of pixels in special way. --------------
  virtual int predict_2ndLast_y(int y) { return predict_x_y(dec->getWidth() - 2, y); }
  virtual void encode_2ndLast_y(int y) { encode_x_y(dec->getWidth() - 2, y); }
  virtual void decode_2ndLast_y(int y) { decode_x_y(dec->getWidth() - 2, y); }

  // --------- Treat last column of pixels in special way. --------------
  virtual int predict_Last_y(int y) { return predict_x_y(dec->getWidth() - 1, y); }
  virtual void encode_Last_y(int y) { encode_x_y(dec->getWidth() - 1, y); }
  virtual void decode_Last_y(int y) { decode_x_y(dec->getWidth() - 1, y); }

  /* 
     The image buffer (srcImg) and error buffer (errImg) 
     collect the pixels and prediction errors
     during the coding process; they act as consistent data source
     for the encoder/decoder to predict future pixel value. 

     This method must be called at the end of encode_?_? and decode_?_? methods.
  */
  void updateImageBuffer(int x, int y, int pixel, int error) {
    srcImg->setPixel(x, y, pixel);
    errImg->setPixel(x, y, error);
  }

  // Make sure the predicited value falls within the intensity range.
  inline int ensureBounds(int pixel) {
    if (pixel < 0)
      return 0;
    else
    if (pixel >= symbolCount)
      return symbolCount - 1;
    return pixel;
  }
};

int encRemapError(bool negativeModel, int symbolCount, int error, int predicted);
int decRemapError(bool negativeModel, int symbolCount, int error, int predicted);

#endif
