#ifndef _SYS_TERMIOS_H_
#define _SYS_TERMIOS_H_

#include <sys/cdefs.h>

#include <minix/termios.h>

__BEGIN_DECLS
#if defined(_NETBSD_SOURCE)
void	cfmakeraw(struct termios *);
int	cfsetspeed(struct termios *, speed_t);
#endif /* defined(_NETBSD_SOURCE) */
__END_DECLS

#endif /* !_SYS_TERMIOS_H_ */

#include <sys/ttydefaults.h>
