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

/*************************************************
 * Fugue processes.                              *
 *                                               *
 * Rewritten by Ken Keys, orginally written by   *
 * Leo Plotkin and modified by Greg Hudson.      *
 * Handles /repeat and /quote processes.         *
 *************************************************/

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include "tf.h"
#include "dstring.h"
#include "util.h"
#include "history.h"
#include "world.h"
#include "socket.h"
#include "macro.h"
#include "expand.h"
#include "output.h"

#define P_REPEAT     001
#define P_QFILE      002
#define P_QCOMMAND   004
#define P_QRECALL    010
#define P_QLOCAL     020

typedef union ProcIn {
    FILE *fp;       /* I/O for shell command or file */
    struct Queue *queue;   /* local command output */
} ProcIn;
    
typedef struct Proc {
    int pid;
    int type;
    int count;
    int PDECL((*funct),(struct Proc *proc));
    int ptime;
    TIME_T timer;       /* time of next execution */
    char *pre;          /* what to prefix */
    char *suf;          /* what to suffix */
    ProcIn in;          /* source of quote input */
    struct World *world;/* where to send output */
    char *cmd;          /* command or file name */
    struct Proc *next, *prev;
} Proc;

static void FDECL(newproc,(int type, int FDECL((*funct),(Proc *proc)),
                           int count, char *pre, char *suf, ProcIn in,
                           struct World *world, char *cmd, int ptime));
static void FDECL(removeproc,(Proc *proc));
static void FDECL(freeproc,(Proc *proc));
static int  FDECL(runproc,(Proc *proc));
static int  FDECL(do_repeat,(Proc *proc));
static int  FDECL(do_quote,(Proc *proc));

void   NDECL(do_ps);
void   FDECL(do_kill,(int pid));
void   FDECL(kill_procs_by_world,(struct World *world));
void   NDECL(kill_procs);
void   FDECL(runall,(TIME_T now));
void   FDECL(start_quote,(char *args));
void   FDECL(start_repeat,(char *args));

TIME_T process_time = 1;          /* default process delay */
TIME_T proctime = 0;              /* when next process should be run */

extern int lpquote;

static Proc *proclist = NULL;     /* procedures to execute */

void do_ps()
{
    Proc *p;
    char c;
    char buf[11];

    oprintf("  PID TYPE    WORLD      PTIME COUNT COMMAND");
    for (p = proclist; p; p = p->next) {
        if (p->world) sprintf(buf, "-w%-8s", p->world->name);
        else sprintf(buf, "%10s", "");
        if (p->type == P_REPEAT) {
            oprintf("%5d /repeat %s -%-4d %5d %s", p->pid, buf,
            (p->ptime == -1) ? process_time : p->ptime, p->count, p->cmd);
        } else {
            if (p->type == P_QFILE) c = '\'';
            else if (p->type == P_QCOMMAND) c = '!';
            else if (p->type == P_QRECALL) c = '#';
            else /* (p->type == P_QLOCAL) */ c = '`';
            oprintf("%5d /quote  %s -%-4d       %s%c\"%s\"%s", p->pid,
                buf, (p->ptime == -1) ? process_time : p->ptime, p->pre, c,
                p->cmd, p->suf);
        }
    }
}

static void newproc(type, funct, count, pre, suf, in, world, cmd, ptime)
    int type, count, ptime;
    int FDECL((*funct),(Proc *proc));
    char *pre, *suf, *cmd;
    ProcIn in;
    struct World *world;
{
    Proc *proc;
    static int pid = 0;

    proc = (Proc *) MALLOC(sizeof(Proc));

    proc->count = count;
    proc->funct = funct;
    proc->ptime = ptime;
    proc->type = type;
    proc->timer = (TIME_T)time(NULL) + ((ptime == -1) ? process_time : ptime);
    proc->pre = STRDUP(pre);
    proc->suf = STRDUP(suf);
    proc->cmd = STRDUP(cmd);
    proc->pid = ++pid;
    proc->in = in;
    proc->world = world;

    if (proclist) proclist->prev = proc;
    proc->next = proclist;
    proc->prev = NULL;
    proclist = proc;
    if (proctime == 0 || proc->timer < proctime) proctime = proc->timer;
    do_hook(H_PROCESS, "%% Starting process %d", "%d", proc->pid);
}

static void freeproc(proc)
    Proc *proc;
{
    do_hook(H_KILL, "%% Killing process %d", "%d", proc->pid);
    if (proc->type == P_QCOMMAND) {
        readers_clear(fileno(proc->in.fp));
        pclose(proc->in.fp);
    } else if (proc->type == P_QFILE) {
        fclose(proc->in.fp);
    } else if (proc->type == P_QLOCAL || proc->type == P_QRECALL) {
        free_queue(proc->in.queue);
        FREE(proc->in.queue);
    }
    FREE(proc->pre);
    FREE(proc->suf);
    FREE(proc->cmd);
    FREE(proc);
}

static void removeproc(proc)
    Proc *proc;
{
    if (proc->next) proc->next->prev = proc->prev;
    if (proc->prev) proc->prev->next = proc->next;
    else proclist = proc->next;
}

void kill_procs()
{
    Proc *next;

    for (; proclist; proclist = next) {
        next = proclist->next;
        freeproc(proclist);
    }
    proctime = 0;
}

void kill_procs_by_world(world)
    struct World *world;
{
    Proc *proc, *next;

    if (!proclist) return;
    for (proc = proclist; proc; proc = next) {
        next = proc->next;
        if (proc->world == world) {
            removeproc(proc);
            freeproc(proc);
        }
    }
}

void do_kill(pid)
    int pid;
{
    Proc *proc;

    for (proc = proclist; proc && (proc->pid != pid); proc = proc->next);
    if (proc) {
        removeproc(proc);
        freeproc(proc);
    } else oputs("% no such process");
}

void runall(now)
    TIME_T now;
{
    Proc *t, *n;
    TIME_T earliest = 0;

    proctime = 0;
    for (t = proclist; t; t = n) {
        n = t->next;
        if (t->type == P_QCOMMAND && is_active(fileno(t->in.fp))) {
            readers_clear(fileno(t->in.fp));
            if (!runproc(t)) t = NULL;
        } else if ((t->type != P_QCOMMAND) && (lpquote || (t->timer <= now))) {
            if (!runproc(t)) t = NULL;
        }

        if (t && (t->type == P_QCOMMAND) && (t->timer <= now))
            readers_set(fileno(t->in.fp));
        if (t && (!earliest || (t->timer < earliest)))
            earliest = t->timer;
    }
    /* calculate next proc (proctime may have been set by a nested process) */
    proctime = (proctime && proctime < earliest) ? proctime : earliest;
}

static int runproc(p)
    Proc *p;
{
    int notdone;
    extern struct Sock *fsock, *xsock;

    if (p->world) xsock = p->world->socket;
    notdone = (*p->funct)(p);
    xsock = fsock;
    if (notdone) {
        return p->timer =
            (TIME_T)time(NULL) + ((p->ptime == -1) ? process_time : p->ptime);
    }
    removeproc(p);
    freeproc(p);
    return 0;
}

static int do_repeat(proc)
    Proc *proc;
{
    Stringp dest;

    if (proc->count--) {
        Stringinit(dest);
        process_macro(proc->cmd, "", dest, TRUE);
        Stringfree(dest);
    }
    return proc->count;
}

static int do_quote(proc)
    Proc *proc;
{
    static Stringp line, buffer;
    static int strings_inited = FALSE;
    Aline *aline;
    extern Stringp qprefix;
    extern int qecho;

    if (!strings_inited) {
        Stringinit(line);
        Stringinit(buffer);
        strings_inited = TRUE;
    }
    if (proc->type == P_QLOCAL || proc->type == P_QRECALL) {
        if ((aline = dequeue(proc->in.queue)) == NULL) return FALSE;
        Sprintf(buffer, "%s%s%s", proc->pre, aline->str, proc->suf);
    } else {
        if (fgetS(line, proc->in.fp) == NULL) return FALSE;
        newline_package(line, 0);
        Sprintf(buffer, "%s%S%s", proc->pre, line, proc->suf);
    }

    if (qecho) oprintf("%S%S", qprefix, buffer);
    check_command(FALSE, buffer);
    return TRUE;
}

static void ecpy(dest, src)
    Stringp dest;
    char *src;
{
    char *start;

    Stringterm(dest, 0);
    while (*src) {
        if (*src == '\\' && *(src + 1)) src++;
        for (start = src++; *src && *src != '\\'; src++);
        Stringncat(dest, start, src - start);
    }
}

static int procopt(argp, ptime, world)
    char **argp;
    int *ptime;
    struct World **world;
{
    char opt, *ptr;

    *world = NULL;
    *ptime = -1;
    startopt(*argp, "0w:");
    while (opt = nextopt(&ptr, ptime)) {
        switch(opt) {
        case 'w':
            if (!*ptr) *world = xworld();
            else if ((*world = find_world(ptr)) == NULL) {
                oprintf("%% No world %s", ptr);
                return FALSE;
            }
            break;
        case '0': break;
        default:  return FALSE;
        }
    }
    *argp = ptr;
    return TRUE;
}

void start_quote(args)
    char *args;
{
    char *p, *command, *suffix;
    int type;
    Stringp expanded;
    static Stringp pre, suf, cmd;
    static int strings_initted = FALSE;
    ProcIn in;
    extern struct Queue *output_dest;
    struct Queue *oldqueue;
    int ptime;
    struct World *world;

    if (!strings_initted) {
        Stringinit(pre);
        Stringinit(suf);
        Stringinit(cmd);
        strings_initted = TRUE;
    }
    if (!procopt(&args, &ptime, &world)) return;

    p = args;
    while (*p && *p != '\'' && *p != '!' && *p != '#' && *p != '`') {
        if (*p == '\\' && *(p + 1)) p += 2;
        else p++;
    }
    if (*p == '\'') type = P_QFILE;
    else if (*p == '!') type = P_QCOMMAND;
    else if (*p == '#') type = P_QRECALL;
    else if (*p == '`') type = P_QLOCAL;
    else {
        oputs("% Bad /quote syntax");
        return;
    }
    *p = '\0';
    if (*++p == '"') {
        command = ++p;
        if ((p = estrchr(p, '"', '\\')) == NULL) suffix = "";
        else {
            *p = '\0';
            suffix = p + 1;
        }
    } else {
        command = p;
        suffix = "";
    }
    ecpy(pre, args);
    ecpy(suf, suffix);
    ecpy(cmd, command);
    switch (type) {
    case P_QFILE:
        Stringexpand(cmd);
        if ((in.fp = fopen(cmd->s, "r")) == NULL) {
            operror(cmd->s);
            return;
        }
        break;
    case P_QCOMMAND:
        Stringcat(cmd, " 2>&1");                     /* capture stderr, too */
        if ((in.fp = popen(cmd->s, "r")) == NULL) {
            operror(cmd->s);
            return;
        }
        Stringterm(cmd, cmd->len - 5);               /* remove " 2>&1" */
        break;
    case P_QRECALL:
        init_queue(in.queue = (struct Queue *)MALLOC(sizeof(struct Queue)));
        if (!recall_history(cmd->s, in.queue)) {
            FREE(in.queue);
            return;
        }
        break;
    case P_QLOCAL:
        init_queue(in.queue = (struct Queue *)MALLOC(sizeof(struct Queue)));
        oldqueue = output_dest;
        output_dest = in.queue;
        Stringinit(expanded);
        process_macro(cmd->s, "", expanded, TRUE);
        Stringfree(expanded);
        output_dest = oldqueue;
        break;
    default:    /* impossible */
        break;
    }
    newproc(type, do_quote, -1, pre->s, suf->s, in, world, cmd->s, ptime);
    if (lpquote) runall(time(NULL));
}

void start_repeat(args)
    char *args;
{
    int ptime, count;
    struct World *world;
    ProcIn in;

    if (!procopt(&args, &ptime, &world)) return;
    count = numarg(&args);
    if (!args || !*args || (count <= 0)) {
        oputs("% Bad /repeat syntax.");
        return;
    }
    newproc(P_REPEAT, do_repeat, count, "", "", in, world, args, ptime);
    if (lpquote) runall(time(NULL));
}
