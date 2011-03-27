CC=gcc
BIN=bin/arppoison
OBJ=obj/arppoison.o
INC=`libnet-config --defines`
LIB=`libnet-config --libs` -ldnet
DEPFILES=src/arppoison.c
#  CCFLAGS=-Wall -Werror -Os -fomit-frame-pointer -Wformat=2 -DNDEBUG
CCFLAGS=-Wall -Werror -O0 -g -Wformat=2 -DDEBUG

compile: ${OBJ}
	@# link
	${CC} ${CCFLAGS} obj/*.o -o ${BIN} ${LIB}

obj/arppoison.o: ${DEPFILES}
	${CC} ${CCFLAGS} -c src/arppoison.c -o obj/arppoison.o ${INC}

clean:
	rm -rf obj/*
	rm -rf bin/*
	rm -rf doc/html
	rm -rf doc/latex

doc: ${OBJ} doc/arppoison
	doxygen doc/arppoison
