
CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE -I../../servers
CC1	= $(CC) $(CFLAGS) -c

LIBRARY	= ../libsocket.a
all:	$(LIBRARY)

OBJECTS	= \
	$(LIBRARY)(socket.o) \
	$(LIBRARY)(listen.o) \
	$(LIBRARY)(connect.o)	\
	$(LIBRARY)(shutdown.o)	\
	$(LIBRARY)(extra.o)	\
	$(LIBRARY)(bind.o)

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(socket.o):	socket.c
	$(CC1) socket.c

$(LIBRARY)(connect.o):	connect.c
	$(CC1) connect.c

$(LIBRARY)(listen.o):	listen.c
	$(CC1) listen.c

$(LIBRARY)(shutdown.o):	shutdown.c
	$(CC1) shutdown.c

$(LIBRARY)(extra.o):	extra.c
	$(CC1) extra.c

$(LIBRARY)(bind.o):	bind.c
	$(CC1) bind.c

clean:
	-rm *.o $(LIBRARY)
