#ifndef _NETSTAT_VTW_H
#define _NETSTAT_VTW_H

void show_vtw_stats(void);
void show_vtw_v4(void (*)(const vtw_t *));
void show_vtw_v6(void (*)(const vtw_t *));
void timebase(struct timeval *);

#endif /* _NETSTAT_VTW_H */
