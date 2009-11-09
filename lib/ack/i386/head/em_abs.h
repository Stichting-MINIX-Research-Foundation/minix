/* $Header$ */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
#define LINO_AD         0
#define FILN_AD         4

#define LINO            (*(int    *)(_hol0()+LINO_AD))
#define FILN            (*(char  **)(_hol0()+FILN_AD))

#define EARRAY          0
#define ERANGE          1
#define ESET            2
#define EIOVFL          3
#define EFOVFL          4
#define EFUNFL          5
#define EIDIVZ          6
#define EFDIVZ          7
#define EIUND           8
#define EFUND           9
#define ECONV           10

#define ESTACK          16
#define EHEAP           17
#define EILLINS         18
#define EODDZ           19
#define ECASE           20
#define EMEMFLT         21
#define EBADPTR         22
#define EBADPC          23
#define EBADLAE         24
#define EBADMON         25
#define EBADLIN         26
#define EBADGTO         27
