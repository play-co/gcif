# Change your compiler settings here

CCPP = clang++
CC = clang

CFLAGS = -Wall -fstrict-aliasing -I.
CPFLAGS = $(CFLAGS)


# List of object files

gcif_objects = gcif.o lodepng.o Log.o Mutex.o Clock.o Thread.o
gcif_objects += EndianNeutral.o lz4.o lz4hc.o HuffmanDecoder.o HuffmanEncoder.o
gcif_objects += MappedFile.o SystemInfo.o HotRodHash.o ImageWriter.o
gcif_objects += ImageReader.o ImageMaskWriter.o ImageMaskReader.o
gcif_objects += ImageCMWriter.o FilterScorer.o Filters.o
gcif_objects += ImageLZWriter.o ImageLZReader.o ImageCMReader.o
gcif_objects += GCIFReader.o GCIFWriter.o EntropyEstimator.o
#gcif_objects += ImageLPReader.o ImageLPWriter.o


# List of source files

SRCS = gcif.cpp lodepng.cpp Log.cpp Mutex.cpp Clock.cpp Thread.cpp
SRCS += EndianNeutral.cpp lz4.c lz4hc.c HuffmanDecoder.cpp HuffmanEncoder.cpp
SRCS += MappedFile.cpp SystemInfo.cpp HotRodHash.cpp ImageWriter.cpp
SRCS += ImageReader.cpp ImageMaskWriter.cpp ImageMaskReader.cpp
SRCS += ImageCMWriter.cpp FilterScorer.cpp Filters.cpp
SRCS += ImageLZWriter.cpp ImageLZReader.cpp ImageCMReader.cpp
SRCS += GCIFReader.cpp GCIFWriter.cpp EntropyEstimator.o
#SRCS += ImageLPReader.cpp ImageLPWriter.cpp


# Release target (default)

release : CFLAGS += -O4
release : gcif


# Debug target

debug : CFLAGS += -g -O0 -DDEBUG
debug : gcif


# gcif executable

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

HotRodHash.o : HotRodHash.cpp
	$(CCPP) $(CPFLAGS) -c HotRodHash.cpp

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

EntropyEstimator.o : EntropyEstimator.cpp
	$(CCPP) $(CPFLAGS) -c EntropyEstimator.cpp

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

