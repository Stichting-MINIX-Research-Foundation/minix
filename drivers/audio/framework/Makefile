CC =	exec cc

all:	audio_fw.o liveupdate.o

audio_fw.o liveupdate.o:
	$(CC) -c audio_fw.c liveupdate.c

clean:
	rm -f a.out *.bak core errs audio_fw.o liveupdate.o
