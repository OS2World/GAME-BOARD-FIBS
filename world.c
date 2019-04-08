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

/********************************************************
 * Fugue world routines.                                *
 ********************************************************/

#include <stdio.h>
#include <ctype.h>
#include "tf.h"
#include "dstring.h"
#include "util.h"
#include "history.h"
#include "world.h"
#include "output.h"
#include "macro.h"
#include "process.h"

static void FDECL(free_fields,(World *w));
static void FDECL(replace_world,(World *old, World *new));
static World *FDECL(insertworld,(World *world));

void   FDECL(addworld,(char *args));
World *FDECL(new_world,(char *name, char *character, char *pass,
             char *address, char *port, char *mfile));
void   FDECL(remove_world,(char *args));
void   FDECL(purge_world,(char *args));
void   FDECL(write_worlds,(char *args));
void   FDECL(list_worlds,(int full, char *pattern, TFILE *fp));
void   FDECL(free_world,(World *w));
void   FDECL(nuke_world,(World *w));
World *NDECL(get_default_world);
World *NDECL(get_world_header);
World *FDECL(find_world,(char *name));
void   FDECL(flush_world,(World *w));

static World *hworld = NULL, *defaultworld = NULL;

static void free_fields(w)
    World *w;
{
    FREE(w->name);
    FREE(w->character);
    FREE(w->pass);
    FREE(w->address);
    FREE(w->port);
    FREE(w->mfile);
}

void free_world(w)
    World *w;
{
    free_fields(w);
    free_history(w->history);
    free_queue(w->queue);
    FREE(w);
}

static void replace_world(old, new)
    World *old, *new;
{
    free_fields(old);
    old->name      = new->name;
    old->character = new->character;
    old->pass      = new->pass;
    old->address   = new->address;
    old->port      = new->port;
    old->mfile     = new->mfile;
    FREE(new);
    do_hook(H_REDEF, "%% Redefined %s %s", "%s %s", "world", old->name);
}

World *new_world(name, character, pass, address, port, mfile)
    char *name, *character, *pass, *address, *port, *mfile;
{
    World *result;
 
    result = (World *) MALLOC(sizeof(World));
    init_queue(result->queue);
    result->socket = NULL;
    result->history->alines = NULL;
    result->name      = STRDUP(name);
    result->character = STRDUP(character);
    result->pass      = STRDUP(pass);
    result->address   = STRDUP(address);
    result->port      = STRDUP(port);
    result->mfile     = STRDUP(mfile);
    result->flags = 0;
    return insertworld(result);
}

void addworld(args)
    char *args;
{
    int count, i;
    static Stringp fields[6];
    static int fields_inited = FALSE;
    World *new;

    if (!fields_inited) {
        for (i = 0; i < 6; i++) Stringinit(fields[i]);
        fields_inited = TRUE;
    }
    for (count = 0; count < 6; count++) {
        if (!*args) break;
        args = getword(fields[count], args);
    }
    if (count < 3 || count > 6) {
        oputs("% Illegal world format");
        return;
    } else if (count >= 5) {
        new = new_world(fields[0]->s, fields[1]->s, fields[2]->s, fields[3]->s,
            fields[4]->s, count == 6 ? fields[5]->s : "");
    } else if (cstrcmp(fields[0]->s, "default") == 0) {
        new = new_world(fields[0]->s, fields[1]->s, fields[2]->s, "", "",
            count == 4 ? fields[3]->s : "");
    } else {
        new = new_world(fields[0]->s, "", "", fields[1]->s, fields[2]->s,
            count == 4 ? fields[3]->s : "");
    }
    new->flags &= ~WORLD_TEMP;
}

static World *insertworld(world)
    World *world;
{
    World *w;

    if (cstrcmp(world->name, "default") == 0) {
        if (defaultworld) replace_world(defaultworld, world);
        else defaultworld = world;
    } else if ((w = find_world(world->name)) != NULL) {
        replace_world(w, world);
        return w;
    } else if (hworld == NULL) {
        hworld = world;
        world->next = NULL;
    } else {
        for (w = hworld; w->next != NULL; w = w->next);
        w->next = world;
        world->next = NULL;
    }
    return world;
}

void nuke_world(w)
    World *w;
{
    World *t;

    if (w->socket) {
        oprintf("%% %s: Cannot nuke world currently in use.", w->name);
    } else {
        if (w == hworld) hworld = w->next;
        else {
            for (t = hworld; t->next != w; t = t->next);
            t->next = w->next;
        }
        kill_procs_by_world(w);
        free_world(w);
    }
}

void remove_world(args)
    char *args;
{
    World *w;

    w = find_world(args);
    if (w == NULL) oprintf("%% No world %s", args);
    else nuke_world(w);
}

void purge_world(args)
    char *args;
{
    World *world, *next;

    if (!smatch_check(args)) return;
    for (world = hworld; world; world = next) {
        next = world->next;
        if (equalstr(args, world->name)) nuke_world(world);
    }
}

void list_worlds(full, pattern, fp)
    int full;
    char *pattern;
    TFILE *fp;
{
    World *p;
    int first = 1;
    STATIC_BUFFER(buf)

    for (p = defaultworld; p || first; p = first ? hworld : p->next, first=0) {
        if (!p || pattern && !equalstr(pattern, p->name)) continue;
        Sprintf(buf, fp ? "/addworld %s " : "%% %s ", p->name);
        if (*p->character) {
            Stringcat(buf, p->character);
            Stringadd(buf, ' ');
            if (full) {
                Stringcat(buf, p->pass);
                Stringadd(buf, ' ');
            }
        }
        if (*p->address) {
            Sprintf(buf, "\200%s %s ", p->address, p->port);
        }
        if (*p->mfile) {
            Stringcat(buf, p->mfile);
        }
        if (fp) {
            Stringadd(buf, '\n');
            tfputs(buf->s, fp);
        } else oputs(buf->s);
    }
}

void write_worlds(args)
    char *args;
{
    TFILE *file;

    if ((file = tfopen(args, "WORLDFILE", "w")) == NULL) return;
    list_worlds(TRUE, NULL, file);
    tfclose(file);
}

World *get_default_world()
{
    return defaultworld;
}

World *get_world_header()
{
    return hworld;
}

World *find_world(name)
    char *name;
{
    World *p;

    for (p=hworld; p && (!p->name || cstrcmp(name, p->name) != 0); p = p->next);
    return p;
}

void flush_world(w)
    World *w;
{
    Textnode *node;
    extern Queue screen_queue[1];

    while (node = w->queue->head) {
        /* moving the node saves free/malloc of dequeue/enqueue */
        if (screen_queue->head)
            screen_queue->tail->next = node;
        else screen_queue->head = node;
        if (!(w->queue->head = w->queue->head->next)) w->queue->tail = NULL;
        (screen_queue->tail = node)->next = NULL;
    }
    w->history->index = w->history->pos;
    oflush();
}

