# Change your compiler settings here

CCPP = clang++
CC = clang

CFLAGS = -Wall -fstrict-aliasing -I.

ifeq ($(DEBUG),1)
CFLAGS += -g -O0
else
CFLAGS += -O4
endif

CPFLAGS = $(CFLAGS)


# List of object files

gcif_objects = gcif.o lodepng.o Log.o Mutex.o Clock.o Thread.o
gcif_objects += EndianNeutral.o lz4.o lz4hc.o HuffmanDecoder.o HuffmanEncoder.o
gcif_objects += MappedFile.o SystemInfo.o MurmurHash3.o ImageWriter.o
gcif_objects += ImageReader.o ImageMaskWriter.o ImageMaskReader.o
gcif_objects += ImageCMWriter.o EntropyEncoder.o FilterScorer.o Filters.o
gcif_objects += ImageLZWriter.o ImageLZReader.o ImageCMReader.o
gcif_objects += GCIFReader.o GCIFWriter.o EntropyDecoder.o ImageLPWriter.o


# List of source files

SRCS = gcif.cpp lodepng.cpp Log.cpp Mutex.cpp Clock.cpp Thread.cpp
SRCS += EndianNeutral.cpp lz4.c lz4hc.c HuffmanDecoder.cpp HuffmanEncoder.cpp
SRCS += MappedFile.cpp SystemInfo.cpp MurmurHash3.cpp ImageWriter.cpp
SRCS += ImageReader.cpp ImageMaskWriter.cpp ImageMaskReader.cpp
SRCS += ImageCMWriter.cpp EntropyEncoder.cpp FilterScorer.cpp Filters.cpp
SRCS += ImageLZWriter.cpp ImageLZReader.cpp ImageCMReader.cpp
SRCS += GCIFReader.cpp GCIFWriter.cpp EntropyDecoder.cpp ImageLPWriter.cpp


# Default target: gcif executable

gcif : $(gcif_objects)
	$(CCPP) -o gcif $(gcif_objects)


# Application files

gcif.o : gcif.cpp
	$(CCPP) $(CPFLAGS) -c gcif.cpp

lodepng.o : lodepng.cpp
	$(CCPP) $(CPFLAGS) -c lodepng.cpp

lz4.o : lz4.c
	$(CC) $(CCFLAGS) -c lz4.c

lz4hc.o : lz4hc.c
	$(CC) $(CCFLAGS) -c lz4hc.c

Log.o : Log.cpp
	$(CCPP) $(CPFLAGS) -c Log.cpp

Mutex.o : Mutex.cpp
	$(CCPP) $(CPFLAGS) -c Mutex.cpp

Clock.o : Clock.cpp
	$(CCPP) $(CPFLAGS) -c Clock.cpp

Thread.o : Thread.cpp
	$(CCPP) $(CPFLAGS) -c Thread.cpp

MappedFile.o : MappedFile.cpp
	$(CCPP) $(CPFLAGS) -c MappedFile.cpp

SystemInfo.o : SystemInfo.cpp
	$(CCPP) $(CPFLAGS) -c SystemInfo.cpp

EndianNeutral.o : EndianNeutral.cpp
	$(CCPP) $(CPFLAGS) -c EndianNeutral.cpp

HuffmanDecoder.o : HuffmanDecoder.cpp
	$(CCPP) $(CPFLAGS) -c HuffmanDecoder.cpp

HuffmanEncoder.o : HuffmanEncoder.cpp
	$(CCPP) $(CPFLAGS) -c HuffmanEncoder.cpp

MurmurHash3.o : MurmurHash3.cpp
	$(CCPP) $(CPFLAGS) -c MurmurHash3.cpp

ImageReader.o : ImageReader.cpp
	$(CCPP) $(CPFLAGS) -c ImageReader.cpp

ImageWriter.o : ImageWriter.cpp
	$(CCPP) $(CPFLAGS) -c ImageWriter.cpp

ImageMaskReader.o : ImageMaskReader.cpp
	$(CCPP) $(CPFLAGS) -c ImageMaskReader.cpp

ImageMaskWriter.o : ImageMaskWriter.cpp
	$(CCPP) $(CPFLAGS) -c ImageMaskWriter.cpp

ImageCMWriter.o : ImageCMWriter.cpp
	$(CCPP) $(CPFLAGS) -c ImageCMWriter.cpp

EntropyEncoder.o : EntropyEncoder.cpp
	$(CCPP) $(CPFLAGS) -c EntropyEncoder.cpp

FilterScorer.o : FilterScorer.cpp
	$(CCPP) $(CPFLAGS) -c FilterScorer.cpp

Filters.o : Filters.cpp
	$(CCPP) $(CPFLAGS) -c Filters.cpp

ImageLZWriter.o : ImageLZWriter.cpp
	$(CCPP) $(CPFLAGS) -c ImageLZWriter.cpp

ImageLZReader.o : ImageLZReader.cpp
	$(CCPP) $(CPFLAGS) -c ImageLZReader.cpp

ImageCMReader.o : ImageCMReader.cpp
	$(CCPP) $(CPFLAGS) -c ImageCMReader.cpp

GCIFReader.o : GCIFReader.cpp
	$(CCPP) $(CPFLAGS) -c GCIFReader.cpp

GCIFWriter.o : GCIFWriter.cpp
	$(CCPP) $(CPFLAGS) -c GCIFWriter.cpp

EntropyDecoder.o : EntropyDecoder.cpp
	$(CCPP) $(CPFLAGS) -c EntropyDecoder.cpp

ImageLPWriter.o : ImageLPWriter.cpp
	$(CCPP) $(CPFLAGS) -c ImageLPWriter.cpp


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

