LDADD+= -lminlib -lcompat_minix -lasyn -lterminfo
BINDIR?=/usr/bin

# BJG too many warnings here
NOGCCERROR?=    yes
NOCLANGERROR?=  yes

