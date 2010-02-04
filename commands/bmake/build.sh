#!/bin/sh

export PATH=$PATH:/usr/gnu/bin
make -f Makefile.boot clean
make -f Makefile.boot CFLAGS="-g -Wall -DHAVE_SETENV -DHAVE_STRERROR -DHAVE_STRDUP -DHAVE_STRFTIME -DHAVE_VSNPRINTF -D_GNU_SOURCE -DUSE_SELECT -DSYSV -D_POSIX_SOURCE"
#make -f Makefile.boot
