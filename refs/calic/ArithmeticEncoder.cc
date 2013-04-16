#include "Arithm.h"

ArithmeticEncoder::ArithmeticEncoder() {
}

ArithmeticEncoder::~ArithmeticEncoder() {
}

void ArithmeticEncoder::start(char *filename) {

  char * msg = "< Encoder > cannot write to file";

  if ((out_file = fopen(filename, "wb")) == NULL) {
    Error(msg);
  }

  bit_index = 8;  
  bit_buffer = bits_to_follow = 0;
  byte_counter = low = 0;
  high = TopValue;
}

long ArithmeticEncoder::stop() {
  char * msg = "< Encoder > cannot write to file";

  ++bits_to_follow;
  ++byte_counter;

  Bit_Plus_Follow(low >= FirstQtr);

  if (putc(bit_buffer >> bit_index, out_file) == EOF) {
    Error(msg);
  }
  if (fclose(out_file) == EOF) {
    Error(msg);
  }

  return byte_counter;
}

void ArithmeticEncoder::writeSymbol(AdaptiveModel *model, int symb) {
  model->updateInterval(symb, &low, &high);
  update();
}

void ArithmeticEncoder::writeBits(int nbits, int symbol) {
  long lm1 = low - 1, range = high - lm1, prod = range;

  symbol &= (1 << nbits) - 1;
  prod *= symbol;
  high = lm1 + ((prod + range) >> nbits);
  low += prod >> nbits;
  update();
}

void ArithmeticEncoder::Bit_Plus_Follow(int b) {

  bit_buffer >>= 1;
  if (b) bit_buffer |= 0x80;
  if (--bit_index == 0) Output_Byte();
  while (bits_to_follow > 0) {
    bit_buffer >>= 1;  --bits_to_follow;
    if (!b) bit_buffer |= 0x80;
    if (--bit_index == 0) Output_Byte(); 
  }
}

void ArithmeticEncoder::Output_Byte() {
  byte_counter++;  
  bit_index = 8;
  if (putc(bit_buffer, out_file) == EOF)
    Error("< Encoder > cannot write to file");
}

void ArithmeticEncoder::update() {
  for (;;) {
    if (high < Half)
      Bit_Plus_Follow(0);
    else {
      if (low >= Half) {
        Bit_Plus_Follow(1);
        low -=  Half;  
        high -= Half; 
      }
      else {
        if ((low >= FirstQtr) && (high < ThirdQtr)) {
          bits_to_follow++;
          low -= FirstQtr;
          high -= FirstQtr; 
        }
        else
          break;
      }
    }

    low <<= 1;  
    high += high + 1; 
  }
}

void Error(char *msg) {
  fprintf(stderr, "%s\n", msg);
  exit(1);
}
