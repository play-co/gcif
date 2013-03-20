# Change your compiler settings here

CC = clang++

CFLAGS = -Wall -fstrict-aliasing -I.

ifeq ($(DEBUG),1)
CFLAGS += -g -O0
else
CFLAGS += -O3
endif


# List of object files to make

gcif_objects = gcif.o lodepng.o Log.o Mutex.o Clock.o Thread.o EndianNeutral.o


# Applications

gcif : $(gcif_objects)
	$(CC) -o gcif $(gcif_objects)


# Application files

gcif.o : gcif.cpp
	$(CC) $(CFLAGS) -c gcif.cpp

lodepng.o : lodepng.cpp
	$(CC) $(CFLAGS) -c lodepng.cpp

Log.o : Log.cpp
	$(CC) $(CFLAGS) -c Log.cpp

Mutex.o : Mutex.cpp
	$(CC) $(CFLAGS) -c Mutex.cpp

Clock.o : Clock.cpp
	$(CC) $(CFLAGS) -c Clock.cpp

Thread.o : Thread.cpp
	$(CC) $(CFLAGS) -c Thread.cpp

EndianNeutral.o : EndianNeutral.cpp
	$(CC) $(CFLAGS) -c EndianNeutral.cpp


# Clean target

.PHONY : clean

clean :
	-rm gcif $(gcif_objects)

