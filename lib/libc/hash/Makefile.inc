#	$NetBSD: Makefile.inc,v 1.12 2012/07/08 01:21:12 rmind Exp $
#	$OpenBSD: Makefile.inc,v 1.5 1997/07/17 06:02:42 millert Exp $

# hash functions
.PATH: ${ARCHDIR}/hash ${.CURDIR}/hash

.include "${.CURDIR}/hash/md2/Makefile.inc"
.include "${.CURDIR}/hash/rmd160/Makefile.inc"
.include "${.CURDIR}/hash/sha1/Makefile.inc"
.include "${.CURDIR}/hash/sha2/Makefile.inc"

.include "${.CURDIR}/hash/murmurhash/Makefile.inc"
