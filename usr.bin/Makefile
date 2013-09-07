#	$NetBSD: Makefile,v 1.210 2012/08/08 14:13:46 christos Exp $
#	from: @(#)Makefile	8.3 (Berkeley) 1/7/94

.include <bsd.own.mk>

SUBDIR= asa \
	banner basename bdes \
	bzip2 bzip2recover cal calendar \
	checknr chpass cksum col colcrt colrm \
	column comm csplit ctags cut \
	deroff dirname du \
	env expand \
	finger fold fpr from \
	fsplit ftp genassym getopt \
	head hexdump id indent infocmp join jot \
	lam ldd leave \
	lock login logname lorder m4 \
	machine make man menuc mesg \
	mkdep mkfifo mkstr mktemp \
	msgc \
	nbperf newgrp nice nl nohup nvi \
	passwd paste pathchk pr \
	printenv printf pwhash \
	renice rev \
	\
	sdiff sed seq shlock \
	shuffle soelim sort split stat su \
	tee tic tput \
	tr tsort tty ul uname unexpand unifdef \
	toproto \
	uniq units unvis unzip users \
	uuidgen vis \
	\
	wc what who whois \
	write xargs xinstall xstr yes

.if !defined(__MINIX)
SUBDIR+= ../external/zlib/pigz/bin/pigz
.else
# Non-NetBSD imports
SUBDIR+= ministat

# Minix commands
SUBDIR+= top 
SUBDIR+= toproto

# LSC FIXME For now we compile unconditionnaly apropos and whatis even if we 
# do not have whatis
SUBDIR+= apropos whatis
.endif # defie

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
