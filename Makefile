# Change your compiler settings here

#CCPP = g++
#CC = gcc
#OPTFLAGS = -O3 -fomit-frame-pointer -funroll-loops
CCPP = clang++
CC = clang
OPTFLAGS = -O4
CFLAGS = -Wall -fstrict-aliasing
CPFLAGS = $(CFLAGS)


# List of object files

decode_objects = EndianNeutral.o Enforcer.o Filters.o GCIFReader.o
decode_objects += HuffmanDecoder.o ImageRGBAReader.o ImageLZReader.o
decode_objects += ImageMaskReader.o ImageReader.o MappedFile.o lz4.o
decode_objects += ImagePaletteReader.o MonoReader.o SmallPaletteReader.o

gcif_objects = gcif.o lodepng.o Log.o Mutex.o Clock.o Thread.o
gcif_objects += lz4hc.o HuffmanEncoder.o PaletteOptimizer.o
gcif_objects += SystemInfo.o ImageWriter.o SmallPaletteWriter.o
gcif_objects += ImageMaskWriter.o MonoWriter.o
gcif_objects += ImageRGBAWriter.o FilterScorer.o
gcif_objects += ImageLZWriter.o ImagePaletteWriter.o
gcif_objects += GCIFWriter.o EntropyEstimator.o WaitableFlag.o
gcif_objects += $(decode_objects)
#gcif_objects += ImageLPReader.o ImageLPWriter.o


# List of source files

DECODE_SRCS = decoder/EndianNeutral.cpp decoder/Enforcer.cpp
DECODE_SRCS += decoder/Filters.cpp decoder/GCIFReader.cpp
DECODE_SRCS += decoder/HuffmanDecoder.cpp
DECODE_SRCS += decoder/ImageRGBAReader.cpp
DECODE_SRCS += decoder/ImageLZReader.cpp
DECODE_SRCS += decoder/ImagePaletteReader.cpp
DECODE_SRCS += decoder/ImageMaskReader.cpp
DECODE_SRCS += decoder/ImageReader.cpp
DECODE_SRCS += decoder/MappedFile.cpp
DECODE_SRCS += decoder/lz4.c decoder/SmallPaletteReader.cpp
DECODE_SRCS += decoder/MonoReader.cpp

SRCS = ./gcif.cpp encoder/lodepng.cpp encoder/Log.cpp encoder/Mutex.cpp
SRCS += encoder/Clock.cpp encoder/Thread.cpp
SRCS += encoder/lz4hc.c encoder/MonoWriter.cpp
SRCS += encoder/HuffmanEncoder.cpp
SRCS += encoder/SystemInfo.cpp encoder/ImageWriter.cpp
SRCS += encoder/ImageMaskWriter.cpp
SRCS += encoder/ImageRGBAWriter.cpp
SRCS += encoder/FilterScorer.cpp encoder/SmallPaletteWriter.cpp
SRCS += encoder/ImageLZWriter.cpp
SRCS += encoder/GCIFWriter.cpp encoder/PaletteOptimizer.cpp
SRCS += encoder/ImagePaletteWriter.cpp
SRCS += encoder/EntropyEstimator.cpp encoder/WaitableFlag.cpp
SRCS += encoder/MonoWriter.cpp
SRCS += $(DECODE_SRCS)
#SRCS += ImageLPReader.cpp ImageLPWriter.cpp


# Release target (default)

release : CFLAGS += $(OPTFLAGS) -DCAT_COMPILE_MMAP
release : gcif


# Debug target

debug : CFLAGS += -g -O0 -DDEBUG -DCAT_COMPILE_MMAP
debug : gcif


# decomp executable

release-decomp : CFLAGS += $(OPTFLAGS) -DCAT_COMPILE_MMAP
release-decomp : decomp


# decompe executable

decomp : $(decode_objects) decomp.o
	$(CCPP) -o decomp $(decode_objects) decomp.o


# gcif executable

gcif : $(gcif_objects)
	$(CCPP) -o gcif $(gcif_objects)


# Application files

gcif.o : ./gcif.cpp
	$(CCPP) $(CPFLAGS) -c ./gcif.cpp

lodepng.o : encoder/lodepng.cpp
	$(CCPP) $(CPFLAGS) -c encoder/lodepng.cpp

lz4.o : decoder/lz4.c
	$(CC) $(CCFLAGS) -c decoder/lz4.c

lz4hc.o : encoder/lz4hc.c
	$(CC) $(CCFLAGS) -c encoder/lz4hc.c

Log.o : encoder/Log.cpp
	$(CCPP) $(CPFLAGS) -c encoder/Log.cpp

Mutex.o : encoder/Mutex.cpp
	$(CCPP) $(CPFLAGS) -c encoder/Mutex.cpp

Clock.o : encoder/Clock.cpp
	$(CCPP) $(CPFLAGS) -c encoder/Clock.cpp

Thread.o : encoder/Thread.cpp
	$(CCPP) $(CPFLAGS) -c encoder/Thread.cpp

MappedFile.o : decoder/MappedFile.cpp
	$(CCPP) $(CPFLAGS) -c decoder/MappedFile.cpp

SystemInfo.o : encoder/SystemInfo.cpp
	$(CCPP) $(CPFLAGS) -c encoder/SystemInfo.cpp

EndianNeutral.o : decoder/EndianNeutral.cpp
	$(CCPP) $(CPFLAGS) -c decoder/EndianNeutral.cpp

HuffmanDecoder.o : decoder/HuffmanDecoder.cpp
	$(CCPP) $(CPFLAGS) -c decoder/HuffmanDecoder.cpp

HuffmanEncoder.o : encoder/HuffmanEncoder.cpp
	$(CCPP) $(CPFLAGS) -c encoder/HuffmanEncoder.cpp

ImageReader.o : decoder/ImageReader.cpp
	$(CCPP) $(CPFLAGS) -c decoder/ImageReader.cpp

ImageWriter.o : encoder/ImageWriter.cpp
	$(CCPP) $(CPFLAGS) -c encoder/ImageWriter.cpp

ImageMaskReader.o : decoder/ImageMaskReader.cpp
	$(CCPP) $(CPFLAGS) -c decoder/ImageMaskReader.cpp

ImageMaskWriter.o : encoder/ImageMaskWriter.cpp
	$(CCPP) $(CPFLAGS) -c encoder/ImageMaskWriter.cpp

FilterScorer.o : encoder/FilterScorer.cpp
	$(CCPP) $(CPFLAGS) -c encoder/FilterScorer.cpp

Filters.o : decoder/Filters.cpp
	$(CCPP) $(CPFLAGS) -c decoder/Filters.cpp

ImageLZWriter.o : encoder/ImageLZWriter.cpp
	$(CCPP) $(CPFLAGS) -c encoder/ImageLZWriter.cpp

ImageLZReader.o : decoder/ImageLZReader.cpp
	$(CCPP) $(CPFLAGS) -c decoder/ImageLZReader.cpp

ImagePaletteWriter.o : encoder/ImagePaletteWriter.cpp
	$(CCPP) $(CPFLAGS) -c encoder/ImagePaletteWriter.cpp

ImagePaletteReader.o : decoder/ImagePaletteReader.cpp
	$(CCPP) $(CPFLAGS) -c decoder/ImagePaletteReader.cpp

ImageRGBAWriter.o : encoder/ImageRGBAWriter.cpp
	$(CCPP) $(CPFLAGS) -c encoder/ImageRGBAWriter.cpp

ImageRGBAReader.o : decoder/ImageRGBAReader.cpp
	$(CCPP) $(CPFLAGS) -c decoder/ImageRGBAReader.cpp

GCIFReader.o : decoder/GCIFReader.cpp
	$(CCPP) $(CPFLAGS) -c decoder/GCIFReader.cpp

GCIFWriter.o : encoder/GCIFWriter.cpp
	$(CCPP) $(CPFLAGS) -c encoder/GCIFWriter.cpp

EntropyEstimator.o : encoder/EntropyEstimator.cpp
	$(CCPP) $(CPFLAGS) -c encoder/EntropyEstimator.cpp

WaitableFlag.o : encoder/WaitableFlag.cpp
	$(CCPP) $(CPFLAGS) -c encoder/WaitableFlag.cpp

Enforcer.o : decoder/Enforcer.cpp
	$(CCPP) $(CPFLAGS) -c decoder/Enforcer.cpp

MonoReader.o : decoder/MonoReader.cpp
	$(CCPP) $(CPFLAGS) -c decoder/MonoReader.cpp

MonoWriter.o : encoder/MonoWriter.cpp
	$(CCPP) $(CPFLAGS) -c encoder/MonoWriter.cpp

PaletteOptimizer.o : encoder/PaletteOptimizer.cpp
	$(CCPP) $(CPFLAGS) -c encoder/PaletteOptimizer.cpp

SmallPaletteWriter.o : encoder/SmallPaletteWriter.cpp
	$(CCPP) $(CPFLAGS) -c encoder/SmallPaletteWriter.cpp

SmallPaletteReader.o : decoder/SmallPaletteReader.cpp
	$(CCPP) $(CPFLAGS) -c decoder/SmallPaletteReader.cpp

#ImageLPWriter.o : ImageLPWriter.cpp
#	$(CCPP) $(CPFLAGS) -c ImageLPWriter.cpp

#ImageLPReader.o : ImageLPReader.cpp
#	$(CCPP) $(CPFLAGS) -c ImageLPReader.cpp

decomp.o : decomp.cpp
	$(CCPP) $(CPFLAGS) -c decomp.cpp


# Depend target

depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	$(CCPP) $(CPPFLAGS) -MM $^ > ./.depend;

include .depend


# Clean target

.PHONY : clean

clean :
	-rm gcif $(gcif_objects)

