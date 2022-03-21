#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "real_address.h"

const char * real_address(const char *address, struct sockaddr_in6 *rval)
{
    struct addrinfo* addressinfo;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET6;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_socktype = SOCK_DGRAM;

    int error = getaddrinfo(address, NULL, &hints, &addressinfo);
    if (error != 0){
        return strerror(error);
    }
    memcpy(rval, (struct sockaddr_in6*) addressinfo->ai_addr, addressinfo->ai_addrlen);
    return NULL;
}
