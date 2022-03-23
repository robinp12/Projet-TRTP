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

int read_write_receiver(const int sfd)
{
    char* buffpacket = malloc(sizeof(PKT_MAX_LEN));
    char *buffer = malloc(MAX_PAYLOAD_SIZE + 16);
    DEBUG("Dump fd : %d", fd);

    int rc = 1;
    struct pollfd *fds = malloc(sizeof(*fds));
    int ndfs = 1;
    

    memset(fds, 0, sizeof(struct pollfd));

    int condition = 1;
    fds[0].fd = sfd;
    fds[0].events = POLLIN;

    while(condition){
        
        int rep = poll(fds, ndfs, -1);
        if(rep < 0){
            printf("error poll");
            return EXIT_FAILURE;
        }
        if(fds[0].revents & POLLIN){
            
            DEBUG("Read");
            rc = read(sfd, buffer, PKT_MAX_LEN);
            pkt_t* pkt = pkt_new();
            pkt_decode(buffer, rc, pkt);
            const char* payload = pkt_get_payload(pkt);
            ASSERT(payload != NULL);
            
            printf("%s", payload);
            pkt_del(pkt);
            
            if (pkt_get_length(pkt) != MAX_PAYLOAD_SIZE){
                DEBUG("End of file");
                condition = 0;
            }
        }
        
    }
    
    DEBUG("Frees")
    free(buffer);
    free(buffpacket);
    return 0;
}