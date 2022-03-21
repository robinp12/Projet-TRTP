#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include "read_write_loop.h"




/* Loop reading a socket and printing to stdout,
 * while reading stdin and writing to the socket
 * @sfd: The socket file descriptor. It is both bound and connected.
 * @return: as soon as stdin signals EOF
 */
void read_write_loop(const int sfd)
{
    fd_set fdset;    // file descriptor set
    char buffer[1024];
    int error;
    int condition = 1;

    while (condition)
    {
        FD_ZERO(&fdset);               // reset
        FD_SET(sfd, &fdset);           // add sfd to the set
        FD_SET(STDIN_FILENO, &fdset);  // add stdin to the set
        ssize_t bytes_read;

        if (select(sfd+1, &fdset, NULL, NULL, 0) == -1){
            return;
        } else if (FD_ISSET(STDIN_FILENO, &fdset)) {

            bytes_read = read(STDIN_FILENO, buffer, 1024);
            if (bytes_read == 0){
                condition = 0;
            }

            error = write(sfd, buffer, bytes_read);
            if (error == -1){
                return;
            }

        } else if (FD_ISSET(sfd, &fdset)) {

            bytes_read = read(sfd, buffer, 1024);
            if (bytes_read == 0) {
                condition = 0;
            }

            error = write(STDOUT_FILENO, buffer, bytes_read);
            if (error == -1){
                return;
            }
        }
    }
}