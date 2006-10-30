# Makefile for profile and profalyze.pl

CC=exec cc

all:			profile sprofalyze.pl cprofalyze.pl

profile:		profile.c
			$(CC) -o $@ profile.c
			install -S 65m $@
sprofalyze.pl:
			install $@

cprofalyze.pl:
			install $@

install: /usr/bin/profile /usr/bin/sprofalyze.pl /usr/bin/cprofalyze.pl

/usr/bin/profile:	profile
			install -cs -o bin $? $@

/usr/bin/sprofalyze.pl:	sprofalyze.pl
			install -o bin $? $@

/usr/bin/cprofalyze.pl:	cprofalyze.pl
			install -o bin $? $@

clean:	
			rm -f *.o core *.bak profile

