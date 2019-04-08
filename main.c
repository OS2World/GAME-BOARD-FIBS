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
 * Fugue main routine                          *
 *                                             *
 * Initializes many internal global variables, *
 * determines initial world (if any), reads    *
 * configuration file, and calls main loop in  *
 * socket.c                                    *
 ***********************************************/

#include <stdio.h>
#include <ctype.h>
#include "tf.h"
#include "dstring.h"
#include "util.h"
#include "history.h"
#include "world.h"
#include "socket.h"
#include "macro.h"
#include "output.h"
#include "signal.h"
#include "keyboard.h"
#include "command1.h"
#include "command2.h"

char version[] = "TinyFugue version 2.1 beta 2 (FIBS patch 1.003)";
static int autologin = TRUE;

static void   FDECL(read_configuration,(char *fname));
static World *FDECL(boot_world,(int argc, char *argv[]));

int main(argc, argv)
    int argc;
    char *argv[];
{
    char *opt, *argv0 = argv[0], *configfile = NULL;
    int opterror = FALSE;
    int worldflag = TRUE;

    while (--argc > 0 && (*++argv)[0] == '-') {
        for (opt = *argv + 1; *opt; )
            switch (*opt++) {
            case 'l':
                autologin = FALSE;
                break;
            case 'n':
                worldflag = FALSE;
                break;
            case 'f':
                if (configfile) FREE(configfile);
                configfile = STRDUP(opt);
                while (*opt) opt++;
                break;
            default:
                opterror = TRUE;
                break;
            }
    }
    if (opterror || argc > 2) {
        char usage[256];
        sprintf(usage,
            "Usage: %s [-f<file>] [-ln] [<world>]\n       %s [-f<file>] <host> <port>\n",
            argv0, argv0);
        die(usage);
    }

    init_table();                            /* util.c     */
    init_signals();                          /* signal.c   */
    init_term();                             /* output.c   */
    init_histories();                        /* history.c  */
    init_prefixes();                         /* command2.c */
    init_macros();                           /* macro.c    */
#ifndef BSD
    srand(getpid());
#else
    srandom(getpid());
#endif
    init_keyboard();                         /* keyboard.c */
    oprintf("Welcome to %s", version);
    read_configuration(configfile);
    main_loop(worldflag ? boot_world(argc, argv) : NULL, autologin);
    return 0;
}

static World *boot_world(argc, argv)
    register int argc;
    register char *argv[];
{
    World *temp;

    if (argc == 0)
        temp = get_world_header();
    else if (argc == 1) {
        if ((temp = find_world(argv[0])) == NULL)
            oprintf("%% The world %s is unknown.",argv[0]);
    } else { /* (argc <= 2) guaranteed by main() */
        temp = new_world("(unnamed)", "", "", argv[0], argv[1], "");
        temp->flags |= WORLD_TEMP;
    }
    return temp;
}

static void read_configuration(fname)
    char *fname;
{
    Stringp filename;
    TFILE *file;

    do_file_load(TFLIBRARY);

    if (fname) {
        if (*fname) do_file_load(fname);
        return;
    }

    do_file_load(PRIVATEINIT);

    Stringinit(filename);
    Stringcpy(filename, (fname = getenv("TINYTALK")) ? fname : CONFIGFILE);
    Stringexpand(filename);
    if (file = tfopen(filename->s, NULL, "r")) {
        do_hook(H_LOAD, "%% Loading commands from %S.", "%S", filename);
        read_file_commands(file, TRUE);
        tfclose(file);
    }
    Stringfree(filename);
}

