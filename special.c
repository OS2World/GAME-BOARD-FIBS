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

/**********************************************************************
 * Fugue filtering functions                                          *
 *                                                                    *
 * Written by Greg Hudson and Ken Keys.                               *
 * Triggers, portals, watchdog, and quiet login are all handled here. *
 **********************************************************************/

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

static void FDECL(extract_first_word,(char *what, Stringp str));
static int  FDECL(keep_quiet,(char *what, short *count));
static int  FDECL(handle_portal,(char *what));

short FDECL(special_hook,(struct History *q, char *what, short *count));

static int keep_quiet(what, count)
    char *what;
    short *count;
{
    if (!*count) return FALSE;
    if (!cstrncmp(what, "Use the WHO command", 19) ||
      !cstrncmp(what, "### end of messages ###", 23)) {
        *count = 0;
    } else (*count)--;
    return TRUE;
}

short special_hook(q, what, count)
    struct History *q;
    char *what;
    short *count;
{
    short attr = 0;
    extern Stringp lastname;           /* person who last acted */
    extern int borg, hilite, gag;

    if ((borg || hilite || gag) && ((attr = check_trigger(what)) & F_GAG & gag))
        return attr & F_SUPERGAG;
    if (keep_quiet(what, count)) return F_GAG;
    if (is_suppressed(q, what)) return F_GAG;
    if (handle_portal(what)) return F_GAG;
    extract_first_word(what, lastname);
    return attr;
}

static void extract_first_word(what, str)
    char *what;
    Stringp str;
{
    char *place;

    place = strchr(what, ' ');
    if (place == NULL) place = strchr(what, ',');
    if (place == NULL) place = strchr(what, '(');
    if (place != NULL) Stringncpy(str, what, place - what);
    else Stringcpy(str, what);
}

static int handle_portal(what)
    char *what;
{
    smallstr name, address, port;
    char *buffer, *place;
    World *world;
    extern int bamf;

    if (!bamf) return(0);
    if (sscanf(what, "#### Please reconnect to %64s (%64[^ )]) port %64s ####",
		 name, address, port) != 3) return 0;

    place = strchr(name, '@');              /* Get IP address. */
    if (place == NULL) return (0);
    *place++ = '\0';

    if (bamf == 1) {
        buffer = MALLOC(strlen(name) + 2);
        *buffer = '@';
        strcpy(buffer + 1, name);
        world = fworld();
        world = new_world(buffer, world->character, world->pass,
            (isdigit(*place) ? place : address), port, world->mfile);
        FREE(buffer);
        world->flags |= WORLD_TEMP;
    } else if (!(world = find_world(name))) {
        world = new_world(name, "", "", isdigit(*place) ? place : address,
            port, "");
        world->flags |= WORLD_TEMP;
    }

    do_hook(H_BAMF, "%% Bamfing to %s", "%s", name);
    if (bamf == 1) disconnect("");
    if (!connect_to(world, TRUE))
        oputs("% Connection through portal failed.");
    return 1;
}
