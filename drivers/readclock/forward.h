#ifndef __FORWARD_H
#define __FORWARD_H

int fwd_set_label(char *label);
int fwd_init(void);
int fwd_get_time(struct tm *t, int flags);
int fwd_set_time(struct tm *t, int flags);
int fwd_pwr_off(void);
void fwd_exit(void);

#endif /* __FORWARD_H */
