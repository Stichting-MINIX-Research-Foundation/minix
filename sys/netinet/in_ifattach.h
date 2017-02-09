#ifndef _NETINET_IN_IFATTACH_H_
#define	_NETINET_IN_IFATTACH_H_

void	in_domifdetach(struct ifnet *, void *);
void	*in_domifattach(struct ifnet *);

#endif /* !_NETINET_IN_IFATTACH_H_ */
