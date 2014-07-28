/* VirtualBox driver - by D.C. van Moolenbroek */
#include <minix/drivers.h>
#include <errno.h>

#include "vmmdev.h"
#include "proto.h"

static int codes[] = {
  OK,			/*     0: success */
  EGENERIC,		/*    -1: general failure */
  EINVAL,		/*    -2: invalid parameter */
  EINVAL,		/*    -3: invalid magic */
  EBADF,		/*    -4: invalid handle */
  ENOLCK,		/*    -5: lock failed */
  EFAULT,		/*    -6: invalid pointer */
  EGENERIC,		/*    -7: patching IDT failed */
  ENOMEM,		/*    -8: memory allocation failed */
  EEXIST,		/*    -9: already loaded */
  EPERM,		/*   -10: permission denied */
  EINVAL,		/*   -11: version mismatch */
  ENOSYS,		/*   -12: function not implemented */
  EGENERIC,		/*   -13 */
  EGENERIC,		/*   -14 */
  EGENERIC,		/*   -15 */
  EGENERIC,		/*   -16 */
  EGENERIC,		/*   -17 */
  EGENERIC,		/*   -18: not equal */
  EINVAL,		/*   -19: not a symlink */
  ENOMEM,		/*   -20: temporary memory allocation failed */
  EINVAL,		/*   -21: invalid file mode */
  EINVAL,		/*   -22: incorrect call order */
  EGENERIC,		/*   -23: no TLS available */
  EGENERIC,		/*   -24: failed to set TLS */
  EGENERIC,		/*   -25 */
  ENOMEM,		/*   -26: contiguous memory allocation failed */
  ENOMEM,		/*   -27: no memory available for page table */
  EGENERIC,		/*   -28 */
  ESRCH,		/*   -29: thread is dead */
  EINVAL,		/*   -30: thread is not waitable */
  EGENERIC,		/*   -31: page table not present */
  EINVAL,		/*   -32: invalid context */
  EBUSY,		/*   -33: timer is busy */
  EGENERIC,		/*   -34: address conflict */
  EGENERIC,		/*   -35: unresolved error */
  ENOTTY,		/*   -36: invalid function */
  EINVAL,		/*   -37: not supported */
  EACCES,		/*   -38: access denied */
  EINTR,		/*   -39: interrupted */
  ETIMEDOUT,		/*   -40: timeout */
  E2BIG,		/*   -41: buffer overflow */
  E2BIG,		/*   -42: too much data */
  EAGAIN,		/*   -43: max thread number reached */
  EAGAIN,		/*   -44: max process number reached */
  EGENERIC,		/*   -45: signal refused */
  EBUSY,		/*   -46: signal already pending */
  EINVAL,		/*   -47: invalid signal */
  EGENERIC,		/*   -48: state changed */
  EINVAL,		/*   -49: invalid UUID format */
  ESRCH,		/*   -50: process not found */
  OK,			/*   -51: waited-for process is still running */
  EAGAIN,		/*   -52: try again */
  EGENERIC,		/*   -53: generic parse error */
  ERANGE,		/*   -54: value out of range */
  EOVERFLOW,		/*   -55: value too big */
  EGENERIC,		/*   -56: no digits in string */
  ERANGE,		/*   -57: minus sign in unsigned value */
  EILSEQ,		/*   -58: character translation failed */
  EGENERIC,		/*   -59: encountered unicode byte order mark */
  EGENERIC,		/*   -60: encountered unicode surrogate */
  EILSEQ,		/*   -61: invalid UTF8 encoding */
  EILSEQ,		/*   -62: invalid UTF16 encoding */
  EGENERIC,		/*   -63: no UTF16 for character */
  ENOMEM,		/*   -64: string memory allocation failed */
  ENOMEM,		/*   -65: UTF16 string memory allocation failed */
  ENOMEM,		/*   -66: code point array allocation failed */
  EBUSY,		/*   -67: cannot free in-use memory */
  EGENERIC,		/*   -68: timer already active */
  EGENERIC,		/*   -69: timer already suspended */
  ECANCELED,		/*   -70: operation cancelled */
  ENOMEM,		/*   -71: failed to initialize memory object */
  ENOMEM,		/*   -72: low physical memory allocation failed */
  ENOMEM,		/*   -73: physical memory allocation failed */
  EGENERIC,		/*   -74: address too big */
  EGENERIC,		/*   -75: memory mapping failed */
  EGENERIC,		/*   -76: trailing characters */
  EGENERIC,		/*   -77: trailing spaces */
  ESRCH,		/*   -78: not found */
  EGENERIC,		/*   -79: invalid state */
  ENOMEM,		/*   -80: out of resources */
  ENFILE,		/*   -81: no more handles */
  EGENERIC,		/*   -82: preemption disabled */
  EGENERIC,		/*   -83: end of string */
  EGENERIC,		/*   -84: page count out of range */
  EGENERIC,		/*   -85: object destroyed */
  EGENERIC,		/*   -86: dangling objects */
  EGENERIC,		/*   -87: invalid Base64 encoding */
  EGENERIC,		/*   -88: return triggered by callback */
  EGENERIC,		/*   -89: authentication failure */
  EGENERIC,		/*   -90: not a power of two */
  EGENERIC,		/*   -91: ignored */
  EGENERIC,		/*   -92: concurrent access not allowed */
  EGENERIC,		/*   -93: invalid reference sharing */
  EGENERIC,		/*   -94 */
  EGENERIC,		/*   -95: no change */
  ENOMEM,		/*   -96: executable memory allocation failed */
  EINVAL,		/*   -97: unsupported alignment */
  EGENERIC,		/*   -98: duplicate */
  EGENERIC,		/*   -99: missing */
  EIO,			/*  -100: I/O error */
  ENXIO,		/*  -101: open failed */
  ENOENT,		/*  -102: file not found */
  ENOTDIR,		/*  -103: path not found (may also mean ENOENT) */
  EINVAL,		/*  -104: invalid name */
  EEXIST,		/*  -105: already exists */
  ENFILE,		/*  -106: too many open files */
  EIO,			/*  -107: seek error */
  EINVAL,		/*  -108: negative seek */
  ESPIPE,		/*  -109: seek on device */
  EGENERIC,		/*  -110: end of file */
  EIO,			/*  -111: generic read error */
  EIO,			/*  -112: generic write error */
  EROFS,		/*  -113: write protected */
  ETXTBSY,		/*  -114: sharing violation */
  ENOLCK,		/*  -115: file lock failed */
  EAGAIN,		/*  -116: file lock violation */
  EIO,			/*  -117: cannot create file */
  EIO,			/*  -118: cannot delete directory */
  EXDEV,		/*  -119: not the same device */
  ENAMETOOLONG,		/*  -120: file name too long */
  ENXIO,		/*  -121: media not present */
  EIO,			/*  -122: media not recognized */
  OK,			/*  -123: nothing to unlocked */
  EGENERIC,		/*  -124: lock lost */
  ENOTEMPTY,		/*  -125: directory not empty */
  ENOTDIR,		/*  -126: not a directory */
  EISDIR,		/*  -127: is a directory */
  EFBIG,		/*  -128: file too big */
  EGENERIC,		/*  -129: no asynchronous I/O request */
  EGENERIC,		/*  -130: asynchronous I/O in progress */
  EGENERIC,		/*  -131: asynchronous I/O completed */
  EGENERIC,		/*  -132: asynchronous I/O busy */
  EGENERIC,		/*  -133: asynchronous I/O limit exceeded */
  EGENERIC,		/*  -134: asynchronous I/O canceled */
  EGENERIC,		/*  -135: asynchronous I/O not submitted */
  EGENERIC,		/*  -136: asynchronous I/O not prepared */
  EGENERIC,		/*  -137: asynchronous I/O out of resources */
  EBUSY,		/*  -138: device or resource busy */
  EGENERIC,		/*  -139: not a file */
  EGENERIC,		/*  -140: is a file */
  EGENERIC,		/*  -141: unexpected file type */
  EGENERIC,		/*  -142: missing path root specification */
  EGENERIC,		/*  -143: path is relative */
  EGENERIC,		/*  -144: path is not relative */
  EGENERIC,		/*  -145 */
  EGENERIC,		/*  -146 */
  EGENERIC,		/*  -147 */
  EGENERIC,		/*  -148 */
  EGENERIC,		/*  -149 */
  EIO,			/*  -150: disk I/O error */
  ENXIO,		/*  -151: invalid drive number */
  ENOSPC,		/*  -152: disk full */
  EIO,			/*  -153: disk changed */
  EGENERIC,		/*  -154: drive locked */
  ENXIO,		/*  -155: invalid disk format */
  ELOOP,		/*  -156: too many symlinks */
  EOPNOTSUPP,		/*  -157: can not set symlink file times */
  EOPNOTSUPP,		/*  -158: can not change symlink owner */
  EGENERIC,		/*  -159 */
  EGENERIC,		/*  -160 */
  EGENERIC,		/*  -161 */
  EGENERIC,		/*  -162 */
  EGENERIC,		/*  -163 */
  EGENERIC,		/*  -164 */
  EGENERIC,		/*  -165 */
  EGENERIC,		/*  -166 */
  EGENERIC,		/*  -167 */
  EGENERIC,		/*  -168 */
  EGENERIC,		/*  -169 */
  EGENERIC,		/*  -170 */
  EGENERIC,		/*  -171 */
  EGENERIC,		/*  -172 */
  EGENERIC,		/*  -173 */
  EGENERIC,		/*  -174 */
  EGENERIC,		/*  -175 */
  EGENERIC,		/*  -176 */
  EGENERIC,		/*  -177 */
  EGENERIC,		/*  -178 */
  EGENERIC,		/*  -179 */
  EGENERIC,		/*  -180 */
  EGENERIC,		/*  -181 */
  EGENERIC,		/*  -182 */
  EGENERIC,		/*  -183 */
  EGENERIC,		/*  -184 */
  EGENERIC,		/*  -185 */
  EGENERIC,		/*  -186 */
  EGENERIC,		/*  -187 */
  EGENERIC,		/*  -188 */
  EGENERIC,		/*  -189 */
  EGENERIC,		/*  -190 */
  EGENERIC,		/*  -191 */
  EGENERIC,		/*  -192 */
  EGENERIC,		/*  -193 */
  EGENERIC,		/*  -194 */
  EGENERIC,		/*  -195 */
  EGENERIC,		/*  -196 */
  EGENERIC,		/*  -197 */
  EGENERIC,		/*  -198 */
  EGENERIC,		/*  -199 */
  EGENERIC,		/*  -200: search error */
  OK,			/*  -201: no more files */
  ENFILE,		/*  -202: no more search handles available */
};

int convert_err(int code)
{
/* Return a POSIX error code for the given VirtualBox error code.
 */
  unsigned int index;

  index = -code;

  if (index < sizeof(codes) / sizeof(codes[0]))
	return codes[index];

  return EGENERIC;
}
