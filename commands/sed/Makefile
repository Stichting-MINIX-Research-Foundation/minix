# Makefile for minised

# If your compiler does not support this flags, just remove them.
# They only ensure that no new warning regressions make it into the source.
CFLAGS = -Wall -Wwrite-strings

minised: sedcomp.o sedexec.o
	$(CC) $(LFLAGS) sedcomp.o sedexec.o -o minised

sedcomp.o: sedcomp.c sed.h
sedexec.o: sedexec.c sed.h

install: minised
	install -o bin -m 755 minised /usr/bin/
	install -o bin -m 755 minised /bin/

clean:
	rm -f minised sedcomp.o sedexec.o

check: minised
	cd tests; ./run ../minised

