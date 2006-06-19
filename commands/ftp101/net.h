/* net.h Copyright 1992-2000 by Michael Temari All Rights Reserved
 *
 * This file is part of ftp.
 *
 *
 * 01/25/96 Initial Release	Michael Temari, <Michael@TemWare.Com>
 */

_PROTOTYPE(int NETinit, (void));
_PROTOTYPE(int DOopen, (void));
_PROTOTYPE(int DOclose, (void));
_PROTOTYPE(int DOquit, (void));
_PROTOTYPE(int DOdata, (char *datacom, char *file, int direction, int fd));

extern int ftpcomm_fd;
