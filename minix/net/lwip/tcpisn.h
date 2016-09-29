#ifndef MINIX_NET_LWIP_TCPISN_H
#define MINIX_NET_LWIP_TCPISN_H

/*
 * Length, in bytes, of the secret (random seed) that is used as part of the
 * input to the hashing function that generates TCP Initial Sequence Numbers.
 */
#define TCPISN_SECRET_LENGTH		16

/*
 * Size of the hexadecimal-string representation of the secret, including
 * trailing null terminator.
 */
#define TCPISN_SECRET_HEX_LENGTH	(TCPISN_SECRET_LENGTH * 2 + 1)

void tcpisn_init(void);
ssize_t tcpisn_secret(struct rmib_call * call, struct rmib_node * node,
	struct rmib_oldp * oldp, struct rmib_newp * newp);

#endif /* !MINIX_NET_LWIP_TCPISN_H */
