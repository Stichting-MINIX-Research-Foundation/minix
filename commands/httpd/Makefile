# Makefile for httpd
#
# 02/17/1996 			Michael Temari <Michael@TemWare.Com>
# 07/07/1996 Initial Release	Michael Temari <Michael@TemWare.Com>
# 12/29/2002 			Michael Temari <Michael@TemWare.Com>
# 07/07/2003			Al Woodhull <awoodhull@hampshire.edu>
#

CFLAGS=	-O -D_MINIX -D_POSIX_SOURCE -DDAEMON=1
#CFLAGS=	-O -D_MINIX -D_POSIX_SOURCE -DDAEMON=1 -DDEBUG=9
LDFLAGS=-i
BINDIR=/usr/local/bin

HTTPD_OBJS=	httpd.o utility.o request.o process.o reply.o \
		police.o cgiexec.o net.o config.o pass.o proxy.o
DIR2HTML_OBJS=	dir2html.o

all:	httpd dir2html

httpd:	$(HTTPD_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(HTTPD_OBJS)
	install -S 16kw $@

dir2html:	$(DIR2HTML_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(DIR2HTML_OBJS)
	install -S 8kw $@

clean:
	rm -f httpd $(HTTPD_OBJS) dir2html $(DIR2HTML_OBJS) *.bak

install:	$(BINDIR)/httpd $(BINDIR)/in.httpd $(BINDIR)/dir2html

tar:
	tar cvf ../httpd.tar Makefile README *.c *.h *.sh *.conf *.mtype

$(BINDIR)/httpd:	httpd
	install -cs -o bin $? $@

$(BINDIR)/in.httpd:	$(BINDIR)/httpd
	install -l $? $@

$(BINDIR)/dir2html:	dir2html
	install -cs -o bin $? $@

httpd.o:	httpd.c		http.h	utility.h	net.h	config.h
utility.o:	utility.c		utility.h		config.h
request.o:	request.c	http.h	utility.h		config.h
process.o:	process.c	http.h	utility.h
reply.o:	reply.c		http.h	utility.h	net.h	config.h
police.o:	police.c	http.h	utility.h		config.h pass.h
cgiexec.o:	cgiexec.c	http.h				config.h
net.o:		net.c					net.h
config.o:	config.c		utility.h		config.h
pass.o:		pass.c		pass.h
proxy.o:	proxy.c		http.h
dir2html.o:	dir2html.c

installman:
	mkdir -p /usr/local/man/man5 
	mkdir -p /usr/local/man/man8 
	cp -p httpd.conf.5 http_status.5 /usr/local/man/man5
	cp -p httpd.8 /usr/local/man/man8
	makewhatis /usr/local/man


