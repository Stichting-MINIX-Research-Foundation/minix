/* tinytcap.c */

/* This file contains functions which simulate the termcap functions.
 *
 * It doesn't access a "termcap" file.  Instead, it uses an initialized array
 * of strings to store the entries.  Any string that doesn't start with a ':'
 * is taken to be the name of a type of terminal.  Any string that does start
 * with a ':' is interpretted as the list of fields describing all of the
 * terminal types that precede it.
 *
 * Note: since these are C strings, you can't use special sequences like
 * ^M or \E in the fields; your C compiler won't understand them.  Also,
 * at run time there is no way to tell the difference between ':' and '\072'
 * so I sure hope your terminal definition doesn't require a ':' character.
 *
 * getenv(TERM) on VMS checks the SET TERM device setting.  To implement
 * non-standard terminals set the logical ELVIS_TERM in VMS. (jdc)
 *
 * Other possible terminal types are...
 * 	TERM_WYSE925	- "wyse925", a Wyse 50 terminal emulating Televideo 925
 * ... or you could set $TERMCAP to the terminal's description string, which
 * $TERM set up to match it.
 *
 * Note that you can include several terminal types at the same time.  Elvis
 * chooses which entry to use at runtime, based primarily on the value of $TERM.
 */


#include "config.h"
extern char *getenv();

/* decide which terminal descriptions should *always* be included. */
#if MSDOS
# define	TERM_NANSI
# define	TERM_DOSANSI
# if RAINBOW
#  define	TERM_RAINBOW
# endif
#endif

#if VMS
# define	TERM_VT100
# define	TERM_VT100W
# define	TERM_VT52
#endif

#if AMIGA
# define	TERM_AMIGA	/* Internal Amiga termcap entry */
/* # define	TERM_VT52	/* The rest of these are here for those */
# define	TERM_VT100	/* people who want to use elvis over an */
/* # define	TERM_NANSI	/* AUX: port (serial.device). */
/* # define	TERM_DOSANSI	/* Take out all but AMIGA to save memory. */
/* # define	TERM_MINIX	/* Vanilla ANSI? */
/* # define	TERM_925	/* Hang a terminal off your Amiga */
#endif

#if MINIX || UNIXV
# define	TERM_MINIX
#endif

#if COHERENT
# define	TERM_COHERENT
#endif

#if TOS
# define	TERM_ATARI
#endif

static char *termcap[] =
{
#ifdef TERM_AMIGA
"AA",
"amiga",
"Amiga ANSI",
/* Amiga termcap modified from version 1.3 by Kent Polk */
":co#80:li#24:am:bs:bw:xn:\
:AL=\233%dL:DC=\233%dP:DL=\233%dM:DO=\233%dB:\
:LE=\233%dD:RI=\233%dC:SF=\233%dS:SR=\233%dT:UP=\233%dA:IC=\233%d@:\
:ae=\2330m:al=\233L:as=\2333m:bl=\007:bt=\233Z:cd=\233J:\
:ce=\233K:cl=\013:cm=\233%i%d;%dH:dc=\233P:dl=\233M:do=\233B:\
:kb=^H:ho=\233H:ic=\233@:is=\23320l:\
:mb=\2337;2m:md=\2331m:me=\2330m:mh=\2332m:mk=\2338m:mr=\2337m:nd=\233C:\
:rs=\033c:se=\2330m:sf=\233S:so=\2337m:sb=\233T:sr=\233T:ue=\23323m:\
:up=\233A:us=\2334m:vb=\007:ve=\233\040p:vi=\2330\040p:\
:k1=\2330~:k2=\2331~:k3=\2332~:k4=\2333~:k5=\2334~:\
:k6=\2335~:k7=\2336~:k8=\2337~:k9=\2338~:k0=\2339~:\
:s1=\23310~:s2=\23311~:s3=\23312~:s4=\23313~:s5=\23314~:\
:s6=\23315~:s7=\23316~:s8=\23317~:s9=\23318~:s0=\23319~:\
:kd=\233B:kl=\233D:kn#10:kr=\233C:ku=\233A:le=\233D:\
:kP=\233T:kN=\233S:kh=\233\040A:kH=\233\040@:",
#endif

#ifdef TERM_NANSI
"fansi",
"nnansi",
"nansi",
"pcbios",
":al=\033[L:dl=\033[M:am:bs:ce=\033[K:cl=\033[2J:\
:cm=\033[%i%d;%dH:co#80:do=\033[B:\
:k1=#;:k2=#<:k3=#=:k4=#>:k5=#?:k6=#@:k7=#A:k8=#B:k9=#C:k0=#D:\
:s1=#T:s2=#U:s3=#V:s4=#W:s5=#X:s6=#Y:s7=#Z:s8=#[:s9=#\\:s0=#]:\
:c1=#^:c2=#_:c3=#`:c4=#a:c5=#b:c6=#c:c7=#d:c8=#e:c9=#f:c0=#g:\
:a1=#h:a2=#i:a3=#j:a4=#k:a5=#l:a6=#m:a7=#n:a8=#o:a9=#p:a0=#q:\
:kd=#P:kh=#G:kH=#O:kI=#R:kl=#K:kN=#Q:kP=#I:kr=#M:ku=#H:\
:li#25:md=\033[1m:me=\033[m:nd=\033[C:se=\033[m:so=\033[7m:\
:ue=\033[m:up=\033[A:us=\033[4m:",
#endif

#ifdef TERM_DOSANSI
#if !ANY_UNIX
"ansi",
#endif
"dosansi",
":am:bs:ce=\033[K:cl=\033[2J:\
:cm=\033[%i%d;%dH:co#80:do=\033[B:\
:k1=#;:k2=#<:k3=#=:k4=#>:k5=#?:k6=#@:k7=#A:k8=#B:k9=#C:k0=#D:\
:s1=#T:s2=#U:s3=#V:s4=#W:s5=#X:s6=#Y:s7=#Z:s8=#[:s9=#\\:s0=#]:\
:c1=#^:c2=#_:c3=#`:c4=#a:c5=#b:c6=#c:c7=#d:c8=#e:c9=#f:c0=#g:\
:a1=#h:a2=#i:a3=#j:a4=#k:a5=#l:a6=#m:a7=#n:a8=#o:a9=#p:a0=#q:\
:kd=#P:kh=#G:kH=#O:kI=#R:kl=#K:kN=#Q:kP=#I:kr=#M:ku=#H:\
:li#25:md=\033[1m:me=\033[m:nd=\033[C:se=\033[m:so=\033[7m:\
:ue=\033[m:up=\033[A:us=\033[4m:",
#endif

#ifdef TERM_RAINBOW
"vt220",
"rainbow",
":al=\033[L:dl=\033[M:am:bs:ce=\033[K:cl=\033[2J:\
:cm=\033[%i%d;%dH:co#80:do=\033[B:kd=\033[B:kl=\033[D:\
:kr=\033[C:ku=\033[A:kP=\033[5~:kN=\033[6~:kI=\033[2~:\
:li#24:md=\033[1m:me=\033[m:nd=\033[C:se=\033[m:so=\033[7m:\
:ue=\033[m:up=\033[A:us=\033[4m:xn:",
#endif

#ifdef TERM_VT100
"vt100-80",
"vt200-80",
"vt300-80",
"vt101-80",
"vt102-80",
":al=\033[L:am:bs:ce=\033[K:cl=\033[2J:cm=\033[%i%d;%dH:\
:co#80:dl=\033[M:do=\033[B:k0=\033[20~:k1=\033[1~:\
:k2=\033[2~:k3=\033[3~:k4=\033[4~:k5=\033[5~:k6=\033[6~:\
:k7=\033[17~:k8=\033[18~:k9=\033[19~:kd=\033[B:kh=\033[H:\
:kH=\033[Y:kI=\033[I:kl=\033[D:kN=\033[U:kP=\033[V:\
:kr=\033[C:ku=\033[A:li#24:md=\033[1m:me=\033[m:nd=\033[C:\
:se=\033[m:so=\033[7m:ti=\033[1;24r\033[24;1H:\
:ue=\033[m:up=\033[A:us=\033[4m:xn:",
#endif

#ifdef TERM_VT100W
"vt100-w",
"vt200-w",
"vt300-w",
"vt101-w",
"vt102-w",
"vt100-132",
"vt200-132",
"vt300-132",
"vt101-132",
"vt102-132",
":al=\033[L:am:bs:ce=\033[K:cl=\033[2J:cm=\033[%i%d;%dH:\
:co#132:dl=\033[M:do=\033[B:k0=\033[20~:k1=\033[1~:\
:k2=\033[2~:k3=\033[3~:k4=\033[4~:k5=\033[5~:k6=\033[6~:\
:k7=\033[17~:k8=\033[18~:k9=\033[19~:kd=\033[B:kh=\033[H:\
:kH=\033[Y:kI=\033[I:kl=\033[D:kN=\033[U:kP=\033[V:\
:kr=\033[C:ku=\033[A:li#24:md=\033[1m:me=\033[m:nd=\033[C:\
:se=\033[m:so=\033[7m:ti=\033[1;24r\033[24;1H:\
:ue=\033[m:up=\033[A:us=\033[4m:xn:",
#endif

#ifdef TERM_VT52
"vt52",
":do=\n:le=\b:up=\033A:nd=\033C:cm=\033Y%+ %+ :ti=\033e\033v:\
:sr=\033I:cd=\033J:ce=\033K:cl=\033H\033J:co#80:li#24:\
:ku=\033A:kd=\033B:kr=\033C:kl=\033D:kb=\b:pt:am:xn:bs:",
#endif

#ifdef TERM_MINIX
"minix",
"ansi",
"AT386",
":al=\033[L:am:bs:ce=\033[K:cl=\033[2J:cm=\033[%i%d;%dH:\
:co#80:dl=\033[M:do=\033[B:k0=\033[20~:k1=\033[1~:\
:k2=\033[2~:k3=\033[3~:k4=\033[4~:k5=\033[5~:k6=\033[6~:\
:k7=\033[17~:k8=\033[18~:k9=\033[19~:kd=\033[B:kh=\033[H:\
:kH=\033[Y:kI=\033[I:kl=\033[D:kN=\033[U:kP=\033[V:\
:kr=\033[C:ku=\033[A:li#25:md=\033[1m:me=\033[m:nd=\033[C:\
:se=\033[m:so=\033[7m:ue=\033[m:up=\033[A:us=\033[4m:",
#endif /* MINIX */

#ifdef TERM_COHERENT
"coherent",
"ansipc",
":al=\033[L:am:bs:ce=\033[K:cl=\033[2J:cm=\033[%i%d;%dH:\
:co#80:dl=\033[M:do=\033[B:k0=\033[0x:k1=\033[1x:k2=\033[2x:\
:k3=\033[3x:k4=\033[4x:k5=\033[5x:k6=\033[6x:\
:k7=\033[7x:k8=\033[8x:k9=\033[9x:kd=\033[B:kh=\033[H:\
:kH=\033[24H:kI=\033[@:kl=\033[D:kN=\033[U:kP=\033[V:\
:kr=\033[C:ku=\033[A:li#24:md=\033[1m:me=\033[m:\
:nd=\033[C:se=\033[m:so=\033[7m:ue=\033[m:up=\033[A:\
:us=\033[4m:",
#endif /* COHERENT */

#ifdef TERM_ATARI
"atari-st",
"vt52",
":al=\033L:am:bs:ce=\033K:cl=\033E:cm=\033Y%i%+ %+ :\
:co#80:dl=\033M:do=\033B:\
:k1=#;:k2=#<:k3=#=:k4=#>:k5=#?:k6=#@:k7=#A:k8=#B:k9=#C:k0=#D:\
:s1=#T:s2=#U:s3=#V:s4=#W:s5=#X:s6=#Y:s7=#Z:s8=#[:s9=#\\:s0=#]:\
:c1=#^:c2=#_:c3=#`:c4=#a:c5=#b:c6=#c:c7=#d:c8=#e:c9=#f:c0=#g:\
:a1=#h:a2=#i:a3=#j:a4=#k:a5=#l:a6=#m:a7=#n:a8=#o:a9=#p:a0=#q:\
kd=#P:kh=#G:kI=#R:kl=#K:kr=#M:ku=#H:li#25:nd=\033C:se=\033q:\
:so=\033p:te=:ti=\033e\033v:up=\033A:",
#endif

#ifdef TERM_925
"wyse925",
":xn@:\
:hs:am:bs:co#80:li#24:cm=\033=%+ %+ :cl=\033*:cd=\033y:\
:ce=\033t:is=\033l\033\":\
:al=\033E:dl=\033R:im=:ei=:ic=\033Q:dc=\033W:\
:ho=\036:nd=\014:bt=\033I:pt:so=\033G4:se=\033G0:sg#1:us=\033G8:ue=\033G0:ug#1:\
:up=\013:do=\026:kb=\010:ku=\013:kd=\026:kl=\010:kr=\014:\
:kh=\036:ma=\026\012\014 :\
:k1=\001@\r:k2=\001A\r:k3=\001B\r:k4=\001C\r:k5=\001D\r:k6=\001E\r:k7=\001F\r:\
:k8=\001G\r:k9=\001H\r:k0=\001I\r:ko=ic,dc,al,dl,cl,ce,cd,bt:\
:ts=\033f:fs=\033g:ds=\033h:sr=\033j:",  /* was :xn: for tvi925 alone*/
#endif

(char *)0
};


static char *fields;


/*ARGSUSED*/
int tgetent(bp, name)
	char	*bp;	/* buffer for storing the entry -- ignored */
	char	*name;	/* name of the entry */
{
	int	i;

	/* if TERMCAP is defined, and seems to match, then use it */
	fields = getenv("TERMCAP");
	if (fields)
	{
		for (i = 0; fields[i] && fields[i] != ':'; i++)
		{
			if (!strncmp(fields + i, name, strlen(name)))
			{
				return 1;
			}
		}
	}

	/* locate the entry in termcap[] */
	for (i = 0; termcap[i] && strcmp(termcap[i], name); i++)
	{
	}
	if (!termcap[i])
	{
		return 0;
	}

	/* search forward for fields */
	while (termcap[i][0] != ':')
	{
		i++;
	}
	fields = termcap[i];
	return 1;
}


static char *find(id, vtype)
	char	*id;	/* name of a value to locate */
	int	vtype;	/* '=' for strings, '#' for numbers, or 0 for bools */
{
	int	i;

	/* search for a ':' followed by the two-letter id */
	for (i = 0; fields[i]; i++)
	{
		if (fields[i] == ':'
		 && fields[i + 1] == id[0]
		 && fields[i + 2] == id[1])
		{
			/* if correct type, then return its value */
			if (fields[i + 3] == vtype)
				return &fields[i + 4];
			else
				return (char *)0;
		}
	}
	return (char *)0;
}

int tgetnum(id)
	char	*id;
{
	id = find(id, '#');
	if (id)
	{
		return atoi(id);
	}
	return -1;
}

int tgetflag(id)
	char	*id;
{
	if (find(id, ':'))
	{
		return 1;
	}
	return 0;
}

/*ARGSUSED*/
char *tgetstr(id, bp)
	char	*id;
	char	**bp;	/* pointer to pointer to buffer - ignored */
{
	char	*cpy;

	/* find the string */
	id = find(id, '=');
	if (!id)
	{
		return (char *)0;
	}

	/* copy it into the buffer, and terminate it with NUL */
	for (cpy = *bp; *id != ':'; )
	{
		if (id[0] == '\\' && id[1] == 'E')
			*cpy++ = '\033', id += 2;
		else
			*cpy++ = *id++;
	}
	*cpy++ = '\0';

	/* update the bp pointer */
	id = *bp;
	*bp = cpy;

	/* return a pointer to the copy of the string */
	return id;
}

/*ARGSUSED*/
char *tgoto(cm, destcol, destrow)
	char	*cm;	/* cursor movement string -- ignored */
	int	destcol;/* destination column, 0 - 79 */
	int	destrow;/* destination row, 0 - 24 */
{
	static char buf[30];

#ifdef CRUNCH
# if TOS
	sprintf(buf, "\033Y%c%c", ' ' + destrow, ' ' + destcol);
# else
	sprintf(buf, "\033[%d;%dH", destrow + 1, destcol + 1);
# endif
#else
	if (cm[1] == 'Y' || cm[1] == '=')
		sprintf(buf, "\033%c%c%c", cm[1], ' ' + destrow, ' ' + destcol);
	else
		sprintf(buf, "\033[%d;%dH", destrow + 1, destcol + 1);
#endif
	return buf;
}

/*ARGSUSED*/
void tputs(cp, affcnt, outfn)
	char	*cp;		/* the string to output */
	int	affcnt;		/* number of affected lines -- ignored */
	int	(*outfn)();	/* the output function */
{
	while (*cp)
	{
		(*outfn)(*cp);
		cp++;
	}
}
