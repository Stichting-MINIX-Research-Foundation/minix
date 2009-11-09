/* screen.h Copyright Michael Temari 08/01/1996 All Rights Reserved */

_PROTOTYPE(int ScreenInit, (void));
_PROTOTYPE(void ScreenMsg, (char *msg));
_PROTOTYPE(void ScreenWho, (char *user, char *host));
_PROTOTYPE(void ScreenEdit, (char lcc[], char rcc[]));
_PROTOTYPE(void ScreenPut, (char *data, int len, int mywin));
_PROTOTYPE(void ScreenEnd, (void));

extern int ScreenDone;

#define	LOCALWIN	0
#define	REMOTEWIN	1
