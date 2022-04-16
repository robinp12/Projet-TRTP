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

static char *copybuf;
static int eof_reached = 0; // flag for the end of file
static int lastPkt = 0;     // flag to send the last packet (end of file and all packets are send)



/* Stats variables */
static int data_sent = 0;             // number of PTYPE_DATA packet sent
static int data_received = 0;         // number of valid PTYPE_DATA packets received
static int data_truncated_received;   // number of valied PTYPE_DATA packets with TR field to 1 received
static int fec_sent = 0;              // number of PTYPE_FEC packets sent
static int fec_received = 0;          // number of valid PTYPE_FEC packets received
static int ack_sent = 0;              // number of PTYPE_ACK packets sent
static int ack_received = 0;          // number of valid PTYPE_ACK packets received
static int nack_sent = 0;             // number of PTYPE_NACK packets sent
static int nack_received = 0;         // number of valid PTYPE_NACK packets received 
static int packet_ignored = 0;        // number of ignored packets
static uint32_t min_rtt;              // minimum time (in milliseconds) between the sending of a PTYPE_DATA packet and the receiving of the corresponding PTYPE_ACK packet
static uint32_t max_rtt;              // maximum time (in milliseconds) between the sending of a PTYPE_DATA packet and the receiving of the corresponding PTYPE_ACK packet
static int packet_retransmitted = 0;  // number of  PTYPE_DATA packets resent (loss, truncation or corruption)



/*
* Send final packet
*/
int send_final_pkt(const int sfd, window_pkt_t* window)
{
    ASSERT(window->linkedList->size == 0);

    struct timeval timestamp;

    pkt_t* pkt = pkt_new();
    pkt_set_type(pkt, PTYPE_DATA);
    pkt_set_tr(pkt, 0);
    pkt_set_length(pkt, 0);
    pkt_set_window(pkt, window->windowsize);
    pkt_set_seqnum(pkt, window->seqnumNext);
    gettimeofday(&timestamp, NULL);
    pkt_set_timestamp(pkt, (uint32_t) timestamp.tv_usec);

    size_t len = PKT_MAX_LEN;
    if (pkt_encode(pkt, copybuf, &len) != PKT_OK){
        ERROR("Failed to encode last packet !");
        return -1;
    }
    if (write(sfd, copybuf, len) == -1){
        ERROR("Failed to send last packet !");
        return -1;
    }
    ++data_sent;
    linkedList_add_pkt(window->linkedList, pkt);
    return 0;
}

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
        ++window->pktnum;

        pkt_t *pkt = pkt_new();
        pkt_set_type(pkt, PTYPE_DATA);
        pkt_set_seqnum(pkt, window->seqnumNext);
        pkt_set_window(pkt, window->windowsize);
        pkt_set_tr(pkt, 0);
        pkt_set_payload(pkt, copybuf, bytes_read);
        gettimeofday(&timestamp, NULL);
        pkt_set_timestamp(pkt, (uint32_t)timestamp.tv_usec);

        linkedList_add_pkt(list, pkt);
      
        window->seqnumTail = window->seqnumNext;
        window->seqnumNext = (window->seqnumNext + 1) % 256;
        

        bytes_read = PKT_MAX_LEN;
        pkt_encode(pkt, copybuf, &bytes_read);

        if (write(sfd, copybuf, bytes_read) == -1)
        {
            ERROR("Failed to send packet : %s", strerror(errno));
            return -1;
        }

        ++data_sent;
        ++n_filled;
    }
    window->seqnumHead = pkt_get_seqnum(list->head->pkt);
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

        if (pkt != NULL && ((uint32_t) timeout.tv_usec - pkt_get_timestamp(pkt)) > TIMEOUT)
        {
            pkt_set_timestamp(pkt, (uint32_t) timeout.tv_usec);
            pkt_encode(pkt, copybuf, &bytes_copied);
            if (write(sfd, copybuf, bytes_copied) == -1)
            {
                ERROR("Failed to resent pkt : %s", strerror(errno));
                return -1;
            }
            ++packet_retransmitted;
            ++nbr_resent;
        }
        if (current == window->linkedList->tail)
            return nbr_resent;
        current = current->next;
    }
    return nbr_resent;
}

/*
* Print the current state of the window (debug)
*/
void print_window(window_pkt_t* window){
    
    linkedList_t* list = window->linkedList;
    node_t* current = window->linkedList->head;
    if (list->size == 0){
        printf("[empty list]\n");
        return;
    }
    
    printf("Head %d, tail %d, size %d    | ", window->seqnumHead, window->seqnumTail, list->size);

    while (1)
    {
        printf(" %d ", pkt_get_seqnum(current->pkt));
        if (current == list->tail)
            break;
        current = current->next;
    }
    printf("\n");
}

/*
* Check if the packet can be removed from the window
* @return 1 if yes, 0 if no
*/
int seqnum_in_window(window_pkt_t* window, uint8_t ackSeqnum, uint8_t pktSeqnum)
{
    /* the window is not cut by the border [-----xxxxxx--] */
    /*                                     [-----HxxxxT--] */
    if (window->seqnumTail >= window->seqnumHead){
        if (pktSeqnum < ackSeqnum)
            return 1;
        else
            return 0;
    }
    /* the window is cut by the border [xx------xxxxx] */
    else {
        if (ackSeqnum > window->seqnumHead){    /* [xx------xxxAx] */
            if (pktSeqnum < ackSeqnum && pktSeqnum > window->seqnumTail) /* [xx------xPxAx] */
                return 1;
            else
                return 0;
        }
        if (ackSeqnum <= window->seqnumTail + 1){     /* [xxAx------xx] */
            if (pktSeqnum <= window->seqnumTail){ /* [??A?------xx] */
                if (pktSeqnum < ackSeqnum)       /* [xPAx------xx] */
                    return 1;
                else                             /* [xxAP------xx] */
                    return 0;
            }
            if (pktSeqnum >= window->seqnumHead)  /* [xA------xPxx] */
                return 1;
        }
    }
    return 0;
}

/*
 * Delete from window the acked packets
 */
int ack_window(window_pkt_t *window, pkt_t* pkt)
{
    uint8_t ackSeqnum = pkt_get_seqnum(pkt);
    DEBUG("ack %d", ackSeqnum);

    linkedList_t* list = window->linkedList;
    struct timeval timestamp;
    node_t* current = list->head;
    node_t* previous = list->head;
    int no_acked = 0;

    while (current != NULL && seqnum_in_window(window, ackSeqnum, pkt_get_seqnum(current->pkt)))
    {

        gettimeofday(&timestamp, NULL);
        uint32_t rtt = ( (uint32_t) timestamp.tv_usec - pkt_get_timestamp(current->pkt)) / 1000;
        
        if (rtt > max_rtt)
            max_rtt = rtt;
        else if (rtt < min_rtt)
            min_rtt = rtt;

        if (current == list->head) {
            linkedList_remove(list);

            current = list->head;
            previous = list->head;

            if (list->size > 0)
                window->seqnumHead = pkt_get_seqnum(current->pkt);
        }
        else if (current == list->tail) {
            linkedList_remove_end(list);
            
            if (list->size > 0)
                window->seqnumTail = pkt_get_seqnum(list->tail->pkt);
            return no_acked++;
        }
        else {
            linkedList_remove_middle(list, previous, current);
            current = previous->next;
        }

        no_acked++;
        if (list->size == 0){
            return no_acked;
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
    ++packet_retransmitted;
    return 0;
}


/*
* Update the size of the window in the linked list
* @return : 0 if the size stay the same
*           1 if the size increase
*           2 if the size decrease
            -1 in case of error
*/
int update_window(window_pkt_t* window, uint8_t newSize){
    if (newSize == window->windowsize){
        return 0;
    }
    if (newSize > window->windowsize){
        window->windowsize = newSize;
        return 1;
    }

    linkedList_t* list = window->linkedList;
    while (list->size > window->windowsize)
    {
        if (linkedList_remove_end(list) != 0){
            ERROR("linkedList_remove_end failed");
            return -1;
        }
    }
    
    window->seqnumTail = pkt_get_seqnum(list->tail->pkt);
    window->windowsize = newSize;
    return 2;
}

void read_write_sender(const int sfd, const int fd, const char* stats_filename)
{

    copybuf = (char*) malloc(sizeof(char)*PKT_MAX_LEN);
    if (copybuf == NULL){
        ERROR("Ne memoire");
        return;
    }

    int retval;
    min_rtt = UINT32_MAX;
    max_rtt = 0;

    window_pkt_t *window = malloc(sizeof(window_pkt_t));
    window->offset = 0;
    window->pktnum = 0;
    window->seqnumHead = 0;
    window->seqnumTail = 0;
    window->seqnumNext = 0;
    window->windowsize = 1;
    window->linkedList = linkedList_create();

    struct pollfd *fds = malloc(sizeof(*fds));
    int ndfs = 1;

    memset(fds, 0, sizeof(struct pollfd));

    eof_reached = 0;
    fds[0].fd = sfd;
    fds[0].events = POLLOUT | POLLIN;

    while (1)
    {
        retval = poll(fds, ndfs, -1);
        if (retval < 0)
        {
            ERROR("error poll");
            return;
        }

        /* Sending packets */
        if ((fds[0].revents & POLLOUT))
        {
            if (!eof_reached && window->linkedList->size != window->windowsize){
                retval = fill_window(fd, sfd, window);
            }

            retval = resent_pkt(sfd, window);
            if (retval != 0){
                DEBUG("resent_pkt returned %d", retval);
            }
            
        }

        /* Reading incoming packets */
        if (fds[0].revents & POLLIN)
        {
            ssize_t bytes_read = read(sfd, copybuf, PKT_MAX_LEN);
  
            pkt_t *pkt = pkt_new();
            retval = pkt_decode(copybuf, bytes_read, pkt);

            if (retval == PKT_OK && pkt_get_tr(pkt) == 0)
            {
                if (pkt_get_type(pkt) == PTYPE_NACK)
                {
                    ++nack_received;
                    retval = resent_nack(sfd, window, pkt_get_seqnum(pkt));
                }
                else if (pkt_get_type(pkt) == PTYPE_ACK)
                {
                    ++ack_received;
                    retval = ack_window(window, pkt);

                    if (!lastPkt && eof_reached && window->linkedList->size == 0)
                    {
                        lastPkt = 1;
                        if (send_final_pkt(sfd, window) != 0)
                        {
                            break;
                        }
                    }
                    if (lastPkt && window->linkedList->size == 0)
                    {
                        break;
                    }
                }
                
                if (!lastPkt)
                {
                    retval = update_window(window, pkt_get_window(pkt));
                }
                

                
            } else {
                ++packet_ignored;
            }
            
            pkt_del(pkt);
        }
        if (eof_reached && window->linkedList->size == 0){
            
        }
    }

    /* Frees */

    free(copybuf);
    linkedList_del(window->linkedList);
    free(window);
    free(fds);

    if (stats_filename != NULL){
        FILE* f = fopen(stats_filename, "w");
        if (f == NULL){
            ERROR("Failed to open file %s : %s", stats_filename, strerror(errno));
            return;
        }
        fprintf(f, "data_sent:%d\n", data_sent);
        fprintf(f, "data_received:%d\n", data_received);
        fprintf(f, "data_truncated_received:%d\n", data_truncated_received);
        fprintf(f, "fec_sent:%d\n", fec_sent);
        fprintf(f, "fec_received:%d\n", fec_received);
        fprintf(f, "ack_sent:%d\n", ack_sent);
        fprintf(f, "ack_received:%d\n", ack_received);
        fprintf(f, "nack_sent:%d\n", nack_sent);
        fprintf(f, "nack_received:%d\n", nack_received);
        fprintf(f, "packet_ignored:%d\n", packet_ignored);
        fprintf(f, "min_rtt:%d\n", min_rtt);
        fprintf(f, "max_rtt:%d\n", max_rtt);
        fprintf(f, "packet_retransmitted:%d\n", packet_retransmitted);
        fclose(f);
    }
    return;
}