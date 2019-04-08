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

/**************************************
 * Fugue signal handlers              *
 *                                    *
 * The following signals are trapped: *
 * SIGINT                             *
 * SIGTSTP                            *
 * SIGSEGV                            *
 * SIGBUS                             *
 * SIGQUIT                            *
 * SIGILL                             *
 * SIGTRAP                            *
 * SIGFPE                             *
 * SIGWINCH                           *
 * SIGPIPE                            *
 **************************************/

#include <stdio.h>
#include <signal.h>
#include "tf.h"
#include "dstring.h"
#include "util.h"
#include "history.h"
#include "world.h"
#include "process.h"
#include "socket.h"
#include "keyboard.h"
#include "output.h"

#define TEST_SIG(sig) (pending_signals & (1 << ((sig) - 1)))
#define SET_SIG(sig) (pending_signals |= (1 << ((sig) - 1)))
#define CLR_SIG(sig) (pending_signals &= ~(1 << ((sig) - 1)))
#define ZERO_SIG() (pending_signals = 0)

static unsigned long pending_signals;

extern int visual;

static void NDECL(sigint_run);
#ifndef WINS
# ifdef SIGTSTP
static void NDECL(sigtstp_run);
# endif
#endif
#ifndef NOCOREHANDLERS
static void FDECL(core_handler,(int sig));
static void FDECL(coremsg,(int sig));
#endif
static void FDECL(ignore_signal,(int sig));
static void FDECL(signal_scheduler,(int sig));

void NDECL(process_signals);
void NDECL(init_signals);


void init_signals()
{
    ZERO_SIG();
    signal(SIGINT  , signal_scheduler);
#ifndef WINS
# ifdef SIGTSTP
    signal(SIGTSTP , signal_scheduler);
# endif
#endif
#ifndef NO_COREHANDLERS
    signal(SIGSEGV , core_handler);
    signal(SIGBUS  , core_handler);
    signal(SIGQUIT , core_handler);
    signal(SIGILL  , core_handler);
    signal(SIGTRAP , core_handler);
    signal(SIGFPE  , core_handler);
#endif
    signal(SIGPIPE , ignore_signal);
#ifdef SIGWINCH
    signal(SIGWINCH, signal_scheduler);
#endif
}

static void ignore_signal(sig)
    int sig;
{
    signal(sig, ignore_signal);
}

static void sigint_run()
{
    int c;
    extern int borg;

    if (visual) fix_screen();
    else clear_input_window();
    printf("C) continue; X) exit; T) disable triggers; P) kill processes ");
    fflush(stdout);
    c = getch();
    clear_input_window();
    if (strchr("xyXY", c)) die("Interrupt, exiting.\n");
    setup_screen();
    do_replace();
    if (c == 't' || c == 'T') {
        borg = FALSE;
        oputs("% Cyborg triggers disabled");
    } else if (c == 'p' || c == 'P') {
        kill_procs();
    }
}

#ifndef WINS
# ifdef SIGTSTP
static void sigtstp_run()
{
    extern Stringp keybuf;
 
    cooked_echo_mode();
    if (visual) fix_screen();
    kill(getpid(), SIGSTOP);
    cbreak_noecho_mode();
    if (keybuf->len) set_refresh_pending();
    setup_screen();
    do_replace();
#ifdef MAILDELAY
    check_mail();
#endif
}
# endif
#endif

#ifndef NOCOREHANDLERS
static void coremsg(sig)
    int sig;
{
    printf("Core dumped - signal %d\n", sig);
    puts("Please, if you can, examine the core and report the location");
    puts("of it to kkeys@ucsd.edu.");
}

static void core_handler(sig)
    int sig;
{
    cleanup();
    if (sig != SIGQUIT) coremsg(sig);
    signal(sig, SIG_DFL);
    kill(getpid(), sig);
}
#endif

static void signal_scheduler(sig)
    int sig;
{
    signal(sig, signal_scheduler);  /* restore handler (SysV) */
    SET_SIG(sig);                   /* set flag to deal with it later */
}

void process_signals()
{
    if (pending_signals == 0) return;

    if (TEST_SIG(SIGINT))   sigint_run();
#ifndef WINS
# ifdef SIGTSTP
    if (TEST_SIG(SIGTSTP))  sigtstp_run();
# endif
#endif
#ifdef SIGWINCH
    if (TEST_SIG(SIGWINCH)) get_window_size();
#endif
    ZERO_SIG();
}
