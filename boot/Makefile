# Makefile for the boot monitor package.

SYS	= ..

CC	= exec cc
CC86	= exec cc -mi86 -Was-ncc
CFLAGS	= -I$(SYS)
LIBS	= -lsys
LD	= $(CC) -s -.o
LD86	= $(CC86) -.o
BIN	= /usr/bin
MDEC	= /usr/mdec

all:	bootblock boot edparams masterboot jumpboot installboot addaout
dos:	boot.com mkfile.com

bootblock:	bootblock.s
	$(LD86) -com -o $@ bootblock.s

masterboot:	masterboot.s
	$(LD86) -com -o $@ masterboot.s

jumpboot:	jumpboot.s
	$(LD86) -com -o $@ jumpboot.s

boot.o:	boot.c
	$(CC86) $(CFLAGS) -c boot.c

bootimage.o:	bootimage.c
	$(CC86) $(CFLAGS) -c bootimage.c

rawfs86.o:	rawfs.c rawfs.o
	ln -f rawfs.c rawfs86.c
	$(CC86) $(CFLAGS) -c rawfs86.c
	rm rawfs86.c
	-cmp -s rawfs.o rawfs86.o && ln -f rawfs.o rawfs86.o

boot:	boothead.s boot.o bootimage.o rawfs86.o
	$(LD86) -o $@ \
		boothead.s boot.o bootimage.o rawfs86.o $(LIBS)
	install -S 8kb boot

edparams.o:	boot.c
	ln -f boot.c edparams.c
	$(CC) $(CFLAGS) -DUNIX -c edparams.c
	rm edparams.c

edparams:	edparams.o rawfs.o
	$(CC) $(CFLAGS) $(STRIP) -o $@ edparams.o rawfs.o
	install -S 16kw edparams

dosboot.o:	boot.c
	$(CC86) $(CFLAGS) -DDOS -o $@ -c boot.c

doshead.o:	doshead.s
	$(CC) -mi386 -o $@ -c doshead.s

dosboot:	doshead.o dosboot.o bootimage.o rawfs86.o
	$(LD86) -com -o $@ \
		doshead.o dosboot.o bootimage.o rawfs86.o $(LIBS)

boot.com:	dosboot
	./a.out2com dosboot boot.com

mkfile:	mkfhead.s mkfile.c
	$(LD) -.o -mi86 -com -o $@ mkfhead.s mkfile.c $(LIBS)

mkfile.com:	mkfile
	./a.out2com mkfile mkfile.com

installboot:	installboot.o rawfs.o
	$(CC) $(STRIP) -o installboot installboot.o rawfs.o
	install -S 6kw installboot

addaout:	addaout.o
	$(CC) -o addaout addaout.o

installboot.o bootimage.o: image.h
boot.o bootimage.o dosboot.o edparams.o: boot.h
rawfs.o rawfs86.o installboot.o boot.o bootimage.o: rawfs.h

install:	$(MDEC)/bootblock $(MDEC)/boot $(MDEC)/masterboot \
		$(MDEC)/jumpboot $(BIN)/installboot $(BIN)/edparams
dosinstall:	$(MDEC)/boot.com $(MDEC)/mkfile.com

$(MDEC)/bootblock:	bootblock
	install -cs -o bin -m 644 $? $@

$(MDEC)/boot:		boot
	install -cs -o bin -m 644 $? $@

$(MDEC)/boot.com:	boot.com
	install -c -m 644 $? $@

$(MDEC)/mkfile.com:	mkfile.com
	install -c -m 644 $? $@

$(MDEC)/masterboot:	masterboot
	install -cs -o bin -m 644 $? $@

$(MDEC)/jumpboot:	jumpboot
	install -cs -o bin -m 644 $? $@

$(BIN)/installboot:	installboot
	install -cs -o bin $? $@

$(BIN)/addaout:	addaout
	install -cs -o bin $? $@

$(BIN)/edparams:	edparams
	install -cs -o bin $? $@

clean:
	rm -f *.bak *.o
	rm -f bootblock addaout installboot boot masterboot jumpboot edparams 
	rm -f dosboot boot.com mkfile mkfile.com
