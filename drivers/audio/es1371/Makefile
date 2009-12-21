# Makefile for the ES1371 sounddriver (SB16)

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm

# programs, flags, etc.
CC 		= exec cc
CFLAGS 	= -I$i
LDFLAGS = -i
LIBS 	= -lsys 

PROGRAM_NAME 	= es1371
INSTALL_BIN		= /usr/sbin/$(PROGRAM_NAME)
OBJ				= es1371.o AC97.o audio_fw.o liveupdate.o pci_helper.o wait.o sample_rate_converter.o



# build local binary
all: $(PROGRAM_NAME)


$(PROGRAM_NAME): $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)


audio_fw.o liveupdate.o: ../framework/audio_fw.c ../framework/liveupdate.c ../framework/audio_fw.h
	$(CC) -c ../framework/audio_fw.c ../framework/liveupdate.c
	

%.o: %.c
	$(CC) $(CFLAGS) -c $<


install: $(INSTALL_BIN)


$(INSTALL_BIN):	$(PROGRAM_NAME)
	install -o root -S 1024k -c $? $@


# clean up local files
clean:
	rm -f $(OBJ) $(PROGRAM_NAME) core 


depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend


# Include generated dependencies.
include .depend

