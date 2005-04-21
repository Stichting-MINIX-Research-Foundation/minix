# Makefile for cron.

CFLAGS=		-D_MINIX -D_POSIX_SOURCE
LDFLAGS=

all:	cron crontab

CRON_OBJ=	cron.o tab.o misc.o
CRONTAB_OBJ=	crontab.o tab.o misc.o

cron:	$(CRON_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(CRON_OBJ)
	install -S 8kw $@

crontab:	$(CRONTAB_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(CRONTAB_OBJ)
	install -S 4kw $@

install:	/usr/bin/cron /usr/bin/crontab

/usr/bin/cron:	cron
	install -cs $? $@

/usr/bin/crontab:	crontab
	install -cs -o root -m 4755 $? $@

clean:
	rm -f *.o cron crontab core a.out

# Dependencies.
cron.o crontab.o:	misc.h tab.h
tab.o:			misc.h tab.h
misc.o:			misc.h
