#	$NetBSD: Makefile,v 1.221 2015/06/03 14:32:16 christos Exp $
#	from: @(#)Makefile	8.3 (Berkeley) 1/7/94

.include <bsd.own.mk>

SUBDIR= asa \
	banner basename \
	bzip2 bzip2recover cal calendar \
	checknr chpass cksum cmp col colcrt colrm \
	column comm csplit ctags cut \
	deroff dirname du \
	env expand \
	false find finger flock fold fmt fpr from \
	fsplit ftp genassym gencat getent \
	getopt \
	head hexdump id indent infocmp ipcrm ipcs join jot \
	lam last ldd leave \
	locale lock logger login logname lorder m4 \
	machine make man menuc mesg \
	mkcsmapper mkdep mkesdb mkfifo mklocale mkstr mktemp \
	msgc \
	nbperf netstat newgrp nice nl nohup \
	pagesize passwd paste patch pathchk pr \
	printenv printf pwhash \
	renice rev rsh \
	\
	sdiff sed seq shar shlock \
	shuffle  sort split stat su \
	tail tee telnet tic time touch \
	tput \
	tr true tsort tty ul uname unexpand unifdef \
	uniq units unvis unzip users \
	uudecode uuencode uuidgen vis \
	w \
	wall wc what whereis who whois \
	write xargs xinstall xstr yes

.if !defined(__MINIX)
SUBDIR+= ../external/zlib/pigz/bin/pigz
.endif # defined(__MINIX)

.if (${MKGROFF} == "no") || defined(__MINIX)
SUBDIR+= soelim
.endif

.if (${MKMAKEMANDB} == "no") || defined(__MINIX)
SUBDIR+= apropos whatis
.endif

.if (${MKRUMP} != "no")
SUBDIR+= rump_allserver rump_dhcpclient rump_halt rump_server shmif_dumpbus \
	rump_wmd
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
.if !defined(__MINIX)
SUBDIR+= nbsvtool
.endif # defined(__MINIX)
.endif

.include <bsd.subdir.mk>
