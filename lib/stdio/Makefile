# Makefile for lib/stdio.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE
CC1	= $(CC) $(CFLAGS) -c

LIBRARY	= ../libc.a
all:	$(LIBRARY)

OBJECTS	= \
	$(LIBRARY)(clearerr.o) \
	$(LIBRARY)(data.o) \
	$(LIBRARY)(doprnt.o) \
	$(LIBRARY)(doscan.o) \
	$(LIBRARY)(ecvt.o) \
	$(LIBRARY)(fclose.o) \
	$(LIBRARY)(feof.o) \
	$(LIBRARY)(ferror.o) \
	$(LIBRARY)(fflush.o) \
	$(LIBRARY)(fgetc.o) \
	$(LIBRARY)(fgetpos.o) \
	$(LIBRARY)(fgets.o) \
	$(LIBRARY)(fileno.o) \
	$(LIBRARY)(fillbuf.o) \
	$(LIBRARY)(flushbuf.o) \
	$(LIBRARY)(fopen.o) \
	$(LIBRARY)(fprintf.o) \
	$(LIBRARY)(fputc.o) \
	$(LIBRARY)(fputs.o) \
	$(LIBRARY)(fread.o) \
	$(LIBRARY)(freopen.o) \
	$(LIBRARY)(fscanf.o) \
	$(LIBRARY)(fseek.o) \
	$(LIBRARY)(fsetpos.o) \
	$(LIBRARY)(ftell.o) \
	$(LIBRARY)(fwrite.o) \
	$(LIBRARY)(getc.o) \
	$(LIBRARY)(getchar.o) \
	$(LIBRARY)(gets.o) \
	$(LIBRARY)(icompute.o) \
	$(LIBRARY)(mktemp.o) \
	$(LIBRARY)(perror.o) \
	$(LIBRARY)(printf.o) \
	$(LIBRARY)(putc.o) \
	$(LIBRARY)(putchar.o) \
	$(LIBRARY)(puts.o) \
	$(LIBRARY)(remove.o) \
	$(LIBRARY)(rewind.o) \
	$(LIBRARY)(scanf.o) \
	$(LIBRARY)(setbuf.o) \
	$(LIBRARY)(setvbuf.o) \
	$(LIBRARY)(sprintf.o) \
	$(LIBRARY)(sscanf.o) \
	$(LIBRARY)(tmpfile.o) \
	$(LIBRARY)(tmpnam.o) \
	$(LIBRARY)(ungetc.o) \
	$(LIBRARY)(vfprintf.o) \
	$(LIBRARY)(vprintf.o) \
	$(LIBRARY)(vscanf.o) \
	$(LIBRARY)(vsprintf.o) \
	$(LIBRARY)(vsscanf.o) \

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(clearerr.o):	clearerr.c
	$(CC1) clearerr.c

$(LIBRARY)(data.o):	data.c
	$(CC1) data.c

$(LIBRARY)(doprnt.o):	doprnt.c
	$(CC1) doprnt.c

$(LIBRARY)(doscan.o):	doscan.c
	$(CC1) doscan.c

$(LIBRARY)(ecvt.o):	ecvt.c
	$(CC1) ecvt.c

$(LIBRARY)(fclose.o):	fclose.c
	$(CC1) fclose.c

$(LIBRARY)(feof.o):	feof.c
	$(CC1) feof.c

$(LIBRARY)(ferror.o):	ferror.c
	$(CC1) ferror.c

$(LIBRARY)(fflush.o):	fflush.c
	$(CC1) fflush.c

$(LIBRARY)(fgetc.o):	fgetc.c
	$(CC1) fgetc.c

$(LIBRARY)(fgetpos.o):	fgetpos.c
	$(CC1) fgetpos.c

$(LIBRARY)(fgets.o):	fgets.c
	$(CC1) fgets.c

$(LIBRARY)(fileno.o):	fileno.c
	$(CC1) fileno.c

$(LIBRARY)(fillbuf.o):	fillbuf.c
	$(CC1) fillbuf.c

$(LIBRARY)(flushbuf.o):	flushbuf.c
	$(CC1) flushbuf.c

$(LIBRARY)(fopen.o):	fopen.c
	$(CC1) fopen.c

$(LIBRARY)(fprintf.o):	fprintf.c
	$(CC1) fprintf.c

$(LIBRARY)(fputc.o):	fputc.c
	$(CC1) fputc.c

$(LIBRARY)(fputs.o):	fputs.c
	$(CC1) fputs.c

$(LIBRARY)(fread.o):	fread.c
	$(CC1) fread.c

$(LIBRARY)(freopen.o):	freopen.c
	$(CC1) freopen.c

$(LIBRARY)(fscanf.o):	fscanf.c
	$(CC1) fscanf.c

$(LIBRARY)(fseek.o):	fseek.c
	$(CC1) fseek.c

$(LIBRARY)(fsetpos.o):	fsetpos.c
	$(CC1) fsetpos.c

$(LIBRARY)(ftell.o):	ftell.c
	$(CC1) ftell.c

$(LIBRARY)(fwrite.o):	fwrite.c
	$(CC1) fwrite.c

$(LIBRARY)(getc.o):	getc.c
	$(CC1) getc.c

$(LIBRARY)(getchar.o):	getchar.c
	$(CC1) getchar.c

$(LIBRARY)(gets.o):	gets.c
	$(CC1) gets.c

$(LIBRARY)(icompute.o):	icompute.c
	$(CC1) icompute.c

$(LIBRARY)(mktemp.o):	mktemp.c
	$(CC1) mktemp.c

$(LIBRARY)(perror.o):	perror.c
	$(CC1) perror.c

$(LIBRARY)(printf.o):	printf.c
	$(CC1) printf.c

$(LIBRARY)(putc.o):	putc.c
	$(CC1) putc.c

$(LIBRARY)(putchar.o):	putchar.c
	$(CC1) putchar.c

$(LIBRARY)(puts.o):	puts.c
	$(CC1) puts.c

$(LIBRARY)(remove.o):	remove.c
	$(CC1) remove.c

$(LIBRARY)(rewind.o):	rewind.c
	$(CC1) rewind.c

$(LIBRARY)(scanf.o):	scanf.c
	$(CC1) scanf.c

$(LIBRARY)(setbuf.o):	setbuf.c
	$(CC1) setbuf.c

$(LIBRARY)(setvbuf.o):	setvbuf.c
	$(CC1) setvbuf.c

$(LIBRARY)(sprintf.o):	sprintf.c
	$(CC1) sprintf.c

$(LIBRARY)(sscanf.o):	sscanf.c
	$(CC1) sscanf.c

$(LIBRARY)(tmpfile.o):	tmpfile.c
	$(CC1) tmpfile.c

$(LIBRARY)(tmpnam.o):	tmpnam.c
	$(CC1) tmpnam.c

$(LIBRARY)(ungetc.o):	ungetc.c
	$(CC1) ungetc.c

$(LIBRARY)(vfprintf.o):	vfprintf.c
	$(CC1) vfprintf.c

$(LIBRARY)(vprintf.o):	vprintf.c
	$(CC1) vprintf.c

$(LIBRARY)(vscanf.o):	vscanf.c
	$(CC1) vscanf.c

$(LIBRARY)(vsprintf.o):	vsprintf.c
	$(CC1) vsprintf.c

$(LIBRARY)(vsscanf.o):	vsscanf.c
	$(CC1) vsscanf.c
