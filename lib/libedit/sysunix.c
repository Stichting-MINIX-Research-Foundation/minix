/*
**  Unix system-dependant routines for editline library.
*/
#include "editline.h"

#if	defined(HAVE_TCGETATTR)
#include <termios.h>

void
rl_ttyset(Reset)
    int				Reset;
{
    static struct termios	old;
    struct termios		new;

    if (Reset == 0) {
	(void)tcgetattr(0, &old);
	rl_erase = old.c_cc[VERASE];
	rl_kill = old.c_cc[VKILL];
	rl_eof = old.c_cc[VEOF];
	rl_intr = old.c_cc[VINTR];
	rl_quit = old.c_cc[VQUIT];

	new = old;
	new.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
	new.c_iflag &= ~(ICRNL);
	new.c_cc[VMIN] = 1;
	new.c_cc[VTIME] = 0;
	(void)tcsetattr(0, TCSADRAIN, &new);
    }
    else
	(void)tcsetattr(0, TCSADRAIN, &old);
}

#else
#if	defined(HAVE_TERMIO)
#include <termio.h>

void
rl_ttyset(Reset)
    int				Reset;
{
    static struct termio	old;
    struct termio		new;

    if (Reset == 0) {
	(void)ioctl(0, TCGETA, &old);
	rl_erase = old.c_cc[VERASE];
	rl_kill = old.c_cc[VKILL];
	rl_eof = old.c_cc[VEOF];
	rl_intr = old.c_cc[VINTR];
	rl_quit = old.c_cc[VQUIT];

	new = old;
	new.c_cc[VINTR] = -1;
	new.c_cc[VQUIT] = -1;
	new.c_lflag &= ~(ECHO | ICANON);
	new.c_cc[VMIN] = 1;
	new.c_cc[VTIME] = 0;
	(void)ioctl(0, TCSETAW, &new);
    }
    else
	(void)ioctl(0, TCSETAW, &old);
}

#else
#include <sgtty.h>

void
rl_ttyset(Reset)
    int				Reset;
{
    static struct sgttyb	old_sgttyb;
    static struct tchars	old_tchars;
    struct sgttyb		new_sgttyb;
    struct tchars		new_tchars;

    if (Reset == 0) {
	(void)ioctl(0, TIOCGETP, &old_sgttyb);
	rl_erase = old_sgttyb.sg_erase;
	rl_kill = old_sgttyb.sg_kill;

	(void)ioctl(0, TIOCGETC, &old_tchars);
	rl_eof = old_tchars.t_eofc;
	rl_intr = old_tchars.t_intrc;
	rl_quit = old_tchars.t_quitc;

	new_sgttyb = old_sgttyb;
	new_sgttyb.sg_flags &= ~ECHO;
	new_sgttyb.sg_flags |= RAW;
	(void)ioctl(0, TIOCSETP, &new_sgttyb);

	new_tchars = old_tchars;
	new_tchars.t_intrc = -1;
	new_tchars.t_quitc = -1;
	(void)ioctl(0, TIOCSETC, &new_tchars);
    }
    else {
	(void)ioctl(0, TIOCSETP, &old_sgttyb);
	(void)ioctl(0, TIOCSETC, &old_tchars);
    }
}
#endif	/* defined(HAVE_TERMIO) */
#endif	/* defined(HAVE_TCGETATTR) */

void
rl_add_slash(path, p)
    char	*path;
    char	*p;
{
    struct stat	Sb;

    if (stat(path, &Sb) >= 0)
	(void)strcat(p, S_ISDIR(Sb.st_mode) ? "/" : " ");
}

/*
 * $PchId: sysunix.c,v 1.4 1996/02/22 21:16:56 philip Exp $
 */
