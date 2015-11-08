# Special rules for making 32-bit and 64-bit binaries on a sunos5 box
all: sparcv7/top sparcv9/top
	-cp -f $(ISAEXEC) top

sparcv7/top: $(SRC) $(INC)
	cd sparcv7; $(MAKE) -f ../Makefile VPATH=.. srcdir=.. \
		BINARY=./top ARCHFLAG= top

sparcv9/top: $(SRC) $(INC)
	cd sparcv9; $(MAKE) -f ../Makefile VPATH=.. srcdir=.. BINARY=./top top
