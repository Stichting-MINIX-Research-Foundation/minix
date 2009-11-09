/* $Header$ */
/*
 * (c) copyright 1983 by the Vrije Universiteit, Amsterdam, The Netherlands.
 *
 *          This product is part of the Amsterdam Compiler Kit.
 *
 * Permission to use, sell, duplicate or disclose this software must be
 * obtained in writing. Requests for such permissions may be sent to
 *
 *      Dr. Andrew S. Tanenbaum
 *      Wiskundig Seminarium
 *      Vrije Universiteit
 *      Postbox 7161
 *      1007 MC Amsterdam
 *      The Netherlands
 *
 */

/* function strbuf(var b:charbuf):string; */

char *strbuf(s) char *s; {
	return(s);
}

/* function strtobuf(s:string; var b:charbuf; blen:integer):integer; */

int strtobuf(s,b,l) char *s,*b; {
	int i;

	i = 0;
	while (--l>=0) {
		if ((*b++ = *s++) == 0)
			break;
		i++;
	}
	return(i);
}

/* function strlen(s:string):integer; */

int strlen(s) char *s; {
	int i;

	i = 0;
	while (*s++)
		i++;
	return(i);
}

/* function strfetch(s:string; i:integer):char; */

int strfetch(s,i) char *s; {
	return(s[i-1]);
}

/* procedure strstore(s:string; i:integer; c:char); */

strstore(s,i,c) char *s; {
	s[i-1] = c;
}
