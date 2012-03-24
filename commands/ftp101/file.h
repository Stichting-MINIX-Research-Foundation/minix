/* file.h Copyright 1992-2000 by Michael Temari All Rights Reserved
 *
 * This file is part of ftp.
 *
 *
 * 01/25/96 Initial Release	Michael Temari, <Michael@TemWare.Com>
 */

int recvfile(int fd, int fdin);
int sendfile(int fd, int fdout);
int DOascii(void);
int DObinary(void);
int DOblock(void);
int DOstream(void);
int DOpwd(void);
int DOcd(void);
int DOmkdir(void);
int DOrmdir(void);
int DOdelete(void);
int DOmdtm(void);
int DOsize(void);
int DOstat(void);
int DOlist(void);
int DOnlst(void);
int DOretr(void);
int DOrretr(void);
int DOMretr(void);
int DOappe(void);
int DOstor(void);
int DOrstor(void);
int DOstou(void);
int DOMstor(void);
int DOclone(void);
