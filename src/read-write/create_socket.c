

#include "create_socket.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "../log.h"

/* Creates a socket and initialize it
 * @source_addr: if !NULL, the source address that should be bound to this socket
 * @src_port: if >0, the port on which the socket is listening
 * @dest_addr: if !NULL, the destination address to which the socket should send data
 * @dst_port: if >0, the destination port to which the socket should be connected
 * @return: a file descriptor number representing the socket,
 *         or -1 in case of error (explanation will be printed on stderr)
 */
int create_socket(struct sockaddr_in6 *source_addr, int src_port, struct sockaddr_in6 *dest_addr, int dst_port)
{
    int skt = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    int error;
    if (skt == -1)
    {
        ERROR("Failed to create socket : %s", strerror(errno));
        return -1;
    }

    if (source_addr != NULL && src_port > 0)
    {
        source_addr->sin6_port = htons(src_port);
        source_addr->sin6_family = AF_INET6;
        error = bind(skt, (const struct sockaddr *)source_addr, sizeof(struct sockaddr_in6));
        if (error != 0)
        {
            ERROR("Failed to bind socket");
            return -1;
        }
    }

    if (dest_addr != NULL && dst_port > 0)
    {
        dest_addr->sin6_port = htons(dst_port);
        dest_addr->sin6_family = AF_INET6;
        error = connect(skt, (const struct sockaddr *)dest_addr, sizeof(struct sockaddr_in6));
        if (error != 0)
        {
            return -1;
        }
    }
    return skt;
}