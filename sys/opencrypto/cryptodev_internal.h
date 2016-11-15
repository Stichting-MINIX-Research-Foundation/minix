/* $NetBSD: cryptodev_internal.h,v 1.1 2011/02/19 16:26:34 drochner Exp $ */

/* exported to compat code, not for consumers */

struct csession;
int cryptodev_op(struct csession *, struct crypt_op *, struct lwp *);
int cryptodev_mop(struct fcrypt *, struct crypt_n_op *, int, struct lwp *);
int cryptodev_session(struct fcrypt *, struct session_op *);
int cryptodev_msession(struct fcrypt *, struct session_n_op *, int);
struct csession *cryptodev_csefind(struct fcrypt *fcr, u_int ses);
