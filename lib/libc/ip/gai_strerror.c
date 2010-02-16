#include <errno.h>
#include <netdb.h>
#include <stdio.h> 
#include <string.h>

/*
 * gai_strerror is based on
 * http://www.opengroup.org/onlinepubs/009695399/functions/gai_strerror.html 
 */
const char *gai_strerror(int ecode)
{
	static char buffer[256];

	/* check for each known error code */
	switch (ecode)
	{
		case EAI_AGAIN:
			return "The name could not be resolved at this time";

		case EAI_BADFLAGS:
			return "The flags had an invalid value";

		case EAI_FAIL:
			return "A non-recoverable error occurred";

		case EAI_FAMILY:
			return "The address family was not recognized or the "
				"address length was invalid for the specified "
				"family";

		case EAI_MEMORY:
			return "There was a memory allocation failure";

		case EAI_NONAME:
			return "The name does not resolve for the supplied "
				"parameters, NI_NAMEREQD is set and the host's "
				"name cannot be located, or both nodename and "
				"servname were null";

		case EAI_SERVICE:
			return "The service passed was not recognized for the "
				"specified socket type";

		case EAI_SOCKTYPE:
			return "The intended socket type was not recognized";

		case EAI_SYSTEM:
			snprintf(buffer, 
				sizeof(buffer), 
				"A system error occurred: %s",
				strerror(errno));
			return buffer;

		case EAI_OVERFLOW:
			return "An argument buffer overflowed";
	}

	/* unknown error code */
	snprintf(buffer, 
		sizeof(buffer), 
		"An unknown error code was passed to gai_strerror: %d",
		ecode);
	return buffer;
}
