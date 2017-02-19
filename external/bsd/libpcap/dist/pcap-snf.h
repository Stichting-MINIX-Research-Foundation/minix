/*	$NetBSD: pcap-snf.h,v 1.2 2014/11/19 19:33:30 christos Exp $	*/

pcap_t *snf_create(const char *, char *, int *);
int snf_findalldevs(pcap_if_t **devlistp, char *errbuf);
