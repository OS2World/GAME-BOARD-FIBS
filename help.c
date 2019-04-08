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

/****************************************************************
 * Fugue help handling                                          *
 *                                                              *
 * Uses the help index to search the helpfile for a topic.      *
 *                                                              *
 * Algorithm developed by Leo Plotkin.  Code rewritten by Greg  *
 * Hudson and Ken Keys to work with rest of program.            *
 ****************************************************************/

#include <stdio.h>
#include "tf.h"
#include "dstring.h"
#include "util.h"
#include "output.h"

void FDECL(do_help,(char *what));

void do_help(what) 
    char *what;
{
    static Stringp indexfname, input;
    static int strings_inited = FALSE;
    FILE *indexfile;
    TFILE *helpfile;
    long offset = -1;
    char *place;

    if (!strings_inited) {
        Stringinit(indexfname);
        Stringinit(input);
        strings_inited = TRUE;
    }
    if (!*what) what = "summary";

    if ((helpfile = tfopen("", "HELPFILE", "r")) == NULL) return;
    Sprintf(indexfname, "tf.idx");
    if ((indexfile = fopen(indexfname->s, "r")) == NULL) {
        oputs("% Could not open help index.");
        tfclose(helpfile);
        return;
    }

    while (fgetS(input, indexfile) != NULL) {
        newline_package(input, 0);
        place = strchr(input->s, ':');
        if (place == NULL) {
            oputs("% Error in indexfile");
            return;
        } else place++;
        if (!cstrcmp(place, what)) {
            offset = atol(input->s);
            break;
        }
    }
    fclose(indexfile);
    if (offset == -1) {
        oprintf("%% Help on subject %s not found.", what);
        tfclose(helpfile);
        return;
    }

    /* find offset and skip lines starting with "@@@@" */
    tfjump(helpfile, offset);
    while (tfgetS(input, helpfile) != NULL)
        if (strncmp(input->s, "@@@@", 4) != 0) break;

    oprintf("Help on topic %s:", what);
    do {
        if (strncmp(input->s, "@@@@", 4) == 0) break;
        newline_package(input, 0);
        oputs(input->s);
    } while (tfgetS(input, helpfile) != NULL);
    tfclose(helpfile);
}
