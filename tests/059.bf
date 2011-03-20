# Referenz auf undefinierten '%'-Platzhalter

all: test.o

%.o: %1.c %2.cc

#SHOULD_FAIL:L01
