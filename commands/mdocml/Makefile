
VERSION	   = 1.10.2
VDATE	   = 19 June 2010

VFLAGS	   = -DVERSION="\"$(VERSION)\""

.if ${CC} == gcc
WFLAGS     = -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings
CFLAGS    += -g $(WFLAGS) $(VFLAGS) -DHAVE_CONFIG_H
ARFLAGS	   = rs
.else
CFLAGS	+=	${VFLAGS} -DHAVE_CONFIG_H
ARFLAGS	   = r
.endif

# Specify this if you want to hard-code the operating system to appear
# in the lower-left hand corner of -mdoc manuals.
# CFLAGS += -DOSNAME="\"OpenBSD 4.5\""

# Unset this if you don't want Xo/Xc allowing split `It' lines, which
# breaks symmetry.
CFLAGS	  += -DUGLY

SRCS   = main.c mdoc_term.c chars.c term.c tree.c compat.c \
     man_term.c html.c mdoc_html.c man_html.c out.c \
     term_ps.c term_ascii.c man_macro.c man.c man_hash.c \
     man_validate.c man_action.c mandoc.c man_argv.c roff.c \
     mdoc_macro.c mdoc.c mdoc_hash.c mdoc_strings.c mdoc_argv.c \
     mdoc_validate.c mdoc_action.c lib.c att.c arch.c vol.c \
     msec.c st.c

MAN	   = mandoc.1 mdoc.3 mdoc.7 mandoc_char.7 man.7 man.3 roff.7 roff.3

PROG	   = mandoc

.include <bsd.prog.mk>
