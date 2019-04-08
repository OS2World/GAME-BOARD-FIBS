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
 * Fugue history and logging                                    *
 *                                                              *
 * Maintains the circular lists for input and output histories. *
 * Handles text queuing and file I/O for logs.                  *
 ****************************************************************/

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

#define torange(x, r) (((x) >= 0) ? ((x)%(r)) : ((x)%(r) + (r)))
#define empty(q) (!(q)->alines || !(q)->size)

static void   FDECL(init_history,(History *q, int maxsize));
static void   FDECL(saveline,(History *q, Aline *aline));
static void   FDECL(display_history,(History *q, int start, int end,
              int numbers, char *pattern, Queue *buf, int attrs));
static int    FDECL(check_watchname,(History *q));
static int    FDECL(check_watchdog,(History *q, char *str));

void   NDECL(init_histories);
void   FDECL(free_history,(History *q));
int    FDECL(history_full,(History *q));
void   FDECL(record_world,(History *q, Aline *aline));
void   FDECL(record_local,(Aline *aline));
void   FDECL(record_input,(char *str));
int    FDECL(recall_history,(char *args, Queue *buf));
void   FDECL(recall_input,(Stringp str, int dir));
int    FDECL(is_suppressed,(History *q, char *str));
void   FDECL(history_sub,(char *pattern));
void   FDECL(do_log,(char *args));

static History input[1], global[1], local[1];

extern int log_on, gag;

Stringp lastname;

static void init_history(q, maxsize)
    History *q;
    int maxsize;
{
    q->alines = (Aline **) MALLOC(maxsize * sizeof(Aline *));
    q->maxsize = maxsize;
    q->pos = q->index = -1;
    q->size = q->num = 0;
    q->logfile = NULL;
}

void init_histories()
{
    init_history(input, SAVEINPUT);
    init_history(global, SAVEGLOBAL);
    init_history(local, SAVELOCAL);
    Stringinit(lastname);
}

#ifdef DMALLOC
void free_histories()
{
    free_history(input);
    free_history(global);
    free_history(local);
    Stringfree(lastname);
}
#endif

void free_history(q)
    History *q;
{
    int i;

    if (q->alines) {
        for (i = 0; i < q->size; i++) free_aline(q->alines[i]);
        FREE(q->alines);
        if (q->logfile) {
            tfclose(q->logfile);
            if (!--log_on) put_logging(FALSE);
        }
    }
}

static void saveline(q, aline)
    History *q;
    Aline *aline;
{
    if (!(aline->attrs & F_NORECORD)) {
        if (!q->alines) init_history(q, SAVEWORLD);
        q->pos = torange(q->pos + 1, q->maxsize);
        if (q->size < q->maxsize) q->size++;
        else free_aline(q->alines[q->pos]);
        (q->alines[q->pos] = aline)->links++;
        q->num++;
    }
    if (q->logfile) {
        tfputs(aline->str, q->logfile);
        tfputc('\n', q->logfile);
        tfflush(q->logfile);
    }
}

int history_full(q)
    History *q;
{
    return (q->alines && q->pos == torange(q->index - 1, q->size));
}

void record_local(aline)
    Aline *aline;
{
    saveline(local, aline);
    saveline(global, aline);
}

void record_world(q, aline)
    History *q;
    Aline *aline;
{
    if (q) saveline(q, aline);
    saveline(global, aline);
}

void record_input(str)
    char *str;
{
    saveline(input, new_aline(str, F_NEWLINE));
    input->index = input->pos;
}

void recall_input(s, dir)
    Stringp s;
    int dir;
{
    if (empty(input)) return;
    Stringcpy(s, input->alines[torange(input->index + 1 + dir, input->size)]->str);
    if (input->size > 1) input->index = torange(input->index + dir, input->size);
}

int recall_history(args, buf)
    char *args;
    Queue *buf;
{
    int num, start, end, numbers = FALSE;
    short attrs = 0;
    char opt, *arg, *place, *pattern;
    World *world = xworld();
    History *q = NULL;
    static Aline *startmsg = NULL, *endmsg = NULL;

    if (!startmsg) {
        startmsg = new_aline("---- Recall start ----", F_NEWLINE | F_NORECORD);
        endmsg = new_aline("----- Recall end -----", F_NEWLINE | F_NORECORD);
        startmsg->links = endmsg->links = 1;
    }
    startopt(args, "a:f:w:lgi");
    while(opt = nextopt(&arg, &num)) {
        switch (opt) {
        case 'w':
            if (!*arg || (world = find_world(arg)) == NULL) {
                oprintf("%% No world %s", arg);
                return 0;
            }
            q = world->history;
            break;
        case 'l':
            q = local;
            break;
        case 'g':
            q = global;
            break;
        case 'i':
            q = input;
            break;
        case 'a': case 'f':
            if ((num = parse_attrs(arg)) == -1) return 0;
            attrs |= num;
            break;
        default: return 0;
        }
    }
    if (!q) q = world ? world->history : global;
    if (empty(q)) return 0;
    if (arg && (pattern = strchr(arg, ' ')) != NULL) {
        *pattern++ = '\0';
        if (!smatch_check(pattern)) return 0;
    } else pattern = NULL;
    if (arg && *arg == '#') {
        numbers = TRUE;
        arg++;
    }
    if (!arg || !*arg) {
        start = end = q->pos;
    } else if (*arg == '-') {
        start = atoi(++arg);
        if (start > q->size) start = q->size;
        if (start < 1) start = 1;
        start = end = torange(q->pos - start + 1, q->size);
    } else if (!isdigit(*arg)) {
        oputs("% Bad recall syntax.");
        return 0;
    } else {
        if ((start = atoi(arg)) < 1) start = 1;
        if ((place = strchr(arg, '-')) == NULL) {
            if (start > q->size) start = q->size;
            end = q->pos;
            start = torange(q->pos - start + 1, q->size);
        } else {
            if (start > q->num) start = q->num;
            end = isdigit(place[1]) ? atoi(place + 1) : start;
            if (end < start) end = start;
            else if (end > q->num) end = q->num;
            end = torange(end - 1, q->size);
            start = torange(start - 1, q->size);
        }
    }
    if (buf == NULL) aoutput(startmsg);
    display_history(q, start, end, numbers, pattern, buf, ~attrs);
    if (buf == NULL) aoutput(endmsg);
    return 1;
}

static void display_history(q, start, end, numbers, pattern, buf, attrs)
    History *q;
    Queue *buf;
    int start, end, numbers;
    char *pattern;
    int attrs;
{
    int i, done = FALSE;
    char *str;
    Aline *aline;
    STATIC_BUFFER(buffer)

    attrs |= F_NORM;
    for (i = start; !done; i = torange(i + 1, q->size)) {
        if (i == end) done = TRUE;
        if (gag && (q->alines[i]->attrs & F_GAG & attrs)) continue;
        if (pattern && !equalstr(pattern, q->alines[i]->str)) continue;
        if (numbers) {
            Sprintf(buffer, "%d: %s", q->num - torange(q->pos - i, q->size),
                q->alines[i]->str);
            str = buffer->s;
        } else str = q->alines[i]->str;

        if (numbers || q->alines[i]->attrs & ~attrs & F_ALL) {
            aline = new_aline(str, (q->alines[i]->attrs & attrs) | F_NEWLINE);
        } else aline = q->alines[i];            /* share aline if possible */
        if (buf) {
            enqueue(buf, aline);
        } else {
            aline->attrs |= F_NORECORD;           /* don't record /recalls */
            aoutput(aline);
        }
    }
}

static int check_watchname(q)
    History *q;
{
    extern int wnmatch, wnlines, gpri;
    int pos = q->pos, size = q->size;
    int nmatches = 0, i, slines, nlen = lastname->len;
    char *name = lastname->s;

    if (wnlines > q->size) slines = q->size;
    else slines = wnlines;
    for (i = 0; i < slines; i++) {
        if (!strncmp(q->alines[torange(pos - i, size)]->str, name, nlen))
            if (++nmatches == wnmatch) break;
    }
    if (nmatches < wnmatch) return 0;
    oprintf("%% Watchname: gagging \"%S*\"", lastname);
    Stringcat(lastname, " *");
    add_macro(new_macro("",lastname->s,"",0,"","",NULL,gpri,100,F_GAG,0,0));
    Stringterm(lastname, nlen);
    return 1;
}

static int check_watchdog(q, str)
    History *q;
    char *str;
{
    extern int wdmatch, wdlines;
    int pos = q->pos, size = q->size;
    int nmatches = 0, i, slines;

    if (wdlines > q->size) slines = q->size;
    else slines = wdlines;
    for (i = 0; i < slines; i++) {
        if (!cstrcmp(q->alines[torange(pos - i, size)]->str, str))
            if (nmatches++ == wdmatch) return 1;
    }
    return 0;
}

int is_suppressed(q, str)
    History *q;
    char *str;
{
    extern int wn_enabled, wd_enabled;
 
    if (empty(q)) return 0;
    return ((wn_enabled && check_watchname(q)) ||
            (wd_enabled && check_watchdog(q, str)));
}

void history_sub(pattern)
    char *pattern;
{
    int size = input->size, pos = input->pos, i;
    Aline **l = input->alines;
    char *replace, *loc = NULL;
    STATIC_BUFFER(buffer)

    if (empty(input) || !*pattern) return;
    if ((replace = strchr(pattern, '^')) == NULL) return;
    else *replace++ = '\0';
    for (i = 0; i < size; i++)
        if ((loc = STRSTR(l[torange(pos - i, size)]->str, pattern)) != NULL)
            break;
    if (i == size) return;
    i = torange(pos - i, size);
    Stringncpy(buffer, l[i]->str, loc - l[i]->str);
    Stringcat(buffer, replace);
    Stringcat(buffer, loc + (replace - pattern - 1));
    record_input(buffer->s);
    check_command(FALSE, buffer);
}

void init_queue(q)
    Queue *q;
{
    q->head = q->tail = NULL;
}

void enqueue(q, aline)
    Queue *q;
    Aline *aline;
{
    Textnode *node;

    node = (Textnode *)MALLOC(sizeof(Textnode));
    (node->aline = aline)->links++;
    node->next = NULL;
    if (!q->head) q->head = node;
    else q->tail->next = node;
    q->tail = node;
}

Aline *dequeue(q)
    Queue *q;
{
    Textnode *node;
    Aline *result;

    if (!q->head) return NULL;
    result = (node = q->head)->aline;
    if (!(q->head = q->head->next)) q->tail = NULL;
    FREE(node);
    return result;
}

void free_queue(q)
    Queue *q;
{
    Textnode *node;

    while (node = q->head) {
        q->head = q->head->next;
        free_aline(node->aline);
        FREE(node);
    }
    q->tail = NULL;
}

void listlog(world)
    World *world;
{
    if (world->history->logfile)
        oprintf("%% Logging world %s output to %s",
          world->name, world->history->logfile->name);
}

void do_log(args)
    char *args;
{
    char c;
    World *world;
    History *history = global;
    TFILE *logfile;

    startopt(args, "ligw:");
    while (c = nextopt(&args, NULL))
        switch (c) {
        case 'l':
            history = local;
            break;
        case 'i':
            history = input;
            break;
        case 'g':
            history = global;
            break;
        case 'w':
            if (!*args) world = xworld();
            else world = find_world(args);
            if (!world) {
                oprintf("%% No world %s", args);
                history = NULL;
            } else history = world->history;
            break;
        }
    if (!history) return;
    if (!*args) {
        if (log_on) {
            if (input->logfile)
                oprintf("%% Logging input to %s", input->logfile->name);
            if (local->logfile)
                oprintf("%% Logging local output to %s", local->logfile->name);
            if (global->logfile)
                oprintf("%% Logging global output to %s", global->logfile->name);
            mapsock(listlog);
        } else {
            oputs("% Logging disabled.");
        }
        return;
    } else if (cstrcmp(args, "OFF") == 0) {
        if (history->logfile) {
            tfclose(history->logfile);
            history->logfile = NULL;
            if (!--log_on) put_logging(FALSE);
        }
        return;
    } else if (cstrcmp(args, "ON") == 0) {
        logfile = tfopen(NULL, "LOGFILE", "a");
    } else {
        logfile = tfopen(args, NULL, "a");
    }
    if (!logfile) return;
    if (history->logfile) {
        tfclose(history->logfile);
        log_on--;
    }
    oprintf("%% Logging to file %s", logfile->name);
    history->logfile = logfile;
    if (!log_on++) put_logging(TRUE);
}

