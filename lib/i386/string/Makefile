# Makefile for lib/i386/string.

CC1	= $(CC) -Was-ack -c

LIBRARY	= ../../libc.a
all:	$(LIBRARY)

OBJECTS	= \
	$(LIBRARY)(_memmove.o) \
	$(LIBRARY)(_strncat.o) \
	$(LIBRARY)(_strncmp.o) \
	$(LIBRARY)(_strncpy.o) \
	$(LIBRARY)(_strnlen.o) \
	$(LIBRARY)(bcmp.o) \
	$(LIBRARY)(bcopy.o) \
	$(LIBRARY)(bzero.o) \
	$(LIBRARY)(index.o) \
	$(LIBRARY)(memchr.o) \
	$(LIBRARY)(memcmp.o) \
	$(LIBRARY)(memcpy.o) \
	$(LIBRARY)(memmove.o) \
	$(LIBRARY)(memset.o) \
	$(LIBRARY)(rindex.o) \
	$(LIBRARY)(strcat.o) \
	$(LIBRARY)(strchr.o) \
	$(LIBRARY)(strcmp.o) \
	$(LIBRARY)(strcpy.o) \
	$(LIBRARY)(strlen.o) \
	$(LIBRARY)(strncat.o) \
	$(LIBRARY)(strncmp.o) \
	$(LIBRARY)(strncpy.o) \
	$(LIBRARY)(strnlen.o) \
	$(LIBRARY)(strrchr.o) \

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(_memmove.o):	_memmove.s
	$(CC1) _memmove.s

$(LIBRARY)(_strncat.o):	_strncat.s
	$(CC1) _strncat.s

$(LIBRARY)(_strncmp.o):	_strncmp.s
	$(CC1) _strncmp.s

$(LIBRARY)(_strncpy.o):	_strncpy.s
	$(CC1) _strncpy.s

$(LIBRARY)(_strnlen.o):	_strnlen.s
	$(CC1) _strnlen.s

$(LIBRARY)(bcmp.o):	bcmp.s
	$(CC1) bcmp.s

$(LIBRARY)(bcopy.o):	bcopy.s
	$(CC1) bcopy.s

$(LIBRARY)(bzero.o):	bzero.s
	$(CC1) bzero.s

$(LIBRARY)(index.o):	index.s
	$(CC1) index.s

$(LIBRARY)(memchr.o):	memchr.s
	$(CC1) memchr.s

$(LIBRARY)(memcmp.o):	memcmp.s
	$(CC1) memcmp.s

$(LIBRARY)(memcpy.o):	memcpy.s
	$(CC1) memcpy.s

$(LIBRARY)(memmove.o):	memmove.s
	$(CC1) memmove.s

$(LIBRARY)(memset.o):	memset.s
	$(CC1) memset.s

$(LIBRARY)(rindex.o):	rindex.s
	$(CC1) rindex.s

$(LIBRARY)(strcat.o):	strcat.s
	$(CC1) strcat.s

$(LIBRARY)(strchr.o):	strchr.s
	$(CC1) strchr.s

$(LIBRARY)(strcmp.o):	strcmp.s
	$(CC1) strcmp.s

$(LIBRARY)(strcpy.o):	strcpy.s
	$(CC1) strcpy.s

$(LIBRARY)(strlen.o):	strlen.s
	$(CC1) strlen.s

$(LIBRARY)(strncat.o):	strncat.s
	$(CC1) strncat.s

$(LIBRARY)(strncmp.o):	strncmp.s
	$(CC1) strncmp.s

$(LIBRARY)(strncpy.o):	strncpy.s
	$(CC1) strncpy.s

$(LIBRARY)(strnlen.o):	strnlen.s
	$(CC1) strnlen.s

$(LIBRARY)(strrchr.o):	strrchr.s
	$(CC1) strrchr.s
