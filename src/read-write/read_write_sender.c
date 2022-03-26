#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>

#include "read_write_sender.h"
#include "../packet_interface.h"
#include "../log.h"

#define TIMEOUT 2000000

int windowsize = 1;
off_t offset = 0;
int no_pkt = 0;
char* copybuf;
int eof_reached;

typedef struct window_pkt {
    int seqnummax;
    int seqnummin;
    int no_pktmax;
    int no_pktmin;
    int windowsize;
    int offset;
    pkt_t** pkt_array;
} window_pkt_t;

/*
* Fill the window with packet and sent them
*/
int fill_window(const int fd, const int sfd, window_pkt_t* window, int no_pkt)
{
    size_t bytes_read;
    struct timeval timestamp;
    int n_filled = 0;
    for (int i = 0; (i < window->windowsize) && (eof_reached == 0); i++)
    {
       
        if (window->pkt_array[i] == NULL){
            bytes_read = pread(fd, copybuf, MAX_PAYLOAD_SIZE, MAX_PAYLOAD_SIZE*no_pkt);
            if (bytes_read != MAX_PAYLOAD_SIZE){
                DEBUG("EOF");
                eof_reached = 1;
            }

            pkt_t* pkt = pkt_new();
            pkt_set_type(pkt, PTYPE_DATA);
            pkt_set_seqnum(pkt, no_pkt % 256);
            pkt_set_window(pkt, window->windowsize);
            pkt_set_tr(pkt, 0);
            pkt_set_payload(pkt, copybuf, bytes_read);
            gettimeofday(&timestamp, NULL);
            pkt_set_timestamp(pkt, (uint32_t) timestamp.tv_usec);

            window->pkt_array[i] = pkt;
            bytes_read = PKT_MAX_LEN;
            pkt_encode(pkt, copybuf, &bytes_read);
            
            DEBUG("Sending packet %d", no_pkt);
            if (write(sfd, copybuf, bytes_read) == -1){
                ERROR("Failed to send packet : %s", strerror(errno));
                return -1;
            }
            no_pkt++;
            n_filled++;
        }
        window->seqnummax = no_pkt % 256;
        window->no_pktmax = no_pkt;
    }
    return n_filled;
}

/*
* Resent timeout packet in the window
*/
int resent_pkt(const int sfd, window_pkt_t* window)
{
    int nbr_resent = 0;
    struct timeval timeout;
    size_t bytes_copied;
    for (int i = 0; i < window->windowsize; i++)
    {
        pkt_t* pkt = window->pkt_array[i];
        gettimeofday(&timeout, NULL);
        if (pkt != NULL && (timeout.tv_usec - pkt_get_timestamp(pkt)) > TIMEOUT){
            pkt_set_timestamp(pkt, (uint32_t) timeout.tv_usec);
            pkt_encode(pkt, copybuf, &bytes_copied);
            if (write(sfd, copybuf, bytes_copied) == -1){
                return -1;
            }
            nbr_resent++;
        }
    }
    return 0;
}


/*
* Delete from window the acked packets
*/
void ack_window(window_pkt_t* window, int ack)
{
    for (int i = 0; i < window->windowsize; i++)
    {
        DEBUG("Seqnum : %d for ack : %d", pkt_get_seqnum(window->pkt_array[i]), ack);
        if (pkt_get_seqnum(window->pkt_array[i]) < ack){
            pkt_del(window->pkt_array[i]);
            window->pkt_array[i] = NULL;
            window->seqnummin = ack - 1;
        }
    }
    
}

/*
* Resent the truncated packet
*/
void resent_nack(const int sfd, window_pkt_t* window, int nack){
    size_t len;
    pkt_encode(window->pkt_array[nack], copybuf, &len);
    write(sfd, copybuf, len);
}

void read_write_sender(const int sfd, const int fd)
{
    
    copybuf = malloc(PKT_MAX_LEN);
    int retval;


    window_pkt_t* window = malloc(sizeof(window_pkt_t));
    window->windowsize = 3;
    window->pkt_array = malloc(sizeof(pkt_t*)*32);
    
    

    struct pollfd *fds = malloc(sizeof(*fds));
    int ndfs = 1;
    
    memset(fds, 0, sizeof(struct pollfd));


    int condition = 1;
    eof_reached = 0;
    fds[0].fd = sfd;
    fds[0].events = POLLOUT | POLLIN;

    while(!eof_reached || condition){
        
        retval = poll(fds, ndfs, -1);
        if(retval < 0){
            ERROR("error poll");
            return;
        }
        if((fds[0].revents & POLLOUT) && !eof_reached){
            // ssize_t bytes_read = read(fd, copybuf, MAX_PAYLOAD_SIZE);
            // if (bytes_read == 0){
            //     DEBUG("End of file");
            //     eof_reached = 1;
            //     continue;
            // }

            // pkt_t* pkt = pkt_new();
            // pkt_set_type(pkt, PTYPE_DATA);
            // pkt_set_payload(pkt, copybuf, bytes_read);

            // size_t bytes_encoded;
            // pkt_encode(pkt, copybuf, &bytes_encoded);
            // DEBUG("Send packet");
            // write(sfd, copybuf, bytes_encoded);
            fill_window(fd, sfd, window, no_pkt);

            resent_pkt(sfd, window);

        } else if ((fds[0].revents & POLLIN) && condition){
            pkt_t* pkt = pkt_new();
            ssize_t bytes_read = read(sfd, copybuf, PKT_MAX_LEN);
            pkt_decode(copybuf, bytes_read, pkt);

            if (pkt_get_type(pkt) == PTYPE_NACK){
                DEBUG("Nack");
                resent_nack(sfd, window, pkt_get_seqnum(pkt));
            } else if (pkt_get_type(pkt) == PTYPE_ACK){
                DEBUG("Ack");
                ack_window(window, pkt_get_seqnum(pkt));
            }
            pkt_del(pkt);
            condition = 0;
        }
    }
    DEBUG("Frees");
    free(copybuf);
    free(window->pkt_array);
    free(window);
}