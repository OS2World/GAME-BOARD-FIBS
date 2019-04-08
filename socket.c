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

/***************************************************************
 * Fugue socket handling                                       *
 *                                                             *
 * Rewritten by Ken Keys to do non-blocking connect(), handle  *
 * background sockets, and do more efficient process running.  *
 * Reception and transmission through sockets is handled here. *
 * This module also contains the main loop.                    *
 * Multiple sockets handled here.                              *
 * Autologin handled here.                                     *
 ***************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/ioctl.h>
#ifdef CONNECT_SVR4
# include <stropts.h>
#endif
#ifdef CONNECT_BSD
# include <sys/socket.h>
# include <sys/uio.h>
#endif
#include "tf.h"
#include "dstring.h"
#include "util.h"
#include "history.h"
#include "world.h"
#include "socket.h"
#include "output.h"
#include "process.h"
#include "macro.h"
#include "keyboard.h"
#include "command1.h"
#include "command2.h"
#include "special.h"
#include "signal.h"
#include "tf.connect.h"
#ifndef CONNECT
#include "opensock.c"
#endif

static void FDECL(wload,(World *w));
static void FDECL(announce_world,(Sock *s));
static void NDECL(bg_sock);
static void FDECL(fg_sock,(Sock *sock));
static void NDECL(any_sock);
static int  FDECL(establish,(Sock *new));
static void NDECL(nuke_dead_socks);
static void FDECL(killsock,(Sock *sock));
static void FDECL(nukesock,(Sock *sock));
static int  FDECL(receive,(Sock *sock, Stringp s));
static void NDECL(magic_login);
static void FDECL(process_socket_input,(Sock *sock, Stringp out));
static void FDECL(handle_socket_input,(Sock *sock));
static void FDECL(flush_output,(Sock *sock));
#ifdef CONNECT
static int  FDECL(nb_connect,(char *address, char *port, FILE **fpp,
            int *tfcerrnop));
static int  FDECL(recvfd,(int sockfd, int *tfcerrnop));
#endif

void   FDECL(main_loop,(World *initial_world, int autologin));
void   FDECL(readers_clear,(int fd));
void   FDECL(readers_set,(int fd));
void   NDECL(background_on);
void   FDECL(mapsock,(void FDECL((*func),(World *world))));
World *NDECL(fworld);
World *NDECL(xworld);
int    FDECL(connect_to,(World *w, int autologin));
void   FDECL(disconnect,(char *args));
void   FDECL(movesock,(int dir));
void   NDECL(disconnect_all);
void   NDECL(listsockets);
void   FDECL(world_output,(World *w, char *str, int attrs));
void   FDECL(check_command,(int keyboard, Stringp str));
void   FDECL(transmit,(char *s, int l));
void   NDECL(clear_refresh_pending);
void   NDECL(set_refresh_pending);
int    NDECL(is_refresh_pending);
void   NDECL(set_done);

#define CONN_WAIT 500000
#ifndef LP_UWAIT
#define LP_UWAIT 250000
#endif
#ifndef LP_SWAIT
#define LP_SWAIT 0
#endif
#ifndef PROC_WAIT
#define PROC_WAIT 100000
#endif
#ifndef REFRESH_TIME
#define REFRESH_TIME 250000
#endif

extern int lpflag, quitdone, bamf, borg, hilite, gag, quiet;
extern int sockmload, lpquote;
extern int login;                       /* Auto-logins enabled? */
extern int background;                  /* background processing enabled? */
extern int input_cursor;                /* is cursor in input window? */
extern TIME_T proctime;                 /* when next process should run */

static fd_set readers;                  /* file descriptors we're watching */
static fd_set active;                   /* active file descriptors */
static int nfds;                        /* max # of readers */
static Sock *hsock = NULL;              /* head of socket list */
static Sock *tsock = NULL;              /* tail of socket list */
static int done = FALSE;                /* Are we all done? */
static int need_refresh;                /* Does input line need refresh? */
static int dead_sock_pending = 0;       /* Number of unnuked dead sockets */

Sock *fsock = NULL;                     /* foreground socket */
Sock *xsock = NULL;                     /* current (transmission) socket */
int active_count = 0;                   /* # of (non-current) active sockets */

void main_loop(initial_world, autologin)
    World *initial_world;
    int autologin;
{
    int count;
    struct timeval tv, *tvp;
    TIME_T now, earliest, mailtime = 0;
    Sock *sock;

    FD_ZERO(&readers);
    FD_ZERO(&active);
    FD_SET(0, &readers);
    nfds = 1;
    if (initial_world != NULL) connect_to(initial_world, autologin);

    while (!done) {
        now = (TIME_T)time(NULL);
        if (!lpquote && now >= proctime) runall(now);
        earliest = proctime;
        if (need_refresh || !input_cursor) {
            tvp = &tv;
            tv.tv_sec = 0;
            tv.tv_usec = REFRESH_TIME;
        } else if (earliest) {
            tvp = &tv;
            tv.tv_sec = earliest - now;
            tv.tv_usec = 0;
            if (tv.tv_sec <= 0) {
                tv.tv_sec = 0;
            } else if (tv.tv_sec == 1) {
                tv.tv_sec = 0;
                tv.tv_usec = PROC_WAIT;
            }
        } else tvp = NULL;

        active = readers;
        count = select(nfds, &active, NULL, NULL, tvp);

        if (count == -1) {
            if (errno != EINTR) {
                operror("TF/main_loop/select");
                die("% Failed select");
            }
        } else if (count == 0) {
            if (need_refresh) do_line_refresh();
            else if (!input_cursor) ipos();
        } else {
            if (FD_ISSET(0, &active)) {
                count--;
                if (need_refresh) do_line_refresh();
                else if (!input_cursor) ipos();
                handle_keyboard_input();
            }
            if (count && fsock && FD_ISSET(fsock->fd, &active)) {
                count--;
                handle_socket_input(fsock);
                FD_CLR(fsock->fd, &active);
            }
            for (sock = hsock; count && sock; sock = sock->next) {
                if (FD_ISSET(sock->fd, &active)) {
                    count--;
                    if (sock->flags & SOCKPENDING) {
                        establish(sock);
                    } else {
                        if (background) {
                            handle_socket_input(xsock = sock);
                            xsock = fsock;
                        } else FD_CLR(sock->fd, &readers);
                    }
                }
            }
        }
        process_signals();
        if (dead_sock_pending) nuke_dead_socks();
    } 
    cleanup();
#ifdef DMALLOC
    handle_purge_command("-i *");
    purge_world("*");
    free_keyboard();
    free_prefixes();
    free_histories();
    free_term();
    debug_mstats("tf");
#endif
}

int is_active(fd)
    int fd;
{
    return FD_ISSET(fd, &active);
}

void readers_clear(fd)
    int fd;
{
    FD_CLR(fd, &readers);
}

void readers_set(fd)
    int fd;
{
    FD_SET(fd, &readers);
    if (fd >= nfds) nfds = fd + 1;
}

/* find open socket to world <name> */
static Sock *find_sock(name)
    char *name;
{
    Sock *sock;

    for (sock = hsock; sock; sock = sock->next) {
        if (sock->flags & (SOCKDEAD | SOCKPENDING)) continue;
        if (!name || cstrcmp(sock->world->name, name) == 0) break;
    }
    return sock;
}

void background_on()
{
    Sock *sock;
    for (sock = hsock; sock; sock = sock->next)
        if (!(sock->flags & (SOCKDEAD | SOCKPENDING)))
            FD_SET(sock->fd, &readers);
}

/* Perform (*func)(world) on every open world */
void mapsock(func)
    void FDECL((*func),(World *world));
{
    Sock *sock;

    for (sock = hsock; sock; sock = sock->next)
        if (!(sock->flags & (SOCKDEAD | SOCKPENDING))) (*func)(sock->world);
}

World *fworld()
{
    return fsock ? fsock->world : NULL;
}

World *xworld()
{
    return xsock ? xsock->world : NULL;
}
  
static void wload(w)
    World *w;
{
    World *d;

    if (*w->mfile) do_file_load(w->mfile);
    else if ((d = get_default_world()) != NULL && *d->mfile)
        do_file_load(d->mfile);
}

static void announce_world(s)
    Sock *s;
{
    put_world(s ? s->world->name : NULL);
    if (!s)
        do_hook(H_WORLD, "---- No world ----", "");
    else if (s->flags & SOCKDEAD)
        do_hook(H_WORLD, "---- World %s (dead) ----", "%s", s->world->name);
    else do_hook(H_WORLD, "---- World %s ----", "%s", s->world->name);
}

static void bg_sock()
{
    /* if (!fsock) return; */
}

static void fg_sock(new)
    Sock *new;
{
    if (new) {
        FD_SET(new->fd, &readers);
        if (new->flags & SOCKACTIVE) put_active(--active_count);
        new->flags &= ~SOCKACTIVE;
        announce_world(new);
        flush_world(new->world);
#ifndef OLD_LPPROMPTS
        if (lpflag) {
            if (new->current_output->len) setprompt(new->current_output->s);
            Stringterm(new->current_output, 0);
        }
#endif
        if (new->flags & SOCKDEAD) {
            nukesock(new);
            fsock = xsock = NULL;
            any_sock();
        } else xsock = fsock = new;
    } else announce_world(NULL);
}

int connect_to(w, autologin)                    /* initiate a connection. */
    World *w;
    int autologin;
{
    int fd, count, tfcerrno = TFC_OK;
    FILE *fp;
    Sock *sock;
    struct timeval tv;
    fd_set pending;
#ifndef CONNECT
    struct sockaddr_in addr;
#endif

    for (sock = hsock; sock != NULL; sock = sock->next) {
        if (sock->world == w &&
          (!(sock->flags & SOCKDEAD) || sock->flags & SOCKACTIVE)) {
            if (sock == fsock) return 1;
            if (sock->flags & SOCKPENDING) {
                oputs("% Connection already in progress.");
                return 0;  /* ??? */
            }
            bg_sock();
            fg_sock(sock);
            if (sock == fsock && sockmload) wload(sock->world);
            return 1;
        }
    }

#ifdef CONNECT
    fd = nb_connect(w->address, w->port, &fp, &tfcerrno);
#else
    fd = open_sock(w->address, w->port, &addr, &tfcerrno);
#endif
    if (fd < 0) {
        do_hook(H_CONFAIL, "%% Connection to %s failed: %s: %s", "%s %s: %s",
            w->name, TFCERROR(tfcerrno),
            tfcerrno == TFC_CANT_FIND ? w->address : STRERROR(errno));
        return 0;
    }
#ifndef CONNECT
    if (connect(fd, &addr, sizeof(struct sockaddr_in)) < 0) {
        close(fd);
        do_hook(H_CONFAIL, "%% Connection to %s failed: %s: %s", "%s %s: %s",
            w->name, TFCERROR(TFC_ECONNECT),
            tfcerrno == TFC_CANT_FIND ? w->address : STRERROR(errno));
        return 0;
    }
#endif
    if (fd >= nfds) nfds = fd + 1;
    FD_SET(fd, &readers);
    sock = (Sock *) MALLOC(sizeof(struct Sock));
    if (hsock == NULL) {
        sock->prev = NULL;
        tsock = hsock = sock;
    } else {
        sock->prev = tsock;
        tsock = tsock->next = sock;
    }
    sock->fd = fd;
    sock->flags = (autologin ? SOCKLOGIN : 0);
    sock->world = w;
    sock->world->socket = sock;
    Stringinit(sock->current_output);
    sock->next = NULL;
#ifdef CONNECT
    sock->flags |= SOCKPENDING;
    sock->fp = fp;
    FD_SET(fd, &pending);
    tv.tv_sec = 0;
    tv.tv_usec = CONN_WAIT;                /* give it a chance */
    count = select(fd + 1, &pending, NULL, NULL, &tv);
    if (count == 1) {
        return establish(sock);
    } else {
        do_hook(H_PENDING, "% Connection to %s in progress.", "%s",
            sock->world->name);
        return 1;  /* ??? */
    }
#else
    return establish(sock);
#endif
}

static int establish(sock)     /* establish a pending connection */
    Sock *sock;
{
    int fd;

#ifdef CONNECT
    int tfcerrno;

    sock->flags &= ~SOCKPENDING;
    FD_CLR(sock->fd, &readers);
    pclose(sock->fp);                /* Let child die.  We don't need status. */
    if (sock->flags & SOCKDEAD) {    /* Sock was killed already. Nuke it now. */
        nukesock(sock);
        return 0;
    }
    fd = recvfd(sock->fd, &tfcerrno);
    if (fd < 0) {
        do_hook(H_CONFAIL, "%% Connection to %s failed: %s: %s", "%s %s: %s",
            sock->world->name, TFCERROR(tfcerrno),
            tfcerrno == TFC_CANT_FIND ? sock->world->address : STRERROR(errno));
        killsock(sock);
        return 0;
    }
    close(sock->fd);
    sock->fd = fd;
    if (fd >= nfds) nfds = fd + 1;
    FD_SET(fd, &readers);
#endif
    sock->quiet = quiet ? MAXQUIET : 0;
    /* skip any old undisplayed lines */
    sock->world->history->index = sock->world->history->pos;
    bg_sock();
    fg_sock(sock);
    wload(fsock->world);
    do_hook(H_CONNECT, NULL, "%s", fsock->world->name);
    magic_login();
    return 1;
}

void movesock(dir)
    int dir;
{
    Sock *sock, *stop;

    reset_outcount();
    if (fsock == NULL) return;
    stop = sock = fsock;
    do {
        if (dir > 0) sock = sock && sock->next ? sock->next : hsock;
        else sock = sock && sock->prev ? sock->prev : tsock;
    } while ((sock->flags & SOCKPENDING) && sock != stop);
    if (sock == fsock) return;
    bg_sock();
    fg_sock(sock);
    if (sockmload) wload(fsock->world);
}

static void killsock(sock)
    Sock *sock;
{
    sock->flags |= SOCKDEAD;
    dead_sock_pending++;
}

static void nukesock(sock)
    Sock *sock;
{
    sock->world->socket = NULL;
    if (sock->flags & SOCKPENDING) return;
    if (sock->world->flags & WORLD_TEMP) {
        nuke_world(sock->world);
        sock->world = NULL;
    }
    if (sock == hsock) hsock = sock->next;
    else sock->prev->next = sock->next;
    if (sock == tsock) tsock = sock->prev;
    else sock->next->prev = sock->prev; dead_sock_pending--;
    FD_CLR(sock->fd, &readers);
    close(sock->fd);
    if (sock->flags & SOCKACTIVE) put_active(--active_count);
    Stringfree(sock->current_output);
    FREE(sock);
}

static void nuke_dead_socks()
{
    Sock *sock, *next;
    int reconnect = FALSE;

    if (fsock && (fsock->flags & SOCKDEAD)) {
        xsock = fsock = NULL;
        reconnect = TRUE;
    }
    for (sock = hsock; sock; sock = next) {
        next = sock->next;
        if (sock->flags & SOCKDEAD) {
            if (sock->flags & SOCKACTIVE) FD_CLR(sock->fd, &readers);
            else nukesock(sock);
        }
    }
    if (quitdone && !hsock) done = 1;
    else if (reconnect) any_sock();
}

static void any_sock()
{
    Sock *sock;

    if (sock = find_sock(NULL)) {
        fg_sock(sock);
        if (sockmload) wload(fsock->world);
    } else announce_world(NULL);
}

void disconnect_all()
{
    Sock *sock, *next;

    for (sock = hsock; sock; sock = next) {
        next = sock->next;
        nukesock(sock);
    }
    hsock = tsock = xsock = fsock = NULL;
    if (quitdone) done = 1;
}

void disconnect(args)
    char *args;
{
    Sock *s;

    if (!*args) {
        if (fsock) killsock(fsock);
    } else if (cstrcmp(args, "-all") == 0) {
        if (hsock) {
            disconnect_all();
            announce_world(NULL);
        }
    } else {
        for (s = hsock; s; s = s->next) {
            if (cstrcmp(s->world->name, args) == 0 && !(s->flags & SOCKDEAD))
                break;
        }
        if (s) {
            oprintf ("%% Connection to %s closed.", s->world->name);
            killsock(s);
        } else oprintf("%% Not connected to %s", args);
    }
}

void listsockets()
{
    Sock *sock;
    char *state, buffer[81];
    int defunct;

    if (hsock == NULL) {
        oputs("% Not connected to any sockets.");
        return;
    }

    for (sock = hsock; sock != NULL; sock = sock->next) {
        defunct = (sock->flags & SOCKPENDING) && (sock->flags & SOCKDEAD);
        if (sock == xsock) state = "current";
        else if (defunct) state = "defunct";
        else if (sock->flags & SOCKPENDING) state = "pending";
        else if (sock->flags & SOCKDEAD) state = "dead";
        else if (sock->flags & SOCKACTIVE) state = "active";
        else state = "idle";
        sprintf(buffer, "%% [%7s]  %15s %30s %s", state,
            sock->world->name, sock->world->address, sock->world->port);
        oputs(buffer);
    }
}

void background_hook(line)
    char *line;
{
    if (xsock == fsock || background > 1) return;
    do_hook(H_BACKGROUND, "%% Trigger in world %s", "%s %s",
        xsock->world->name, line);
}

void do_send(args)
    char *args;
{
    Sock *save = xsock, *sock = xsock;
    int len = -1, opt, Wflag = FALSE;

    if (!hsock) {
        oputs("% Not connected to any sockets.");
        return;
    }
    startopt(args, "w:W");
    while (opt = nextopt(&args, NULL)) {
        switch (opt) {
        case 'w':
            if (*args) {
                if (!(sock = find_sock(args))) {
                    oprintf("%% Not connected to %s", args);
                    return;
                }
            } else sock = xsock;
            break;
        case 'W':
            Wflag = TRUE;
            break;
        default:
            return;
        }
    }
    
    args[len = strlen(args)] = '\n';            /* be careful */
    if (Wflag) {
        for (xsock = hsock; xsock; xsock = xsock->next) transmit(args, len + 1);
    } else {
        xsock = sock;
        transmit(args, len + 1);
    }
    if (len >= 0) args[len] = '\0';    /* restore end of string */
    xsock = save;
}

void transmit(str, numtowrite)
    char *str;
    int numtowrite;
{
    int numwritten;

    if (!xsock || xsock->flags & (SOCKDEAD | SOCKPENDING)) return;
    while (numtowrite) {
        numwritten = send(xsock->fd, str, numtowrite, 0);
        if (numwritten == -1) {
            if (errno == EWOULDBLOCK) numwritten = 0;
            else {
                do_hook(H_DISCONNECT,
                    "%% Connection to %s closed by foreign host: %s",
                    "%s %s", xsock->world->name, STRERROR(errno));
                killsock(xsock);
                return;
            }
        }
        numtowrite -= numwritten;
        str += numwritten;
        if (numtowrite) sleep(1);
    }
}

void check_command(keyboard, str)
    int keyboard;
    Stringp str;
{
    if (str->s[0] == '/' && str->s[1] != '/') {
        newline_package(str, 0);
        handle_command(str->s, NULL);
    } else if (str->s[0] == '/') {
        newline_package(str, 1);
        transmit(str->s + 1, str->len - 1);
    } else if (!keyboard || !(do_hook(H_SEND, NULL, "%S", str) & F_GAG)) {
        newline_package(str, 1);
        transmit(str->s, str->len);
    }
}

static int receive(sock, str)
    Sock *sock;
    Stringp str;
{
    int count;
    char block[513];
    struct timeval tv;
    fd_set readfds;

    FD_ZERO(&readfds);
    FD_SET(sock->fd, &readfds);
#ifndef OLD_LPPROMPTS
    if (lpflag) {
        tv.tv_sec = LP_SWAIT;
        tv.tv_usec = LP_UWAIT;
    } else
#endif
        tv.tv_sec = tv.tv_usec = 0;
    while ((count = select(sock->fd + 1, &readfds, NULL, NULL, &tv)) == -1)
        if (errno != EINTR) {
            operror("TF/receive/select");
            die("% Failed select");
        }
    if (count == 0) return 0;
    do count = recv(sock->fd, block, 512, 0);
        while (count == -1 && errno == EINTR);
    if (count == -1) {
        operror("% recv failed");
        block[0] = '\0';
    } else block[count] = '\0';
    if (count <= 0) {
        flush_output(sock);
        do_hook(H_DISCONNECT, "%% Connection to %s closed.",
            "%s", sock->world->name);
        killsock(sock);
        return -1;
    }
    if (str) Stringcpy(str, block);
    return count;
}

static void magic_login()
{
    World *w;

    if (!(login && fsock->flags & SOCKLOGIN)) return;
    w = (*fsock->world->character) ? fsock->world : get_default_world();
    if (!w) return;
    if (!*w->character) return;
    do_hook(H_LOGIN, NULL, "%s %s %s", w->name, w->character, w->pass);
}

void clear_refresh_pending()
{
    need_refresh = 0;
}

void set_refresh_pending()
{
    need_refresh = 1;
}

int is_refresh_pending()
{
    return need_refresh;
}

static void process_socket_input(sock, out)
    Sock *sock;
    Stringp out;
{
    int len = out->len;
    short attrs = F_NORM;

    if (len && out->s[len - 1] == '\r') {
        while (--len && out->s[len - 1] == '\r');
        Stringterm(out, len);
    }
    attrs = special_hook(sock->world->history, out->s, &sock->quiet);
    world_output(sock->world, out->s, attrs);
}

void world_output(w, str, attrs)
    World *w;
    char *str;
    int attrs;
{
    Aline *aline;

    if (gag && (attrs & F_GAG) && (attrs & F_NORECORD) && !w->history->logfile)
        return;
    aline = new_aline(str, attrs | F_NEWLINE);
    record_world(w->history, aline);
    if (!(gag && (attrs & F_GAG))) {
        if (w == fsock->world) {
#if 0
            flush_world(w);
#endif
            localoutput(aline);
        } else {
            enqueue(w->queue, aline);
            if (!(w->socket->flags & SOCKACTIVE)) {
                w->socket->flags |= SOCKACTIVE;
                put_active(++active_count);
                do_hook(H_ACTIVITY, "%% Activity in world %s", "%s", w->name);
            }
        }
    }
}

static void handle_socket_input(sock)
    Sock *sock;
{
    static Stringp in, temp, out;
    static int strings_inited = FALSE;
    char *place;

    if (!strings_inited) {
        Stringinit(in);
        Stringinit(temp);
        Stringinit(out);
        strings_inited = TRUE;
    }

    while (receive(sock, in) > 0) {
        SStringcat(sock->current_output, in);
        while ((place = strchr(sock->current_output->s, '\n')) != NULL) {
            Stringncpy(out, sock->current_output->s,
                place - sock->current_output->s);
            Stringncpy(temp, place + 1,
                sock->current_output->s + sock->current_output->len - place);
            process_socket_input(sock, out);
            SStringcpy(sock->current_output, temp);
            if (history_full(sock->world->history)) return;
        }
    }
    if (lpflag && sock->current_output->len) {
        if (lpquote) runall(time(NULL));
        if (borg || hilite || gag) check_trigger(sock->current_output->s);
#ifndef OLD_LPPROMPTS
        if (sock == fsock) {
            setprompt(sock->current_output->s);
            Stringterm(sock->current_output, 0);
        }
#else
        process_socket_input(sock, sock->current_output);
        Stringterm(sock->current_output, 0);
#endif
    }
}

static void flush_output(sock)
    Sock *sock;
{
    if (sock->current_output->len) {
        process_socket_input(sock, sock->current_output);
        Stringterm(sock->current_output, 0);
    }
    setprompt("");
}

void set_done()
{
    done = TRUE;
}

#ifdef CONNECT
static int recvfd(sockfd, flag)
    int sockfd, *flag;
{
#ifdef CONNECT_SVR4
    struct strrecvfd rfd;

    if (ioctl(sockfd, I_RECVFD, (char*)&rfd) < 0) {
        *flag = TFC_ERECV;
    } else return rfd.fd;
    return -1;
#else
    struct iovec iov[1];
    struct msghdr msg;
    int fd, error;

    if ((recv(sockfd, flag, sizeof(*flag), 0)) < 0) {
        *flag = TFC_ERECV;
    } else if (*flag == TFC_OK) {
        iov[0].iov_base = NULL;
        iov[0].iov_len = 0;
        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;
        msg.msg_accrights = (char*) &fd;
        msg.msg_accrightslen = sizeof(fd);
        if (recvmsg(sockfd, &msg, 0) >= 0) return fd;
        *flag = TFC_ERECVMSG;
    } else {
        if ((recv(sockfd, &error, sizeof(error), 0)) < 0) *flag = TFC_ERECV;
        errno = error;
    }
    return -1;
#endif
}

/*
 * nb_connect simulates a non-blocking connect() by forking a child to
 * do the actual connect().  The child sends the connected file descriptor
 * or error code back to the parent through a stream pipe when done.
 * The parent reads the fd/error from the pipe when the pipe selects
 * true for reading.  popen() and pclose() provide a portable way of
 * wait()ing for the child (we don't use the pipe it creates).
 */

static int nb_connect(address, port, fpp, tfcerrnop)
    char *address, *port;
    FILE **fpp;
    int *tfcerrnop;
{
    int sv[2];
    struct stat statbuf;
    STATIC_BUFFER(cmd);

    *tfcerrnop = TFC_ESTAT;
    if (stat(TFCONNECT, &statbuf) == -1) return -1;
#ifdef CONNECT_SVR4
    *tfcerrnop = TFC_EPIPE;
    if (pipe(sv) < 0) return -1;
#else
    *tfcerrnop = TFC_ESOCKETPAIR;
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) return -1;
#endif
    Sprintf(cmd, "%s %s %s %d", TFCONNECT, address, port, sv[0]);
    *tfcerrnop = TFC_EPOPEN;
    if ((*fpp = popen(cmd->s, "w")) == NULL) return -1;
    close(sv[0]);                     /* parent doesn't need this */
    *tfcerrnop = TFC_OK;
    return sv[1];
}
#endif /* CONNECT */

