#ifndef __READ_WRITE_SENDER_H_
#define __READ_WRITE_SENDER_H_


/* Loop reading a socket and printing to stdout,
 * while reading stdin and writing to the socket
 * @param: sfd : the socket file descriptor. It is both bound and connected.
 *         fd : the input file to transmit
 *         stats_filename : the name of the file to write de stats
 * @return: as soon as stdin signals EOF
 */
void read_write_sender(const int sfd, const int fd, const char* stats_filename);

#endif
