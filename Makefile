# Change your compiler settings here

CCPP = clang++
CC = clang

CFLAGS = -Wall -fstrict-aliasing -I.

ifeq ($(DEBUG),1)
CFLAGS += -g -O0
else
CFLAGS += -O3
endif

CPFLAGS = $(CFLAGS)


# List of object files to make

gcif_objects = gcif.o lodepng.o Log.o Mutex.o Clock.o Thread.o
gcif_objects += EndianNeutral.o lz4.o lz4hc.o HuffmanDecoder.o HuffmanEncoder.o
gcif_objects += MappedFile.o SystemInfo.o MurmurHash3.o


# Applications

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


# Clean target

.PHONY : clean

clean :
	-rm gcif $(gcif_objects)

