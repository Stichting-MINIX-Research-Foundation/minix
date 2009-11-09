/*	program DATABASE.C					*/

#include	<string.h>
#include	<stdio.h>
#include	"advent.h"
#include	"advdec.h"
#include	"advtext.h"

static char oline[256];

_PROTOTYPE(void rdupto, (FILE *, int, int, char *));
_PROTOTYPE(void rdskip, (FILE *, int, int));

/*
  Function to scan a file up to a specified
  point and either print or return a string.
*/
void rdupto(fdi, uptoc, print, string)
FILE *fdi;
int uptoc, print;
char *string;
{
    int c, i;
    static _CONST unsigned char key[4] = {'c' | 0x80, 'L' | 0x80,
					  'y' | 0x80, 'D' | 0x80};

    i = 1;
    while ((c = getc(fdi)) != uptoc && c != EOF) {
	if (c == '\n')
	    i = 1;
	if (c >= 0x80)
	    c ^= key[i++ & 3];
	if (c == '\r')
	    continue;
	if (print)
	    putchar(c);
	else
	    *string++ = (char) c;
    }
    if (!print)
	*string = '\0';
    return;
}

/*
  Function to read a file skipping
  a given character a specified number
  of times, with or without repositioning
  the file.
*/
void rdskip(fdi, skipc, n)
FILE *fdi;
int skipc, n;
{
    int c;

    while (n--)
	while ((c = getc(fdi)) != skipc)
	    if (c == EOF)
		bug(32);
    return;
}

/*
  Routine to request a yes or no answer to a question.
*/
boolean yes(msg1, msg2, msg3)
int msg1, msg2, msg3;
{
    char answer[INPUTBUFLEN];

    if (msg1)
	rspeak(msg1);
    do {
	switch (*ask("\n> ", answer, sizeof(answer))) {
	case 'n':
	case 'N':
	    if (msg3)
		rspeak(msg3);
	    return (FALSE);
	case 'y':
	case 'Y':
	    if (msg2)
		rspeak(msg2);
	    return (TRUE);
	default:
	    fputs("Please answer Y (yes) or N (no).", stdout);
	}
    } while (TRUE);
}

/*
  Print a location description from "advent4.txt"
*/
void rspeak(msg)
int msg;
{
    if (msg == 54)
	printf("ok.\n");
    else {
	fseek(fd4, idx4[msg - 1], 0);
	rdupto(fd4, '#', 1, 0);
    }
    return;
}

/*
  Print an item message for a given state from "advent3.txt"
*/
void pspeak(item, state)
int item, state;
{
    fseek(fd3, idx3[item - 1], 0);
    rdskip(fd3, '/', state + 2);
    rdupto(fd3, '/', FALSE, oline);
    if (strncmp(oline, "<$$<", 4) != 0)
	printf("%s", oline);
    return;
}

/*
  Print a long location description from "advent1.txt"
*/
void desclg(loc)
int loc;
{
    fseek(fd1, idx1[loc - 1], 0);
    rdupto(fd1, '#', 1, 0);
    return;
}

/*
  Print a short location description from "advent2.txt"
*/
void descsh(loc)
int loc;
{
    fseek(fd2, idx2[loc - 1], 0);
    rdupto(fd2, '#', 1, 0);
    return;
}
