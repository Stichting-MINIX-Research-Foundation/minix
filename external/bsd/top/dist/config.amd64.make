# Special rules for making 32-bit and 64-bit binaries on a sunos5 x86 box
all: i386/top amd64/top
	-cp -f /usr/lib/isaexec top

i386/top: $(SRC) $(INC)
	cd i386; $(MAKE) -f ../Makefile VPATH=.. srcdir=.. \
		BINARY=./top ARCHFLAG= top

amd64/top: $(SRC) $(INC)
	cd amd64; $(MAKE) -f ../Makefile VPATH=.. srcdir=.. BINARY=./top top
