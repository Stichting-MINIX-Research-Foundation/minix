.if !defined(HOSTPROGNAME)
LDADD+= -lminlib -lcompat_minix -lasyn -lterminfo
.endif

BINDIR?=/usr/bin

# BJG too many warnings here
NOGCCERROR?=    yes
NOCLANGERROR?=  yes

