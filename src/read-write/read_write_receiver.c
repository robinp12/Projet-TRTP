#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>

#include "read_write_loop.h"
#include "../packet_interface.h"
#include "../log.h"

char* window;

pkt_status_code format_pkt(char *pkt_format, uint8_t *seqnum, uint32_t *time, ptypes_t type)
{
    pkt_t *pkt = pkt_new();
    pkt_status_code code;
    size_t len;

    code = pkt_set_type(pkt, type);
    code = pkt_set_tr(pkt, 0);
    code = pkt_set_window(pkt, 1);
    code = pkt_set_length(pkt, 0);
    code = pkt_set_seqnum(pkt, *seqnum);
    code = pkt_set_timestamp(pkt, *time);
    code = pkt_set_timestamp(pkt, *time);
}

void read_write_receiver(const int sfd, const int fd)
{
    fd_set fdset;    // file descriptor set
    int error;
    int condition = 1;
    char* buffpacket = malloc(sizeof(PKT_MAX_LEN));
    
    char *buffer = malloc(MAX_PAYLOAD_SIZE + 16);
    pkt_t *pkt = pkt_new();

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
        } else if (FD_ISSET(sfd, &fdset)) { // receive packets

            bytes_read = read(sfd, buffer, PKT_MAX_LEN);
            if (bytes_read == 0) {
                condition = 0;
            }
            error = write(STDOUT_FILENO, buffer, bytes_read);
            if (error == -1){
                return;
            }
            pkt_status_code code = pkt_decode(buffer, sizeof(buffer), pkt);
            if (code == PKT_OK)
            {
                printf("PKT OK");
                if (pkt_get_tr(pkt) == 1)
                {
                    //ENVOI UN NACK
                }
                if(pkt_get_type(pkt) != PTYPE_DATA && pkt_get_tr(pkt) != 0){
                    // PKT IGNORED
                }
            }
            else
            {
                printf("code error : %d\n", code);
            }
            printf("buffer : %ld \n",sizeof(buffer));
            printf("type : %d \n",pkt_get_type(pkt));
            printf("tr : %d \n",pkt_get_tr(pkt));
            printf("window : %d \n",pkt_get_window(pkt));
            printf("length : %d \n",pkt_get_length(pkt));
            printf("seqnum : %d \n",pkt_get_seqnum(pkt));

            
            condition = 0;
        }
    }
    free(buffer);
    free(buffpacket);
}