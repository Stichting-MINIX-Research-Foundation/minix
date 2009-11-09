/*
 * a small awk clone
 *
 * (C) 1989 Saeko Hirabauashi & Kouichi Hirabayashi
 *
 * Absolutely no warranty. Use this software with your own risk.
 *
 * Permission to use, copy, modify and distribute this software for any
 * purpose and without fee is hereby granted, provided that the above
 * copyright and disclaimer notice.
 *
 * This program was written to fit into 64K+64K memory of the Minix 1.2.
 */


#include <stdio.h>

isKanji(c)
{
  c &= 0xff;
  return (c > 0x80 && c < 0xa0 || c > 0xdf && c < 0xfd);
}

jstrlen(s) char *s;
{
  int i;

  for (i = 0; *s; i++, s++)
	if (isKanji(*s))
		s++;
  return i;
}

char *
jStrchr(s, c) char *s;
{
  for ( ; *s; s++)
	if (isKanji(*s))
		s++;
	else if (*s == c)
		return s;
  return NULL;
}
