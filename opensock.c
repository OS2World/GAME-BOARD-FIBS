/*************************************************************
 *    ______                                                 *
 *   /     /\  TinyFugue was derived from a client initially *
 *  /   __/  \ written by Anton Rang (Tarrant) and later     *
 *  |  / /\  | modified by Leo Plotkin (Grod).  The early    *
 *  |  |/    | versions of TinyFugue written by Greg Hudson  *
 *  |  X__/  | (Explorer_Bob).  The current version is       *
 *  \ /      / written and maintained by Ken Keys (Hawkeye), *
 *   \______/  who can be reached at kkeys@ucsd.edu.         *
 *                                                           *
 *             No copyright 1992, no rights reserved.        *
 *             Fugue is in the public domain.                *
 *************************************************************/

/***********************************************
 * Fugue socket opener                         *
 *                                             *
 * Used by socket.c or tf.connect.c to open    *
 * a socket and find the host.                 *
 *                                             *
 * If this were a .c file, some linkers would  *
 * link the object file into the tf executable *
 * even if it's not used.                      *
 ***********************************************/

#include <sys/socket.h>

static int get_host_address(name, addr)
    char *name;
    struct in_addr *addr;
{
    extern struct hostent *gethostbyname();
    extern unsigned long inet_addr();
    struct hostent *blob;
    union {
        long signed_thingy;
        unsigned long unsigned_thingy;
    } thingy;

    if (isdigit(*name)) {           /* IP address. */
        thingy.unsigned_thingy = addr->s_addr = inet_addr(name);
        if (thingy.signed_thingy == -1) return 0;
    } else {                        /* Host name. */
        if ((blob = gethostbyname(name)) == NULL) return 0;
        bcopy(blob->h_addr, addr, sizeof(struct in_addr));
    }
    return 1;
}

static int open_sock(host, port, addr, tfcerrnop)
    char *host, *port;
    struct sockaddr_in *addr;
    int *tfcerrnop;
{
    int fd;

    *tfcerrnop = TFC_OK;
    addr->sin_family = AF_INET;
    addr->sin_port = htons(atoi(port));

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        *tfcerrnop = TFC_ESOCKET;
    } else if (!get_host_address(host, &addr->sin_addr)) {
        close(fd);
        fd = -1;
        *tfcerrnop = TFC_CANT_FIND;
    }
    return fd;
}
