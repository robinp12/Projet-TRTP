#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>

#include "read_write_sender.h"
#include "../packet_implem.h"
#include "../linkedList/linkedList.h"
#include "../log.h"

#define TIMEOUT 2000000

off_t offset = 0;
char *copybuf;
int eof_reached;

typedef struct window_pkt
{
    uint8_t seqnum;
    int windowsize;

    int offset;
    uint64_t pktnum;

    linkedList_t* linkedList;
} window_pkt_t;

/*
 * Fill the window with packet and sent them
 */
int fill_window(const int fd, const int sfd, window_pkt_t *window)
{
    size_t bytes_read;
    struct timeval timestamp;
    int n_filled = 0;
    
    linkedList_t* list = window->linkedList;
    
    while ((list->size < window->windowsize) && (eof_reached == 0))
    {

        bytes_read = pread(fd, copybuf, MAX_PAYLOAD_SIZE, MAX_PAYLOAD_SIZE * window->pktnum);
        if (bytes_read != MAX_PAYLOAD_SIZE)
        {
            DEBUG("EOF");
            eof_reached = 1;
        }

        pkt_t *pkt = pkt_new();
        pkt_set_type(pkt, PTYPE_DATA);
        pkt_set_seqnum(pkt, window->seqnum);
        pkt_set_window(pkt, window->windowsize);
        pkt_set_tr(pkt, 0);
        pkt_set_payload(pkt, copybuf, bytes_read);
        gettimeofday(&timestamp, NULL);
        pkt_set_timestamp(pkt, (uint32_t)timestamp.tv_usec);

        linkedList_add_pkt(list, pkt);

        bytes_read = PKT_MAX_LEN;
        pkt_encode(pkt, copybuf, &bytes_read);

        DEBUG("Sending packet %lld", window->pktnum);
        if (write(sfd, copybuf, bytes_read) == -1)
        {
            ERROR("Failed to send packet : %s", strerror(errno));
            return -1;
        }
        window->pktnum++;
        n_filled++;

        window->seqnum = (window->seqnum + 1) % 256;
    }
    return n_filled;
}

/*
 * Resent timeout packet in the window
 */
int resent_pkt(const int sfd, window_pkt_t *window)
{
    int nbr_resent = 0;
    struct timeval timeout;
    size_t bytes_copied;

    node_t* current = window->linkedList->head;

    while (current != NULL)
    {
        pkt_t *pkt = current->pkt;
        gettimeofday(&timeout, NULL);
        if (pkt != NULL && (timeout.tv_usec - pkt_get_timestamp(pkt)) > TIMEOUT)
        {
            pkt_set_timestamp(pkt, (uint32_t)timeout.tv_usec);
            pkt_encode(pkt, copybuf, &bytes_copied);
            if (write(sfd, copybuf, bytes_copied) == -1)
            {
                return -1;
            }
            nbr_resent++;
        }
        current = current->next;
    }
    return nbr_resent;
}

/*
 * Delete from window the acked packets
 */
int ack_window(window_pkt_t *window, int ack)
{
    linkedList_t* list = window->linkedList;
    node_t* current = list->head;
    node_t* previous = NULL;
    int no_acked = 0;

    while (current != NULL)
    {
        if (pkt_get_seqnum(current->pkt) < ack)
        {
            if (current == list->head) {
                linkedList_remove(list);
                current = list->head;
            }
            else if (current == list->tail) {
                linkedList_remove_end(list);
                previous = current;
                current = current->next;
            }
            else {
                previous->next = current->next;
                list->size--;
                node_t* old = current;
                current = current->next;
                pkt_del(old->pkt);
                free(old);
            }
            no_acked++;
        }
    }
    return no_acked;
}

/*
 * Resent the truncated packet
 */
int resent_nack(const int sfd, window_pkt_t *window, int nack)
{
    size_t len;
    int error;
    node_t* current = window->linkedList->head;

    while (current != NULL && pkt_get_seqnum(current->pkt) != nack)
    {
        current = current->next;
    }
    if (current == NULL){
        return -1;
    }

    struct timeval timeout;
    gettimeofday(&timeout, NULL);
    pkt_set_timestamp(current->pkt, (uint32_t) timeout.tv_usec);

    error = pkt_encode(current->pkt, copybuf, &len);
    if (error != PKT_OK) {return -2;}

    error = write(sfd, copybuf, len);
    if (error == -1) {return -1;}

    return 0;
}

void read_write_sender(const int sfd, const int fd)
{

    copybuf = (char*) malloc(sizeof(char)*PKT_MAX_LEN);
    if (copybuf == NULL){
        ERROR("Ne memoire");
        return;
    }
    int retval;

    window_pkt_t *window = malloc(sizeof(window_pkt_t));
    window->offset = 0;
    window->pktnum = 0;
    window->seqnum = 0;
    window->windowsize = 1;
    window->linkedList = linkedList_create();

    struct pollfd *fds = malloc(sizeof(*fds));
    int ndfs = 1;

    memset(fds, 0, sizeof(struct pollfd));

    eof_reached = 0;
    fds[0].fd = sfd;
    fds[0].events = POLLOUT | POLLIN;

    while (!eof_reached)
    {

        DEBUG("Waiting poll");
        retval = poll(fds, ndfs, -1);
        if (retval < 0)
        {
            ERROR("error poll");
            return;
        }

        DEBUG("after poll");
        if ((fds[0].revents & POLLOUT) && !eof_reached)
        {
            if (window->linkedList->size != window->windowsize){
                retval = fill_window(fd, sfd, window);
                if (retval >= 0){
                    DEBUG("Fill window : %d", retval);
                }
            }

            // retval = resent_pkt(sfd, window);
            // if (retval > 0){
            //     DEBUG("Resent pkt : %d", retval);
            // }
        }
        else if (fds[0].revents & POLLIN)
        {
            DEBUG("read in");
            ssize_t bytes_read = read(sfd, copybuf, PKT_MAX_LEN);
            pkt_t *pkt = pkt_new();
            pkt_decode(copybuf, bytes_read, pkt);

            window->windowsize = pkt_get_window(pkt);

            if (pkt_get_type(pkt) == PTYPE_NACK)
            {
                DEBUG("Nack");
                retval = resent_nack(sfd, window, pkt_get_seqnum(pkt));
                DEBUG("Nack %d", retval);
            }
            else if (pkt_get_type(pkt) == PTYPE_ACK)
            {
                DEBUG("Ack");
                retval = ack_window(window, pkt_get_seqnum(pkt));
                DEBUG("Ack %d", retval);
            }
            pkt_del(pkt);
        }
    }
    DEBUG("Frees");
    free(copybuf);
    linkedList_del(window->linkedList);
    free(window);
}