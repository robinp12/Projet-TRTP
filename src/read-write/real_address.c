#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "real_address.h"

const char *real_address(const char *address, struct sockaddr_in6 *rval)
{
    struct addrinfo *addressinfo;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET6;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_socktype = SOCK_DGRAM;

    memset(rval, 0, sizeof(*rval));

    int retval = getaddrinfo(address, NULL, &hints, &addressinfo);
    if (retval != 0)
    {
        return gai_strerror(retval);
    }
    memcpy(rval, (struct sockaddr_in6 *)addressinfo->ai_addr, addressinfo->ai_addrlen);
    freeaddrinfo(addressinfo);
    return NULL;
}
