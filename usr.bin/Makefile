#	$NetBSD: Makefile,v 1.214 2013/11/22 16:10:25 christos Exp $
#	from: @(#)Makefile	8.3 (Berkeley) 1/7/94

.include <bsd.own.mk>

SUBDIR= asa \
	banner basename \
	bzip2 bzip2recover cal calendar \
	checknr chpass cksum col colcrt colrm \
	column comm csplit ctags cut \
	deroff dirname du \
	env expand \
	false finger fold fpr from \
	fsplit ftp genassym getopt \
	head hexdump id indent infocmp join jot \
	lam last ldd leave \
	lock login logname lorder m4 \
	machine make man menuc mesg \
	mkdep mkfifo mkstr mktemp \
	msgc \
	nbperf newgrp nice nl nohup \
	passwd paste patch pathchk pr \
	printenv printf pwhash \
	renice rev \
	\
	sdiff sed seq shar shlock \
	shuffle  sort split stat su \
	tail tee tic touch tput \
	tr true tsort tty ul uname unexpand unifdef \
	uniq units unvis unzip users \
	uudecode uuencode uuidgen vis \
	w \
	wall wc what whereis who whois \
	write xargs xinstall xstr yes

.if !defined(__MINIX)
SUBDIR+= ../external/zlib/pigz/bin/pigz
.else
# LSC FIXME For now we compile those unconditionnaly
SUBDIR+= apropos whatis bdes soelim 
.endif # defined(__MINIX)

.if (${MKGROFF} == "no")
SUBDIR+= soelim
.endif

.if (${MKMAKEMANDB} == "no")
SUBDIR+= apropos whatis
.endif

.if (${MKRUMP} != "no")
SUBDIR+= rump_allserver rump_dhcpclient rump_halt rump_server
.endif

.if (${MKBSDGREP} != "no")
SUBDIR+= grep
.endif

.if (${MKPIGZGZIP} != "no")
# nothing
.else
SUBDIR+= gzip 
.endif

.if (${MKHESIOD} != "no")
SUBDIR+= hesinfo
.endif

.if (${MKSKEY} != "no")
SUBDIR+= skey skeyinfo skeyinit
.endif

.if (${MKYP} != "no")
SUBDIR+= ypcat ypmatch ypwhich
.endif

.if (${MKCRYPTO} != "no")
SUBDIR+= bdes
SUBDIR+= nbsvtool
.endif

.include <bsd.subdir.mk>
