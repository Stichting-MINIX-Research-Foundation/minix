#	$NetBSD: Makefile,v 1.210 2012/08/08 14:13:46 christos Exp $
#	from: @(#)Makefile	8.3 (Berkeley) 1/7/94

.include <bsd.own.mk>

SUBDIR= \
	banner basename bdes \
	bzip2 bzip2recover \
	cal chpass cksum \
	col column ctags cut \
	dirname du \
	env expand \
	finger from \
	ftp genassym getopt \
	head indent infocmp join \
	ldd leave \
	login logname lorder m4 \
	machine make man \
	mkdep mktemp \
	\
	nbperf newgrp nice nl nohup nvi \
	passwd pr \
	printenv printf \
	rev \
	\
	shuffle sed seq \
	sort stat su \
	tee tic tput \
	tr tsort unexpand \
	toproto \
	uniq units unzip users \
	uuidgen \
	\
	wc whatis who \
	xargs xinstall yes

.if !defined(__MINIX)
SUBDIR+= ../external/zlib/pigz/bin/pigz
.else
# Non-NetBSD imports
SUBDIR+= ministat

# Minix commands
SUBDIR+= top 

# LSC FIXME For now we compile unconditionnaly apropos even if we 
# do not have whatis
SUBDIR+= apropos
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
