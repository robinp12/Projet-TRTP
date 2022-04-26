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

    // This is not an error per-se.
    ERROR("Sender has following arguments: filename is %s, stats_filename is %s, fec_enabled is %d, receiver_ip is %s, receiver_port is %u",
          filename, stats_filename, fec_enabled, receiver_ip, receiver_port);


    /* From ingnious "Envoyer et recevoir des données" */

    char str[INET6_ADDRSTRLEN];
    inet_pton(AF_INET6, receiver_ip, &(receiver_addr.sin6_addr));
    inet_ntop(AF_INET6, &(receiver_addr.sin6_addr), str, INET6_ADDRSTRLEN);
    
    /* Resolve the hostname */
    const char *err = real_address(receiver_ip, &receiver_addr);
    if (err)
    {
        ERROR("Could not resolve hostname %s: %s\n", receiver_ip, err);
        return EXIT_FAILURE;
    }

    /* Get a socket */
    sfd = create_socket(NULL, -1, &receiver_addr, receiver_port); /* Bound */
    if (sfd < 0)
    {
        ERROR("Failed to create the socket : %s", strerror(errno));
        return EXIT_FAILURE;
    }

    if (filename != NULL)
    {
        fd = open(filename, O_RDONLY);
        if (fd == -1)
        {
            ERROR("Unable to open file, error: %s", strerror(errno));
            return errno;
        }
    }
    else
    {
        fd = STDIN_FILENO;
    }

    int fd_stats;
    if (stats_filename != NULL)
    {
        fd_stats = open(stats_filename, O_WRONLY | O_CREAT, S_IRWXO | S_IRWXU);
        if (fd_stats == -1)
        {
            ERROR("Unable to open stats file : %s", strerror(errno));
            return errno;
        }
    } else
    {
        fd_stats = STDERR_FILENO;
    }

    /* Process I/O */
    read_write_sender(sfd, fd, fd_stats, fec_enabled);

    if (fd_stats != STDERR_FILENO)
    {
        close(fd_stats);
    }
    close(fd);
    close(sfd);

    return EXIT_SUCCESS;
}
