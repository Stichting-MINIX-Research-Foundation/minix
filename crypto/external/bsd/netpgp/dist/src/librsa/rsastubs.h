#ifndef RSASTUBS_H_
#define RSASTUBS_H_	20120412

#include "rsa.h"

#ifndef __BEGIN_DECLS
#  if defined(__cplusplus)
#  define __BEGIN_DECLS           extern "C" {
#  define __END_DECLS             }
#  else
#  define __BEGIN_DECLS
#  define __END_DECLS
#  endif
#endif

__BEGIN_DECLS

typedef int pem_password_cb(char */*buf*/, int /*size*/, int /*rwflag*/, void */*userdata*/);

RSA *PEM_read_RSAPrivateKey(FILE *fp, RSA **x, pem_password_cb *cb, void *u);
DSA *PEM_read_DSAPrivateKey(FILE *fp, DSA **x, pem_password_cb *cb, void *u);

__END_DECLS

#endif
