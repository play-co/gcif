CC = gcc
ifeq ($(CC), icc)
  CFLAG = -O3 -tpp7 -xW -axW -unroll
else
  CFLAG = -O4 -funroll-loops -Wall
  ifeq ($(shell uname -m), i686)
    CFLAG += -march=pentiumpro
  endif
endif

ENCODER = encmrp
ENCOBJ = encmrp.o common.o rc.o
DECODER = decmrp
DECOBJ = decmrp.o common.o rc.o


all : $(ENCODER) $(DECODER)

$(ENCODER) : $(ENCOBJ)
	$(CC) $(CFLAG) -o $@ $(ENCOBJ) -lm

$(DECODER) : $(DECOBJ)
	$(CC) $(CFLAG) -o $@ $(DECOBJ) -lm

.c.o :
	$(CC) $(CFLAG) -c $<

encmrp.o : encmrp.c mrp.h
decmrp.o : decmrp.c mrp.h
common.o : common.c mrp.h
rc.o : rc.c mrp.h

clean :
	rm -f $(ENCODER) $(DECODER) $(ENCOBJ) $(DECOBJ) *~
