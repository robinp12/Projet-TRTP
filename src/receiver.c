#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#include "log.h"
#include "packet_interface.h"
#include "read-write/create_socket.h"
#include "read-write/real_address.h"
#include "read-write/wait_for_client.h"
#include "read-write/read_write_receiver.h"

int print_usage(char *prog_name)
{
    ERROR("Usage:\n\t%s [-s stats_filename] listen_ip listen_port", prog_name);
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    int opt;
    // int sock = -1;
    struct sockaddr_in6 listener_addr;
    //socklen_t listener_addrlen = sizeof(listener_addr);

    //FILE *fd;

    char *stats_filename = NULL;
    char *listen_ip = NULL;
    char *listen_port_err;
    uint16_t listen_port;

    while ((opt = getopt(argc, argv, "s:h")) != -1)
    {
        switch (opt)
        {
        case 'h':
            return print_usage(argv[0]);
        case 's':
            stats_filename = optarg;
            break;
        default:
            return print_usage(argv[0]);
        }
    }

    if (optind + 2 != argc)
    {
        ERROR("Unexpected number of positional arguments");
        return print_usage(argv[0]);
    }

    listen_ip = argv[optind];
    listen_port = (uint16_t)strtol(argv[optind + 1], &listen_port_err, 10);
    if (*listen_port_err != '\0')
    {
        ERROR("Receiver port parameter is not a number");
        return print_usage(argv[0]);
    }

    ASSERT(1 == 1);               // Try to change it to see what happens when it fails
    DEBUG_DUMP("Some bytes", 11); // You can use it with any pointer type

    // This is not an error per-se.
    ERROR("Receiver has following arguments: stats_filename is %s, listen_ip is %s, listen_port is %u",
          stats_filename, listen_ip, listen_port);

    DEBUG("You can only see me if %s", "you built me using `make debug`");
    ERROR("This is not an error, %s", "now let's code!");


    /* Resolve the hostname */
    DEBUG("Resolve hostname");
	
	const char *err = real_address(listen_ip, &listener_addr);
	if (err) {
		fprintf(stderr, "Could not resolve hostname %s: %s\n", listen_ip, err);
		return EXIT_FAILURE;
	}
	/* Get a socket */
    DEBUG("Get socket");
	int sfd;
	sfd = create_socket(NULL, -1, &listener_addr, listen_port); /* Connected */
	
	if (sfd < 0) {
		fprintf(stderr, "Failed to create the socket!: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

    char* buff = malloc(sizeof(char)*PKT_MAX_LEN);


    DEBUG("Create first packet to initiate connexion");
    pkt_t* first_packet = pkt_new();
    pkt_set_type(first_packet, PTYPE_ACK);
    pkt_set_seqnum(first_packet, 0);
    pkt_set_window(first_packet, 10);
    size_t len = PKT_MAX_LEN;

    pkt_encode(first_packet, buff, &len);
    if (write(sfd, buff, len) == -1){
        ERROR("Failed to sent first packet : %s", strerror(errno));
        return EXIT_FAILURE;
    }
    pkt_del(first_packet);
    DEBUG("First packet sent");

	/* Process I/O */
	read_write_receiver(sfd, STDOUT_FILENO);

    free(buff);

	close(sfd);

    read_write_receiver(sfd,0);
    
    close(sfd);
    return EXIT_SUCCESS;
}