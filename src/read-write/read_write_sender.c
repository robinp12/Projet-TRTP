#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>

#include "read_write_loop.h"
#include "../packet_interface.h"
#include "../log.h"

int windowsize = 1;
char* window;

void read_write_sender(const int sfd, const int fd)
{
    fd_set fdset;    // file descriptor set
    int error;
    int condition = 1;
    char* buffpacket = malloc(sizeof(PKT_MAX_LEN));
    char* buffer = malloc(sizeof(char)*MAX_PAYLOAD_SIZE);
    pkt_t* pkt = pkt_new();

    int sfdmax;
    if (sfd > fd){
        sfdmax = sfd + 1;
    } else {
        sfdmax = fd + 1;
    }

    while (condition)
    {
        FD_ZERO(&fdset);               // reset
        FD_SET(sfd, &fdset);           // add sfd to the set
        FD_SET(fd, &fdset);            // add file input to the set
        ssize_t bytes_read;


        if (select(sfdmax, &fdset, NULL, NULL, 0) == -1){
            return;
        } else if (FD_ISSET(fd, &fdset)) { // send packets

            bytes_read = read(fd, buffer, MAX_PAYLOAD_SIZE);
            if (bytes_read == 0){
                condition = 0;
            }
            pkt_set_type(pkt, PTYPE_DATA);
            pkt_set_seqnum(pkt, 1);
            pkt_set_payload(pkt, buffer, bytes_read);
            pkt_set_tr(pkt, 0);
            size_t len = PKT_MAX_LEN;
            if (pkt_encode(pkt, buffpacket, &len) != PKT_OK){
                ERROR("Failed to encode packet \n");
                return;
            }

            error = write(sfd, buffpacket, len);
            if (error == -1){
                ERROR("Failed to send packet \n");
                return;
            }
            condition = 0;

        } /*else if (FD_ISSET(sfd, &fdset)) { // receive packets

            bytes_read = read(sfd, buffer, PKT_MAX_LEN);
            if (bytes_read == 0) {
                condition = 0;
            }

            error = write(STDOUT_FILENO, buffer, bytes_read);
            if (error == -1){
                return;
            }
        } */
    }

    free(buffpacket);
}