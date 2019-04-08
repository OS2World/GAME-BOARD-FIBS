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

/* For BSD 4.2 systems. */
#ifndef FD_ZERO
#define fd_set int
#define FD_SET(n, p) (*p |= (1<<(n)))
#define FD_CLR(n, p) (*p &= ~(1<<(n)))
#define FD_ISSET(n, p) (*p & (1<<(n)))
#define FD_ZERO(p) (*p = 0)
#endif
/* End of BSD 4.2 systems. */

#define SOCKDEAD     001       /* connection dead */
#define SOCKPENDING  002       /* connection not yet established */
#define SOCKLOGIN    004       /* autologin */
#define SOCKACTIVE   010       /* text has arrived but not been displayed */

typedef struct Sock {          /* an open connection to a world */
    int fd;                    /* socket to connector OR to remote */
    FILE *fp;                  /* pipe to connector */
    short flags;
    short quiet;               /* # of lines to suppress after connecting */
    struct World *world;       /* world to which socket is connected */
    struct Sock *next, *prev;
    Stringp current_output;
} Sock;


extern void FDECL(main_loop,(struct World *initial_world, int autologin));
extern int  FDECL(is_active,(int fd));
extern void FDECL(readers_clear,(int fd));
extern void FDECL(readers_set,(int fd));
extern void NDECL(background_on);
extern void FDECL(mapsock,(void FDECL((*func),(struct World *world))));
extern struct World *NDECL(fworld);
extern struct World *NDECL(xworld);
extern void FDECL(background_hook,(char *line));
extern int  FDECL(connect_to,(struct World *w, int autologin));
extern void FDECL(disconnect,(char *args));
extern void FDECL(movesock,(int dir));
extern void NDECL(disconnect_all);
extern void NDECL(listsockets);
extern void FDECL(world_output,(struct World *w, char *str, int attrs));
extern void FDECL(do_send,(char *args));
extern void FDECL(check_command,(int keyboard, Stringp str));
extern void FDECL(transmit,(char *s, int l));
extern void NDECL(clear_refresh_pending);
extern void NDECL(set_refresh_pending);
extern int  NDECL(is_refresh_pending);
extern void NDECL(set_done);
