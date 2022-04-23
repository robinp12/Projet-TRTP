#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <time.h>

#include "read_write_sender.h"
#include "../packet_implem.h"
#include "../linkedList/linkedList.h"
#include "../log.h"
#include "../fec.h"

#define TIMEOUT 2000        // timeout to resent pkt
#define MAX_RESENT_LAST_PKT 4  // number of time we have to resent the last packet before ending the transmission

static char *copybuf;
static int eof_reached = 0; // flag for the end of file
static int lastPkt = 0;     // flag to send the last packet (end of file and all packets are send)
static int lastRst = 0;     // number of time the last packet has been resent
static fec_pkt_t* fec;


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
* Return the time in millisecond since epoch
*/
uint32_t time_millis(void){
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    return tp.tv_sec * 1000 + tp.tv_nsec/1000000;
}

/*
* Send final packet
*/
int send_final_pkt(const int sfd, window_pkt_t* window)
{
    pkt_t* pkt = pkt_new();
    pkt_set_type(pkt, PTYPE_DATA);
    pkt_set_tr(pkt, 0);
    pkt_set_length(pkt, 0);
    pkt_set_window(pkt, window->windowsize);
    pkt_set_seqnum(pkt, window->seqnumNext);
    
    pkt_set_timestamp(pkt, time_millis());
    
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
    window->seqnumTail = window->seqnumNext;
    lastPkt = 1;
    LOG_SENDER("Final packet sent with seqnum %d", window->seqnumNext);
    return 0;
}

/*
 * Fill the window with packet and sent them
 */
int fill_window(const int fd, const int sfd, window_pkt_t *window)
{
    size_t bytes_read;

    int n_filled = 0;
    
    linkedList_t* list = window->linkedList;
    
    while ((list->size < window->windowsize) && (eof_reached == 0))
    {

        bytes_read = read(fd, copybuf, MAX_PAYLOAD_SIZE);
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

        pkt_set_timestamp(pkt, time_millis());

        linkedList_add_pkt(list, pkt);
        LOG_SENDER("Adding packet %d to the list", window->seqnumNext);

        bytes_read = PKT_MAX_LEN;
        pkt_encode(pkt, copybuf, &bytes_read);

        if (write(sfd, copybuf, bytes_read) == -1)
        {
            ERROR("Failed to send packet : %s", strerror(errno));
            return -1;
        }

        LOG_SENDER("Packet sent with seqnum %d", window->seqnumNext);

        fec->pkts[fec->i] = pkt;
        ++fec->i;
        if (fec->i == 4){
            pkt_t* fec_pkt = fec_generation(fec->pkts[0], fec->pkts[1], fec->pkts[2], fec->pkts[3]);

            size_t len = PKT_MAX_LEN;
            pkt_encode(fec_pkt, copybuf, &len);

            if (write(sfd, copybuf, len) == -1){
                ERROR("Failed to send fec : %s", strerror(errno));
                return -1;
            }
            LOG_SENDER("Send fec with seqnum %d", pkt_get_seqnum(fec->pkts[0]));
            pkt_del(fec_pkt);
            ++fec_sent;
            fec->i = 0;
        }

        window->seqnumTail = window->seqnumNext;
        window->seqnumNext = (window->seqnumNext + 1) % 256;

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
    size_t bytes_copied;

    node_t* current = window->linkedList->head;

    while (current != NULL)
    {
        pkt_t *pkt = current->pkt;
        uint32_t time = time_millis();
        //DEBUG("%d > %d ? %d", ((uint32_t) timeout.tv_nsec - pkt_get_timestamp(pkt))/1000, (uint32_t) TIMEOUT/1000, (((uint32_t) timeout.tv_nsec - pkt_get_timestamp(pkt)) > TIMEOUT));

        if (pkt != NULL && (time - pkt_get_timestamp(pkt)) > TIMEOUT)
        {
            pkt_set_timestamp(pkt, time);
            bytes_copied = PKT_MAX_LEN;
            pkt_encode(pkt, copybuf, &bytes_copied);
            if (write(sfd, copybuf, bytes_copied) == -1)
            {
                ERROR("Failed to resent pkt : %s", strerror(errno));
                return -1;
            }
            LOG_SENDER("Resent packet with seqnum %d", pkt_get_seqnum(pkt));

            if (pkt_get_length(pkt) == 0){
                ++lastRst;
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
 * @return -1 if the packet cannot ack the window, the number of acked packet otherwise
 */
int ack_window(window_pkt_t *window, pkt_t* pkt)
{
    uint8_t ackSeqnum = pkt_get_seqnum(pkt);

    if (window->seqnumHead < window->seqnumTail)
    {
        if (ackSeqnum > window->seqnumTail + 1 || ackSeqnum < window->seqnumHead)
        {
            ++packet_ignored;
            LOG_SENDER("Packet with seqnum %d ignored (not in window)", ackSeqnum);
            return -1;
        }
    }
    else
    {
        if (ackSeqnum < window->seqnumHead && ackSeqnum > window->seqnumTail + 1)
        {
            ++packet_ignored;
            LOG_SENDER("Packet with seqnum %d ignored (not in window)", ackSeqnum);
            return -1;
        }
    }

    linkedList_t* list = window->linkedList;
   
    node_t* current = list->head;
    node_t* previous = list->head;
    int no_acked = 0;
    uint32_t time = time_millis();

    while (current != NULL && seqnum_in_window(window, ackSeqnum, pkt_get_seqnum(current->pkt)))
    {

        
        uint32_t rtt = (time - pkt_get_timestamp(current->pkt));
        LOG_SENDER("Removing from the list packet with seqnum %d", pkt_get_seqnum(current->pkt));

        fec->i = 0;
        
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
    size_t len = PKT_MAX_LEN;
    int error;
    node_t* current = window->linkedList->head;

    while (current != NULL && pkt_get_seqnum(current->pkt) != nack)
    {
        current = current->next;
    }
    if (current == NULL){
        LOG_SENDER("Failed to find packet with seqnum %d", nack);
        return -1;
    }


    pkt_set_timestamp(current->pkt, time_millis());

    error = pkt_encode(current->pkt, copybuf, &len);
    if (error != PKT_OK) {return -2;}

    error = write(sfd, copybuf, len);
    if (error == -1) {return -1;}
    LOG_SENDER("Resent packet with seqnum %d (nack)", pkt_get_seqnum(current->pkt));
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
    if (newSize == 0){
        LOG_SENDER("Receiver tried to set window to 0, packet ignored");
        packet_ignored++;
        return -1;
    }
    if (newSize == window->windowsize){
        return 0;
    }
    if (newSize > window->windowsize){
        window->windowsize = newSize;
        LOG_SENDER("Window increased to %d", newSize);
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
    LOG_SENDER("Window decreased to %d", newSize);
    return 2;
}

void read_write_sender(const int sfd, const int fd, const int fd_stats)
{

    copybuf = (char*) malloc(sizeof(char)*PKT_MAX_LEN);
    if (copybuf == NULL){
        ERROR("Ne memoire");
        return;
    }

    int retval;
    min_rtt = UINT32_MAX;
    max_rtt = 0;
    fec = malloc(sizeof(fec_pkt_t));
    fec->i = 0;
    fec->seqnum = 0;

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
            if (!eof_reached && window->linkedList->size < window->windowsize){
                retval = fill_window(fd, sfd, window);
            }
            if (!lastPkt && eof_reached && window->linkedList->size < window->windowsize){
                lastPkt = 1;
                retval = send_final_pkt(sfd, window);
            }

            retval = resent_pkt(sfd, window);

            if (lastPkt && lastRst > MAX_RESENT_LAST_PKT){
                LOG_SENDER("No news from receiver, last packet has been resent %d times, assuming end of transmission", MAX_RESENT_LAST_PKT);
                break;
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
                    LOG_SENDER("Received nack with seqnum %d", pkt_get_seqnum(pkt));
                    retval = resent_nack(sfd, window, pkt_get_seqnum(pkt));
                }
                else if (pkt_get_type(pkt) == PTYPE_ACK)
                {
                    ++ack_received;
                    LOG_SENDER("Received ack with seqnum %d", pkt_get_seqnum(pkt));
                    retval = ack_window(window, pkt);

                    if (lastPkt && window->linkedList->size == 0)
                    {
                        LOG_SENDER("All packets ack, end of transmission");
                        pkt_del(pkt);
                        break;
                    }
                }
                
                retval = update_window(window, pkt_get_window(pkt));
                
            } else {
                ++packet_ignored;
                if (retval == E_CRC){
                    LOG_SENDER("Packet ignored due to bad CRC");
                }
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
    free(fec);


    dprintf(fd_stats, "data_sent:%d\n", data_sent);
    dprintf(fd_stats, "data_received:%d\n", data_received);
    dprintf(fd_stats, "data_truncated_received:%d\n", data_truncated_received);
    dprintf(fd_stats, "fec_sent:%d\n", fec_sent);
    dprintf(fd_stats, "fec_received:%d\n", fec_received);
    dprintf(fd_stats, "ack_sent:%d\n", ack_sent);
    dprintf(fd_stats, "ack_received:%d\n", ack_received);
    dprintf(fd_stats, "nack_sent:%d\n", nack_sent);
    dprintf(fd_stats, "nack_received:%d\n", nack_received);
    dprintf(fd_stats, "packet_ignored:%d\n", packet_ignored);
    dprintf(fd_stats, "min_rtt:%d\n", min_rtt);
    dprintf(fd_stats, "max_rtt:%d\n", max_rtt);
    dprintf(fd_stats, "packet_retransmitted:%d\n", packet_retransmitted);

    return;
}