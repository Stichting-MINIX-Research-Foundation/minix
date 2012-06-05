#ifndef _DEVMAND_PROTO_H
#define _DEVMAND_PROTO_H

/* main.c */
struct devmand_usb_driver * add_usb_driver(char *name);
struct devmand_usb_match_id * add_usb_match_id();

/* y.tab.c */
int yyparse();

#endif
