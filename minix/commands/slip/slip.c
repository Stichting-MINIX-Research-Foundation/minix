/*	slip 1.1 - Serial line IP			Author: Kees J. Bot
 *								19 Jul 1997
 */
#define nil 0
#include <sys/types.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/asynchio.h>

#if __minix && !__minix_vmd
#define HAS_ASYN	0	/* Standard Minix doesn't have async I/O. */
#else
#define HAS_ASYN	1	/* Everyone else does in some way. */
#endif

#if !HAS_ASYN
#include <signal.h>
#endif

#define END		0300		/* End of packet. */
#define ESC		0333		/* Byte stuffing escape. */
#define ESC_END		0334		/* END -> ESC ESC_END -> END. */
#define ESC_ESC		0335		/* ESC -> ESC ESC_ESC -> ESC. */

#define PACKLEN		2048		/* Max datagram size. */
#define SLIPLEN   (1 + 2*PACKLEN + 1)	/* Max serial size when all escaped. */

/* Pathetic fprintf() clone to avoid dragging in the stdio library. */
static int fprintf(int fd, const char *format, ...);
#define stderr	2

int main(int argc, char **argv)
{
    char *ps_device;
    int ps_fd;
    int doing[2], discard;
    ssize_t r;
#if !HAS_ASYN
    pid_t other_pid;
#endif
    size_t ps_len[2], sl_len[2];
    unsigned char *sl_end;
    unsigned char ps_buf[2][PACKLEN];
    unsigned char sl_buf[2][SLIPLEN];
    asynchio_t asyn;

    if (argc != 2) {
	fprintf(stderr, "Usage: slip psip-device\n");
	exit(1);
    }
    ps_device= argv[1];

    if ((ps_fd= open(ps_device, O_RDWR)) < 0) {
	fprintf(stderr, "slip: can't open %s: %s\n",
	    ps_device, strerror(errno));
	exit(1);
    }

    doing[0]= 1;	/* We're doing serial -> psip. */
    discard= 0;		/* No input error. */
    sl_len[0]= 0;	/* Nothing read from serial line yet. */
    sl_end= nil;	/* No END marker seen. */
    ps_len[0]= 0;	/* Nothing to write to pseudo IP device. */

    doing[1]= 1;	/* We're doing psip -> serial. */
    sl_len[1]= 0;	/* Nothing read from pseudo IP device yet. */
    ps_len[1]= 0;	/* Nothing to write to serial line. */

#if !HAS_ASYN
    /* Oops, standard Minix can't do asynchronous I/O.  Fork and let the parent
     * do serial -> psip, and the child do psip -> serial.  (Note that we have
     * to make sure that we're not reading and writing at the same time even
     * for standard Minix.  For Minix-vmd we do fill an input buffer while an
     * output buffer is waiting to be drained to improve performance a bit.)
     */
    switch ((other_pid= fork())) {
    case -1:
	fprintf(stderr, "slip: can't fork: %s\n", strerror(errno));
	exit(1);
    case 0:
	/* Child. */
	doing[0]= 0;	/* *Not* doing serial -> psip. */
	other_pid= getppid();
	break;
    default:
	/* Parent. */
	doing[1]= 0;	/* *Not* doing psip -> serial. */
    }
#endif

    asyn_init(&asyn);

    for (;;) {
	if (doing[0]) {
	    /* If there is an END marker in the serial input then create
	     * an IP packet to be send to the TCP/IP task.
	     */
	    while (sl_end != nil && ps_len[0] == 0) {
		unsigned char *sp= sl_buf[0];
		unsigned char *pp= ps_buf[0];

		while (sp < sl_end) {
		    int c= *sp++;

		    if (c == ESC) {
			switch (*sp++) {
			case ESC_ESC:	/* ESC ESC_ESC -> ESC. */
			    c= ESC;
			    break;
			case ESC_END:	/* ESC ESC_END -> END. */
			    c= END;
			    break;
			default:
			    /* Protocol error. */
			    discard= 1;
			}
		    }
		    if (pp < ps_buf[0] + PACKLEN) {
			*pp++ = c;
		    } else {
			/* Packet too big, discard. */
			discard= 1;
		    }
		}
		if (discard) {
		    discard= 0;
		} else {
		    /* A new packet can be send to the TCP/IP server. */
		    ps_len[0]= (pp - ps_buf[0]);
		}
		/* Move what's beyond END to the front. */
		sl_end++;
		sl_len[0] -= (sl_end - sl_buf[0]);
		memmove(sl_buf[0], sl_end, sl_len[0]);
		sl_end= memchr(sl_buf[0], END, sl_len[0]);
	    }

	    /* Reading from serial input. */
	    if (sl_end == nil && (HAS_ASYN || ps_len[0] == 0)) {
		r= asyn_read(&asyn, 0, sl_buf[0] + sl_len[0],
							SLIPLEN - sl_len[0]);
		if (r > 0) {
		    sl_end= memchr(sl_buf[0] + sl_len[0], END, r);
		    sl_len[0]+= r;
		    if (sl_end == nil && sl_len[0] == SLIPLEN) {
			/* Packet is idiotically big and no END in sight. */
			sl_len[0]= 0;
			discard= 1;
		    }
		} else
		if (r == 0) {
		    fprintf(stderr, "slip: EOF on serial input\n");
		    break;
		} else
		if (errno != ASYN_INPROGRESS) {
		    fprintf(stderr, "slip: serial input error: %s\n",
			strerror(errno));
		    break;
		}
	    }

	    /* Writing to the psip device. */
	    if (ps_len[0] > 0) {
		r= asyn_write(&asyn, ps_fd, ps_buf[0], ps_len[0]);
		if (r == ps_len[0]) {
		    /* Packet written. */
		    ps_len[0]= 0;
		} else
		if (r >= 0) {
		    fprintf(stderr,
			"slip: odd write to %s, tried %u, wrote %d\n",
			ps_device, (unsigned) ps_len[0], (int) r);
		    break;
		} else
		if (errno != ASYN_INPROGRESS) {
		    fprintf(stderr, "slip: error writing %s: %s\n",
			ps_device, strerror(errno));
		    break;
		}
	    }
	}

	if (doing[1]) {
	    /* Transform an IP packet to a "byte stuffed" serial packet. */
	    if (ps_len[1] > 0 && sl_len[1] == 0) {
		unsigned char *pp= ps_buf[1];
		unsigned char *sp= sl_buf[1];

		*sp++ = END;
		while (ps_len[1] > 0) {
		    int c= *pp++;
		    ps_len[1]--;
		    switch (c) {
		    case ESC:		/* ESC -> ESC ESC_ESC. */
			*sp++ = ESC;
			c= ESC_ESC;
			break;
		    case END:		/* END -> ESC ESC_END. */
			*sp++ = ESC;
			c= ESC_END;
			break;
		    }
		    *sp++ = c;
		}
		*sp++ = END;
		sl_len[1]= (sp - sl_buf[1]);
	    }

	    /* Reading from the psip device. */
	    if (ps_len[1] == 0 && (HAS_ASYN || sl_len[1] == 0)) {
		r= asyn_read(&asyn, ps_fd, ps_buf[1], PACKLEN);
		if (r > 0) {
		    /* One packet read. */
		    ps_len[1]= r;
		} else
		if (r == 0) {
		    fprintf(stderr, "slip: EOF on %s\n", ps_device);
		    break;
		} else
		if (errno != ASYN_INPROGRESS) {
		    fprintf(stderr, "slip: error reading %s: %s\n",
			ps_device, strerror(errno));
		    break;
		}
	    }

	    /* Writing to serial output. */
	    if (sl_len[1] > 0) {
		r= asyn_write(&asyn, 1, sl_buf[1], sl_len[1]);
		if (r > 0) {
		    if ((sl_len[1]-= r) > 0) {
			memmove(sl_buf[1], sl_buf[1] + r, sl_len[1]);
		    }
		} else
		if (r == 0) {
		    fprintf(stderr, "slip: EOF on serial output\n");
		    break;
		} else
		if (errno != ASYN_INPROGRESS) {
		    fprintf(stderr, "slip: serial output error: %s\n",
			strerror(errno));
		    break;
		}
	    }
	}

	/* Wait for something to happen. */
	if (asyn_wait(&asyn, 0, nil) < 0) {
	    fprintf(stderr,
		"slip: error while waiting for I/O to happen: %s\n",
		strerror(errno));
	    break;
	}
    }
#if !HAS_ASYN
    /* Tell my alter ego that the game is over. */
    kill(other_pid, SIGKILL);
#endif
    return 1;
}

static int fprintf(int fd, const char *format, ...)
/* Simple fprintf() to save a few bytes by not using the stdio library. */
{
    int len;
    ssize_t r;
    const char *fp0, *fp;
    va_list ap;

    len= 0;
    fp= fp0= format;
    va_start(ap, format);

    while (*fp != 0) {
	if (*fp == '%' && memchr("sdu", fp[1], 3) != nil) {
	    if (fp > fp0) {
		if ((r= write(fd, fp0, (fp - fp0))) < 0) goto error;
		len+= r;
	    }
	    fp++;
	    fp0= fp+1;

	    if (*fp == 's') {
		char *s= va_arg(ap, char *);

		if ((r= write(fd, s, strlen(s))) < 0) goto error;
		len+= r;
	    } else {
		int d;
		unsigned u;
		char a[3 * sizeof(u) + 2];
		char *p;

		if (*fp == 'd') {
		    u= d= va_arg(ap, int);
		    if (d < 0) u= -u;
		} else {
		    u= va_arg(ap, unsigned);
		    d= 0;
		}

		p= a + sizeof(a);
		*--p= 0;
		do *--p= '0' + (u % 10); while ((u /= 10) > 0);

		if (d < 0) *--p= '-';
		if ((r= write(fd, p, (a + sizeof(a)) - p)) < 0) goto error;
		len+= r;
	    }
	}
	fp++;
    }
    if (fp > fp0) {
	if ((r= write(fd, fp0, (fp - fp0))) < 0) goto error;
	len+= r;
    }
    va_end(ap);
    return len;
error:
    va_end(ap);
    return -1;
}
