#ifndef __ARITHM_H__
#define __ARITHM_H__

#include <stdlib.h>
#include <stdio.h>

#define MaxSymbols 257

#define CodeValueBits   16

#define TopValue     65535L    /* 2^CodeValueBits - 1 */

#define FirstQtr     16384L    /* (TopValue + 1) / 4 */

#define Half         32768L    /* 2 * FirstQtr */

#define ThirdQtr     49152L    /* 3 * FirstQtr */

//#define MaxFrequency  4095L    /* 2^12 - 1 */
#define MaxFrequency  16383L   /* 2^14 - 1 */

#undef ASSERT
#define ASSERT(flag, msg)   if (!(flag)) {\
                              fprintf(stderr, "%s (%d): %s\n",  __FILE__, __LINE__, (msg));\
                              exit(1);\
                            }



class AdaptiveModel {

protected:

public:
  int numb_symb, *freq, *cum_freq, *index_to_symb, *symb_to_index;

  AdaptiveModel();
  virtual ~AdaptiveModel();
  
  virtual void init(int _numb_symb);
  virtual void reset();

  friend class ArithmeticEncoder;
  friend class ArithmeticDecoder;

  int write(char *filename);
  int write(FILE *fp);
  int read(char *filename);
  int read(FILE *fp);

  AdaptiveModel *clone();
  void copy(AdaptiveModel *model);

protected:

  int selectSymbol(long symb, long *low, long *high);
  void updateInterval(int symb, long *low, long *high);
  virtual void update(int index);
 
};

class StaticModel : public AdaptiveModel {
public:
  StaticModel() : AdaptiveModel() {}
  ~StaticModel() {}

  virtual void reset() {}
protected:
  virtual void update(int index) {}
};


class ArithmeticEncoder {

public:
  FILE * out_file;
  int bit_buffer, bit_index, bits_to_follow;
  long low, high, byte_counter;

  ArithmeticEncoder();
  ~ArithmeticEncoder();

  void start(char *filename);
  long stop();

  void writeSymbol(AdaptiveModel *model, int symb);
  void writeBits(int nbits, int symb);
  long getBytesUsed() { return byte_counter; }

protected:
  void Bit_Plus_Follow(int b);
  void update();
  void Output_Byte();
};


class ArithmeticDecoder {

public:
  FILE * in_file;
  int bit_buffer, bit_index, extra_bits;
  long low, high, value, byte_counter;

  ArithmeticDecoder();
  ~ArithmeticDecoder();

  void start(char *filename);
  void stop();

  int readSymbol(AdaptiveModel *model);
  int readBits(int nbits);
  int getBytesRead() { return byte_counter; }


protected:
  void update();
  void Input_Byte();
};


void Error(char *s);

#endif
