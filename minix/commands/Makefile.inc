# Only include Minix-specific libraries when compiling
# for non-Minix, i.e. not natively in a crossbuild.
.if !defined(HOSTPROGNAME)
LDADD+= -lasyn -lterminfo
.endif

BINDIR?=/usr/bin

# BJG too many warnings here
NOGCCERROR?=    yes
NOCLANGERROR?=  yes

.include "../Makefile.inc"
