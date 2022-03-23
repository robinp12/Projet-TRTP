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
char* buffer;

int send_ack(const int sfd, uint8_t seqnum)
{
    pkt_t* pkt = pkt_new();
    int retval = pkt_set_type(pkt, PTYPE_ACK);
    if (retval != PKT_OK){
        ERROR("Set type failed : %d", retval);
        return -1;
    }

    retval = pkt_set_seqnum(pkt, seqnum);
    if (retval != PKT_OK){
        ERROR("Seqnum failed : %d", retval);
        return -1;
    }

    size_t len = PKT_MAX_LEN;
    DEBUG("Ecoding pkt");
    retval = pkt_encode(pkt, buffer, &len);
    if (retval != PKT_OK){
        ERROR("Encode failed : %d", retval);
        return -1;
    }

    DEBUG("Writing %zu bytes on socket", len);
    fflush(stdout);
    write(sfd, buffer, len);
    DEBUG("ack %d sent", seqnum);
    pkt_del(pkt);
    return 0;
}

int read_write_receiver(const int sfd)
{
    buffer = malloc(PKT_MAX_LEN);

    int rc = 1;
    struct pollfd *fds = malloc(sizeof(*fds));
    int ndfs = 1;
    

    memset(fds, 0, sizeof(struct pollfd));

    int condition = 1;
    fds[0].fd = sfd;
    fds[0].events = POLLIN | POLLOUT;
    uint8_t lastSeqnum = 0;

    while(condition){
        
        int rep = poll(fds, ndfs, -1);
        if(rep < 0){
            ERROR("error poll");
            return EXIT_FAILURE;
        }
        if(fds[0].revents & POLLIN){
            
            DEBUG("Read");
            rc = read(sfd, buffer, PKT_MAX_LEN);
            pkt_t* pkt = pkt_new();
            pkt_decode(buffer, rc, pkt);
            // const char* payload = pkt_get_payload(pkt);
            // ASSERT(payload != NULL);
            
            // printf("%s", payload);
            lastSeqnum = pkt_get_seqnum(pkt);
            pkt_del(pkt);
            
            if (pkt_get_length(pkt) != MAX_PAYLOAD_SIZE){
                DEBUG("End of file");
                condition = 0;
            }
        } else if (fds[0].revents & POLLOUT){
            send_ack(sfd, lastSeqnum);
        }
        
    }
    
    DEBUG("Frees");
    free(buffer);
    return 0;
}