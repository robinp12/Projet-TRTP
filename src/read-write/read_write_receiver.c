#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <string.h>
#include <sys/socket.h>

#include "read_write_loop.h"
#include "../packet_interface.h"
#include "../log.h"

char* window;

int read_write_receiver(const int sfd, const int fd)
{
    char* buffpacket = malloc(sizeof(PKT_MAX_LEN));
    char *buffer = malloc(MAX_PAYLOAD_SIZE + 16);
    pkt_t *pkt = pkt_new();

    int rc = 1;
    struct pollfd *fds = malloc(sizeof(*fds));
    int ndfs = 1;
    

    memset(fds, 0, sizeof(fds));

    int condition = 1;
    while(condition){
        fds[0].fd = sfd;
        fds[0].events = POLLIN;
        int rep = poll(fds, ndfs, -1);
        if(rep < 0){
            printf("error poll");
            return EXIT_FAILURE;
        }
        if(fds[0].revents & POLLIN){
            rc = recv(sfd, buffer, MAX_PAYLOAD_SIZE, 0);
            if(rc < 0){
                printf("error recv");
                return EXIT_FAILURE;
            }
        }
        printf("buffer : %s \n",buffer);
        condition = 0;
    }
    
    free(buffer);
    free(buffpacket);
}