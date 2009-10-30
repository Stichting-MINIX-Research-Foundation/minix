# Makefile for gas2ack.

CFLAGS=		$(OPT)
LDFLAGS=	-i
CC = exec cc

all:	gas2ack

OBJ=	asm86.o gas2ack.o globals.o parse_gnu.o \
	tokenize.o emit_ack.o

gas2ack:	$(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

install:	/usr/bin/gas2ack

/usr/bin/gas2ack:	gas2ack
	install -cs -o bin gas2ack $@

clean:
	rm -f $(OBJ) gas2ack core

# Dependencies.
asm86.o:	asm86.h asmconv.h token.h
gas2ack.o:	asmconv.h languages.h asm86.h
globals.o:	asm86.h
parse_gnu.o:	asmconv.h languages.h token.h asm86.h
tokenize.o:	asmconv.h token.h
emit_ack.o:	asmconv.h languages.h token.h asm86.h
