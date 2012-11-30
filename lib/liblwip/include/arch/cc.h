#ifndef __LWIP_CC_H__
#define __LWIP_CC_H__

#include <sys/cdefs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <assert.h>

typedef i8_t s8_t;
typedef i16_t s16_t;
typedef i32_t s32_t;

#define U16_F	"d"
#define S16_F	"d"
#define X16_F	"x"
#define U32_F	"d"
#define S32_F	"d"
#define X32_F	"x"
#define SZT_F 	"uz"

#define PACK_STRUCT_STRUCT __packed

#define LWIP_PLATFORM_DIAG(x) printf x
#define LWIP_PLATFORM_ASSERT(x) panic(x)

typedef u32_t mem_ptr_t;

#define ENSROK                  (_SIGN 0) /* DNS server returned answer with no data */
#define ENSRNODATA              (_SIGN 160) /* DNS server returned answer with no data */
#define ENSRFORMERR             (_SIGN 161) /* DNS server claims query was misformatted */
#define ENSRSERVFAIL            (_SIGN 162) /* DNS server returned general failure */
#define ENSRNOTFOUND            (_SIGN 163) /* Domain name not found */
#define ENSRNOTIMP              (_SIGN 164) /* DNS server does not implement requested operation */
#define ENSRREFUSED             (_SIGN 165) /* DNS server refused query */
#define ENSRBADQUERY            (_SIGN 166) /* Misformatted DNS query */
#define ENSRBADNAME             (_SIGN 167) /* Misformatted domain name */
#define ENSRBADFAMILY           (_SIGN 168) /* Unsupported address family */
#define ENSRBADRESP             (_SIGN 169) /* Misformatted DNS reply */
#define ENSRCONNREFUSED         (_SIGN 170) /* Could not contact DNS servers */
#define ENSRTIMEOUT             (_SIGN 171) /* Timeout while contacting DNS servers */
#define ENSROF                  (_SIGN 172) /* End of file */
#define ENSRFILE                (_SIGN 173) /* Error reading file */
#define ENSRNOMEM               (_SIGN 174) /* Out of memory */
#define ENSRDESTRUCTION         (_SIGN 175) /* Application terminated lookup */
#define ENSRQUERYDOMAINTOOLONG  (_SIGN 176) /* Domain name is too long */
#define ENSRCNAMELOOP           (_SIGN 177) /* Domain name is too long */

#endif /* __LWIP_CC_H__ */
