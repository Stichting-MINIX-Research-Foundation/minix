/* net.h Copyright 1992-2000 by Michael Temari All Rights Reserved
 *
 * This file is part of ftp.
 *
 *
 * 01/25/96 Initial Release	Michael Temari, <Michael@TemWare.Com>
 */

int NETinit(void);
int DOopen(void);
int DOclose(void);
int DOquit(void);
int DOdata(char *datacom, char *file, int direction, int fd);

extern int ftpcomm_fd;
