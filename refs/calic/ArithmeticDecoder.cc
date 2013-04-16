#include "Arithm.h"

ArithmeticDecoder::ArithmeticDecoder() {
}

ArithmeticDecoder::~ArithmeticDecoder() {
}

void ArithmeticDecoder::start(char *filename) {
  int i;
  char * msg = "< Decoder > cannot read file";

  if ((in_file = fopen(filename, "rb")) == NULL) 
    Error(msg);

  value = low = bit_index = extra_bits = 0;
  high = TopValue;
  byte_counter = 0;

  for (i = 1; i <= CodeValueBits; i++) {
    if (!bit_index) Input_Byte();
    value += value + (bit_buffer & 1);
    bit_buffer >>= 1;
    --bit_index; 
  }
}

void ArithmeticDecoder::stop() {
  fclose(in_file);
}

int ArithmeticDecoder::readSymbol(AdaptiveModel *model) {
  int symbol = model->selectSymbol(value, &low, &high);
  update();
  return symbol;
}

int ArithmeticDecoder::readBits(int nbits) {
  long lm1 = low - 1, range = high - lm1, prod = range;
  int symbol = (int) ((((value - lm1) << nbits) - 1) / range);

  prod *= symbol;
  high = lm1 + ((prod + range) >> nbits);
  low += prod >> nbits;
  update();
  return symbol;
}

void ArithmeticDecoder::Input_Byte() {
  if ((bit_buffer = getc(in_file)) == EOF)
    if (++extra_bits > CodeValueBits - 2)
      Error("< Decoder > attempted to read past end of file");

  ++byte_counter;
  bit_index = 8;
}

void ArithmeticDecoder::update() {
  for (;;) {
    if (high >= Half) {
      if (low >= Half) {
        value -= Half;  
        low -= Half;  
        high -= Half; 
      }
      else {
        if ((low >= FirstQtr) && (high < ThirdQtr)) {
          value -= FirstQtr;  low -= FirstQtr;
          high -= FirstQtr; 
        }
        else
          break;
      }
    }

    if (!bit_index) 
      Input_Byte();

    low <<= 1;
    high += high + 1;
    value += value + (bit_buffer & 1);
    bit_buffer >>= 1; 
    --bit_index; 
  }
}

