#ifndef _LIBUTIL_H
#define _LIBUTIL_H 1

#include <termios.h>

int openpty(int *, int *, char *, struct termios *, struct winsize *);

#endif
