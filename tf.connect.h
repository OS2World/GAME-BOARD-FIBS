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

/**************************************************
 * Fugue connection header                        *
 * Needed by socket.c and (optional) tf.connect.c *
 **************************************************/

#include <netdb.h>
#ifdef WINS
# include <sys/in.h>
# include <sys/inet.h>
#else
# include <netinet/in.h>
#endif

#ifdef CONNECT_SVR4
# define CONNECT
#endif
#ifdef CONNECT_BSD
# define CONNECT
#endif

#define TFC_OK                0
#define TFC_CANT_FIND         1
#define TFC_ESOCKET           2
#define TFC_ECONNECT          3
#define TFC_ESTAT             4
#define TFC_EPIPE             5
#define TFC_ESOCKETPAIR       6
#define TFC_EPOPEN            7
#define TFC_ERECV             8
#define TFC_ERECVMSG          9

#define TFC_NERRORS           9

static char *tfc_errlist[] = {
    "",
    "Can't find host",
    "socket",
    "connect",
    TFCONNECT,
    "pipe",
    "socketpair",
    "popen",
    "recv",
    "recvmsg",
};

#define TFCERROR(num) (((num) >= 0 && (num) <= TFC_NERRORS) \
    ? tfc_errlist[(num)] : "Unknown error")

