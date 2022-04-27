#ifndef __READ_WRITE_SENDER_H_
#define __READ_WRITE_SENDER_H_

#include "../linkedList/linkedList.h"

typedef struct window_pkt
{
    uint8_t seqnumHead;
    uint8_t seqnumTail; // seqnum of the packet at the end of the linked list
    int windowsize;     // current size of the window

    linkedList_t* linkedList; // linked list with all the packet (by ascending order)
} window_pkt_t;

void print_window(window_pkt_t* window);

/*
* Check if the packet is in the window
* @returns 1 if yes, 0 if no
*/
int is_in_sender_window(window_pkt_t* window, uint8_t seqnum);

int can_ack_window(window_pkt_t* window, uint8_t seqnum);

/*
* Check if the packet can be removed from the window
* @return 1 if yes, 0 if no
*/
int seqnum_in_window(window_pkt_t* window, uint8_t ackSeqnum, uint8_t pktSeqnum);

/* Loop reading a socket and printing to stdout,
 * while reading stdin and writing to the socket
 * @param: sfd : the socket file descriptor. It is both bound and connected.
 *         fd : the input file to transmit
 *         stats_filename : the name of the file to write de stats
 * @return: as soon as stdin signals EOF
 */
void read_write_sender(const int sfd, const int fd, const int fd_stats, const int fec_enable);

#endif
