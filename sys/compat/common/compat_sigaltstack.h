/*      $NetBSD: compat_sigaltstack.h,v 1.3 2011/06/05 09:37:10 dsl Exp $        */

/* Wrapper for calling sigaltstack1() from compat (or other) code */

/* Maybe these definitions could be global. */
#ifdef SCARG_P32
/* compat32 */
#define	SCARG_COMPAT_PTR(uap,p)	SCARG_P32(uap, p)
#define	COMPAT_GET_PTR(p)	NETBSD32PTR64(p)
#define	COMPAT_SET_PTR(p, v)	NETBSD32PTR32(p, v)
#else
/* not a size change */
#define	SCARG_COMPAT_PTR(uap,p)	SCARG(uap, p)
#define	COMPAT_GET_PTR(p)	(p)
#define	COMPAT_SET_PTR(p, v)	((p) = (v))
#endif

#define compat_sigaltstack(uap, compat_ss, ss_onstack, ss_disable) do { \
	struct compat_ss css; \
	struct sigaltstack nss, oss; \
	int error; \
\
	if (SCARG_COMPAT_PTR(uap, nss)) { \
		error = copyin(SCARG_COMPAT_PTR(uap, nss), &css, sizeof css); \
		if (error) \
			return error; \
		nss.ss_sp = COMPAT_GET_PTR(css.ss_sp); \
		nss.ss_size = css.ss_size; \
		if (ss_onstack == SS_ONSTACK && ss_disable == SS_DISABLE) \
			nss.ss_flags = css.ss_flags; \
		else \
			nss.ss_flags = \
			    (css.ss_flags & ss_onstack ? SS_ONSTACK : 0) \
			    | (css.ss_flags & ss_disable ? SS_DISABLE : 0); \
	} \
\
	error = sigaltstack1(curlwp, SCARG_COMPAT_PTR(uap, nss) ? &nss : 0, \
				SCARG_COMPAT_PTR(uap, oss) ? &oss : 0); \
	if (error) \
		return (error); \
\
	if (SCARG_COMPAT_PTR(uap, oss)) { \
		COMPAT_SET_PTR(css.ss_sp, oss.ss_sp); \
		css.ss_size = oss.ss_size; \
		if (ss_onstack == SS_ONSTACK && ss_disable == SS_DISABLE) \
			css.ss_flags = oss.ss_flags; \
		else \
			css.ss_flags = \
			    (oss.ss_flags & SS_ONSTACK ? ss_onstack : 0) \
			    | (oss.ss_flags & SS_DISABLE ? ss_disable : 0); \
		error = copyout(&css, SCARG_COMPAT_PTR(uap, oss), sizeof(css));\
		if (error) \
			return (error); \
	} \
	return (0); \
} while (0)
