/*
ttn.h
*/

#ifndef TTN_H
#define TTN_H

#define IAC		255
#define IAC_SE		240
#define IAC_NOP		241
#define IAC_DataMark	242
#define IAC_BRK		243
#define IAC_IP		244
#define IAC_AO		245
#define IAC_AYT		246
#define IAC_EC		247
#define IAC_EL		248
#define IAC_GA		249
#define IAC_SB		250
#define IAC_WILL	251
#define IAC_WONT	252
#define IAC_DO		253
#define IAC_DONT	254

#define OPT_ECHO	1
#define OPT_SUPP_GA	3
#define OPT_TERMTYPE	24

#define TERMTYPE_SEND	1
#define TERMTYPE_IS	0

#define FALSE	0
#define TRUE	(!(FALSE))

extern int DO_echo;
extern int DO_echo_allowed;
extern int WILL_terminal_type;
extern int WILL_terminal_type_allowed;
extern int DO_suppress_go_ahead;
extern int DO_suppress_go_ahead_allowed;
extern int WILL_suppress_go_ahead;
extern int WILL_suppress_go_ahead_allowed;

#endif /* TTN_H */
