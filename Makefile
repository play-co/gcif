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

gcif_objects = gcif.o lodepng.o Log.o Mutex.o Clock.o Thread.o
gcif_objects += EndianNeutral.o lz4.o lz4hc.o HuffmanDecoder.o HuffmanEncoder.o
gcif_objects += MappedFile.o SystemInfo.o HotRodHash.o ImageWriter.o
gcif_objects += ImageReader.o ImageMaskWriter.o ImageMaskReader.o
gcif_objects += ImageCMWriter.o FilterScorer.o Filters.o Enforcer.o
gcif_objects += ImageLZWriter.o ImageLZReader.o ImageCMReader.o
gcif_objects += GCIFReader.o GCIFWriter.o EntropyEstimator.o WaitableFlag.o
#gcif_objects += ImageLPReader.o ImageLPWriter.o


# List of source files

SRCS = ./gcif.cpp encoder/lodepng.cpp encoder/Log.cpp encoder/Mutex.cpp
SRCS += encoder/Clock.cpp encoder/Thread.cpp decoder/EndianNeutral.cpp
SRCS += decoder/lz4.c encoder/lz4hc.c decoder/HuffmanDecoder.cpp
SRCS += encoder/HuffmanEncoder.cpp decoder/MappedFile.cpp
SRCS += encoder/SystemInfo.cpp decoder/HotRodHash.cpp encoder/ImageWriter.cpp
SRCS += decoder/ImageReader.cpp encoder/ImageMaskWriter.cpp
SRCS += decoder/ImageMaskReader.cpp encoder/ImageCMWriter.cpp
SRCS += encoder/FilterScorer.cpp decoder/Filters.cpp decoder/Enforcer.cpp
SRCS += encoder/ImageLZWriter.cpp decoder/ImageLZReader.cpp
SRCS += decoder/ImageCMReader.cpp decoder/GCIFReader.cpp encoder/GCIFWriter.cpp
SRCS += encoder/EntropyEstimator.cpp encoder/WaitableFlag.cpp
#SRCS += ImageLPReader.cpp ImageLPWriter.cpp


# Release target (default)

release : CFLAGS += $(OPTFLAGS)
release : gcif


# Debug target

debug : CFLAGS += -g -O0 -DDEBUG
debug : gcif


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

HotRodHash.o : decoder/HotRodHash.cpp
	$(CCPP) $(CPFLAGS) -c decoder/HotRodHash.cpp

ImageReader.o : decoder/ImageReader.cpp
	$(CCPP) $(CPFLAGS) -c decoder/ImageReader.cpp

ImageWriter.o : encoder/ImageWriter.cpp
	$(CCPP) $(CPFLAGS) -c encoder/ImageWriter.cpp

ImageMaskReader.o : decoder/ImageMaskReader.cpp
	$(CCPP) $(CPFLAGS) -c decoder/ImageMaskReader.cpp

ImageMaskWriter.o : encoder/ImageMaskWriter.cpp
	$(CCPP) $(CPFLAGS) -c encoder/ImageMaskWriter.cpp

ImageCMWriter.o : encoder/ImageCMWriter.cpp
	$(CCPP) $(CPFLAGS) -c encoder/ImageCMWriter.cpp

FilterScorer.o : encoder/FilterScorer.cpp
	$(CCPP) $(CPFLAGS) -c encoder/FilterScorer.cpp

Filters.o : decoder/Filters.cpp
	$(CCPP) $(CPFLAGS) -c decoder/Filters.cpp

ImageLZWriter.o : encoder/ImageLZWriter.cpp
	$(CCPP) $(CPFLAGS) -c encoder/ImageLZWriter.cpp

ImageLZReader.o : decoder/ImageLZReader.cpp
	$(CCPP) $(CPFLAGS) -c decoder/ImageLZReader.cpp

ImageCMReader.o : decoder/ImageCMReader.cpp
	$(CCPP) $(CPFLAGS) -c decoder/ImageCMReader.cpp

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

#ImageLPWriter.o : ImageLPWriter.cpp
#	$(CCPP) $(CPFLAGS) -c ImageLPWriter.cpp

#ImageLPReader.o : ImageLPReader.cpp
#	$(CCPP) $(CPFLAGS) -c ImageLPReader.cpp


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

