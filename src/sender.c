#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "log.h"
#include "packet_implem.h"
#include "read-write/read_write_sender.h"
#include "read-write/real_address.h"
#include "read-write/create_socket.h"
#include "read-write/wait_for_client.h"

int print_usage(char *prog_name)
{
    ERROR("Usage:\n\t%s [-f filename] [-s stats_filename] [-c] receiver_ip receiver_port", prog_name);
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    int opt;
    int sock = -1;
    struct sockaddr_in6 receiver_addr;

    int fd;
    int sfd;

    char *filename = NULL;
    char *stats_filename = NULL;
    char *receiver_ip = NULL;
    char *receiver_port_err;
    bool fec_enabled = false;
    uint16_t receiver_port;

    while ((opt = getopt(argc, argv, "f:s:hc")) != -1)
    {
        switch (opt)
        {
        case 'f':
            filename = optarg;
            break;
        case 'h':
            return print_usage(argv[0]);
        case 's':
            stats_filename = optarg;
            break;
        case 'c':
            fec_enabled = true;
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

    receiver_ip = argv[optind];
    receiver_port = (uint16_t)strtol(argv[optind + 1], &receiver_port_err, 10);
    if (*receiver_port_err != '\0')
    {
        ERROR("Receiver port parameter is not a number");
        return print_usage(argv[0]);
    }

    ASSERT(1 == 1);               // Try to change it to see what happens when it fails
    DEBUG_DUMP("Some bytes", 11); // You can use it with any pointer type

    // This is not an error per-se.
    ERROR("Sender has following arguments: filename is %s, stats_filename is %s, fec_enabled is %d, receiver_ip is %s, receiver_port is %u",
          filename, stats_filename, fec_enabled, receiver_ip, receiver_port);

    DEBUG("You can only see me if %s", "you built me using `make debug`");
    ERROR("This is not an error, %s", "now let's code!");

    /* From ingnious "Envoyer et recevoir des données" */

    /* Resolve the hostname */
    const char *err = real_address(receiver_ip, &receiver_addr);
    if (err)
    {
        ERROR("Could not resolve hostname %s: %s\n", receiver_ip, err);
        return EXIT_FAILURE;
    }

    /* Get a socket */
    sfd = create_socket(NULL, -1, &receiver_addr, receiver_port); /* Bound */
    DEBUG("Waiting for client");
    if (sfd > 0 && wait_for_client(sfd) < 0)
    { /* Connected */
        ERROR("Could not connect the socket after the first message.\n");
        close(sfd);
        return EXIT_FAILURE;
    }
    DEBUG("Socket connected");

    if (sfd < 0)
    {
        ERROR("Failed to create the socket : %s", strerror(errno));
        return EXIT_FAILURE;
    }

    fd = open(filename, O_RDONLY);
    if (!fd)
    {
        ERROR("Unable to open file, error: %s", strerror(errno));
        return errno;
    }

	/* Process I/O */
	read_write_sender(sfd, fd);

    close(fd);
	close(sfd);

    return EXIT_SUCCESS;
}
