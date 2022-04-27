#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#include "wait_for_client.h"
#include "../log.h"


/* Block the caller until a message is received on sfd,
 * and connect the socket to the source addresse of the received message.
 * @sfd: a file descriptor to a bound socket but not yet connected
 * @return: 0 in case of success, -1 otherwise
 * @POST: This call is idempotent, it does not 'consume' the data of the message,
 * and could be repeated several times blocking only at the first call.
 */
int wait_for_client(int sfd)
{
    if (sfd < 0){
        return -1;
    }
    int error;
    char buffer[1024];
    
    struct sockaddr_in6 address;
    socklen_t size = sizeof(struct sockaddr_in6);

    memset(&address, 0, size);

    struct pollfd *fds = malloc(sizeof(struct pollfd));
    const int nfds = 1;
    const int timeout = 10000;

    memset(fds, 0, sizeof(struct pollfd));

    fds[0].fd = sfd;
    fds[0].events = POLLIN;

    int retval = poll(fds, nfds, timeout); // Set a 10 second timeout
    free(fds);

    if (retval == -1)
    {
        ERROR("Poll in wait_for_client failed : %s", strerror(errno));
        return -1;
    }
    if (retval == 0)
    {
        ERROR("Poll timed out in wait_for_client");
        return -1;
    }


    error = recvfrom(sfd, buffer, 1024, MSG_PEEK, (struct sockaddr *) &address, &size);
    if (error == -1){
        return -1;
    }

    error = connect(sfd, (struct sockaddr *) &address, size);
    if (error == -1){
        return -1;
    }
    return 0;

}