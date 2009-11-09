/* file.h
 *
 * This file is part of ftp.
 *
 *
 * 01/25/96 Initial Release	Michael Temari, <temari@ix.netcom.com>
 */

_PROTOTYPE(int recvfile, (int fd, int fdin));
_PROTOTYPE(int sendfile, (int fd, int fdout));
_PROTOTYPE(int DOascii, (void));
_PROTOTYPE(int DObinary, (void));
_PROTOTYPE(int DOpwd, (void));
_PROTOTYPE(int DOcd, (void));
_PROTOTYPE(int DOmkdir, (void));
_PROTOTYPE(int DOrmdir, (void));
_PROTOTYPE(int DOdelete, (void));
_PROTOTYPE(int DOmdtm, (void));
_PROTOTYPE(int DOsize, (void));
_PROTOTYPE(int DOstat, (void));
_PROTOTYPE(int DOlist, (void));
_PROTOTYPE(int DOnlst, (void));
_PROTOTYPE(int DOretr, (void));
_PROTOTYPE(int DOrretr, (void));
_PROTOTYPE(int DOMretr, (void));
_PROTOTYPE(int DOappe, (void));
_PROTOTYPE(int DOstor, (void));
_PROTOTYPE(int DOrstor, (void));
_PROTOTYPE(int DOstou, (void));
_PROTOTYPE(int DOMstor, (void));
