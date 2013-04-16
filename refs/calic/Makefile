ARCH = $(shell uname)

CC = g++ -Wall

PROGRAMS = encode decode 

ARITHM_OBJS = \
  AdaptiveModel.o \
  ArithmeticDecoder.o \
  ArithmeticEncoder.o 

PNM_OBJS = \
  pnm.o 

MISC_OBJS = \
  CommandLine.o

IMAGECODEC_OBJS = \
  Image.o \
  StatisticalModel.o \
  BinaryModeModel.o \
  ImageCodec.o \
  CALIC.o 

#  InterframeCALIC.o
#  ISOCalic.o \
#  PredictOnlyCodec.o 

OBJS = $(ARITHM_OBJS) $(PNM_OBJS) $(MISC_OBJS) $(IMAGECODEC_OBJS)

INCDIR = .

CFLAGS = -O2 -I$(INCDIR) -D$(ARCH)

.SUFFIXES: .cc .
.cc.o:
	$(CC) -c $(INCLUDE) $(CFLAGS) $<

all: $(PROGRAMS)

encode: $(OBJS) encode.o 
	$(CC) -o $@ encode.o $(OBJS) -lm

decode: $(OBJS) decode.o
	$(CC) -o $@ decode.o $(OBJS) -lm

clean:
	rm -rf *.o $(PROGRAMS) core

