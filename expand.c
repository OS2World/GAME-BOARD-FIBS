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

/********************************************************************
 * Fugue macro text expander                                        *
 *                                                                  *
 * Expands a macro body, doing argument and reentrance substitution *
 * Originally written by Greg Hudson, with mods by Ken Keys.        *
 ********************************************************************/

#include <stdio.h>
#include <ctype.h>
#include "tf.h"
#include "dstring.h"
#include "util.h"
#include "macro.h"
#include "socket.h"
#include "command1.h"
#include "output.h"

#define MAX_ARGS 100
#ifndef BSD
#define RANDOM rand
#else
#define RANDOM random
#endif

extern int mecho;
extern Stringp mprefix, lastname;

static int recur_count = 0;
#ifndef MAX_RECUR
#define MAX_RECUR 100
#endif

typedef struct Arg {
    Stringp text;
    int spaces;
} Arg;

enum cmd_types {
    SEND,
    EXECUTE,
    SUBSTITUTE,
    SUBINCLUDE
};

typedef struct Line {
    Stringp text;
    struct Line *next;
} Line;

typedef struct Cmd {
    Stringp text;
    enum cmd_types type;
    struct Cmd *next;
} Cmd;

static Cmd  *FDECL(new_cmd,(enum cmd_types type));
static Line *NDECL(new_line);
static Cmd  *FDECL(reentrance_sub,(char *body));
static Line *FDECL(newline_sub,(char *body));
static void  FDECL(argsub,(char *src, Stringp dest, int argc, Arg **argv));
static void  FDECL(include_subs,(Cmd *c, Stringp buffer, int argc, Arg **argv));

void FDECL(process_macro,(char *body, char *args, Stringp dest, int toplevel));

static Cmd *new_cmd(type)
    enum cmd_types type;
{
    Cmd *c;

    c = (Cmd *) MALLOC(sizeof(Cmd));
    Stringinit(c->text);
    c->next = NULL;
    c->type = type;
    return c;
}

static Line *new_line()
{
    Line *l;

    l = (Line *) MALLOC(sizeof(Line));
    Stringinit(l->text);
    l->next = NULL;
    return l;
}

static Cmd *reentrance_sub(body)
    char *body;
{
    char *start, *in = body;
    Cmd *header, *c;

    if (!*in) return NULL;
    header = new_cmd(SEND);
    c = header;
    while (*in) {
        if (*in == '/') {
            if (*++in == '/') while (*in == '/') Stringadd(c->text, *in++);
            else {
                if (c->type == EXECUTE || c->type == SUBINCLUDE)
                    c->next = new_cmd(SEND);
                else c->next = new_cmd(EXECUTE);
                c = c->next;
            }
        } else if (*in == '$') {
            if (*++in == '$') while (*in == '$') Stringadd(c->text, *in++);
            else {
                if (c->type == SUBSTITUTE) c->next = new_cmd(SEND);
                else if (c->type == SUBINCLUDE) c->next = new_cmd(EXECUTE);
                else if (c->type == EXECUTE) c->next = new_cmd(SUBINCLUDE);
                else c->next = new_cmd(SUBSTITUTE);
                c = c->next;
            }
        } else {
            for (start = in++; *in && *in != '/' && *in != '$'; in++);
            Stringncat(c->text, start, in - start);
        }
    }
    return header;
}

static Line *newline_sub(body)
    char *body;
{
    Line *header, *l;
    char *start, *in = body, ch;

    if (*in == '\0') return NULL;
    header = new_line();
    l = header;
    while (*in) {
        if (*in == '%') {
            ch = *(in + 1);
            if (ch == '%') while (*in == '%') Stringadd(l->text, *in++);
            else if (ch == '\\' || ch == ';') {
                in += 2;
                l->next = new_line();
                l = l->next;
            } else Stringadd(l->text, *in++);
        } else if (*in == '\\') {
            if (isdigit(*(in + 1))) {
                ch = atoi(in + 1);
                Stringadd(l->text, ch);
                while (isdigit(*++in));
            } else Stringadd(l->text, *in++);
        } else {
            for (start = in++; *in && *in != '%' && *in != '\\'; in++);
            Stringncat(l->text, start, in - start);
        }
    }
    return header;
}

static void argsub(src, dest, argc, argv)
    char *src;
    Stringp dest;
    int argc;
    Arg **argv;
{
    char *in, *start;
    int i, n;
    int bracket, empty;

    Stringterm(dest, 0);
    in = src;
    while (*in) {
        if (*in == '%') {
            if (*++in == '%') {
                while (*in == '%') Stringadd(dest, *in++);
                continue;
            }
            bracket = empty = FALSE;
            n = -1;
            if (*in == '{') {
                ++in;
                bracket = TRUE;
            }
            if (*in == '-') {
                ++in;
                if (isdigit(*in)) {
                    n = atoi(in);
                    for (++in; isdigit(*in); ++in);
                    if (n == 0) {
                        if (argc) {
                            for (i = 0; i < argc; i++) {
                                SStringcat(dest, argv[i]->text);
                                Stringnadd(dest, ' ', argv[i]->spaces);
                            }
                        } else empty = TRUE;
                    } else {
                        if (n < argc) {
                            for (i = n; i < argc; i++) {
                                SStringcat(dest, argv[i]->text);
                                Stringnadd(dest, ' ', argv[i]->spaces);
                            }
                        } else empty = TRUE;
                    }
                } else if (ucase(*in) == 'L') {
                    ++in;
                    if (isdigit(*in)) {
                        n = argc - atoi(in);
                        while (isdigit(*++in));
                    } else n = argc - 1;
                    if (n > 0) {
                        for (i = 0; i < n; i++) {
                            SStringcat(dest, argv[i]->text);
                            Stringnadd(dest, ' ', argv[i]->spaces);
                        }
                    } else empty = TRUE;
                } else {
                    ++in;
                    Stringadd(dest, '%');
                    continue;
                }
            } else {
                if (isdigit(*in)) {
                    n = atoi(in);
                    for (++in; isdigit(*in); ++in);
                    if (n == 0) {
                        if (argc) {
                            for (i = 0; i < argc; i++) {
                                SStringcat(dest, argv[i]->text);
                                Stringnadd(dest, ' ', argv[i]->spaces);
                            }
                        } else empty = TRUE;
                    } else {
                        if (--n < argc) SStringcat(dest, argv[n]->text);
                        else empty = TRUE;
                    }
#if 0
                } else if (ucase(*in) == 'S') {
                    ++in;
                    if (isdigit(*in)) {
                        n = atoi(in) - 1;
                        while (isdigit(*++in));
                    } else n = 0;
                    for (subexpr = sublist; n && subexpr; n--, subexpr = subexpr->next);
                    if (subexpr) Stringcat(dest, subexpr->str);
                    else empty = TRUE;
#endif
                } else if (ucase(*in) == 'L') {
                    ++in;
                    if (isdigit(*in)) {
                        n = argc - atoi(in);
                        while (isdigit(*++in));
                    } else n = argc - 1;
                    if (n >= 0) SStringcat(dest, argv[n]->text);
                    else empty = TRUE;
                } else if (lcase(*in) == '*') {
                    ++in;
                    if (argc) {
                        for (i = 0; i < argc; i++) {
                            SStringcat(dest, argv[i]->text);
                            Stringnadd(dest, ' ', argv[i]->spaces);
                        }
                    } else empty = TRUE;
                } else if (ucase(*in) == 'R') {
                    ++in;
                    if (argc) {
                        n = RANDOM() % argc;
                        SStringcat(dest, argv[n]->text);
                    } empty = TRUE;
                } else if (ucase(*in) == 'E') {
                    ++in;
                    Stringadd(dest, '\033');
                } else if (lcase(*in) == 'n') {
                    ++in;
                    SStringcat(dest, lastname);
                } else {
                    Stringadd(dest, '%');
                    if (bracket) Stringadd(dest, '{');
                    continue;
                }
            }
            if (bracket) {
                if (*in == '-') {
                    if (empty) {
                        for (start = ++in; *in && *in != '}'; in++);
                        Stringncat(dest, start, in - start);
                    } else while (*in && *in != '}') in++;
                }
                if (*in != '}') {
                    /* syntax error: missing '}' */
                } else ++in;
            } else {
                if (*in == '-') {
                    oputs("% WARNING: \"%arg-default\" won't be supported in future; use \"%{arg-default}\"");
                    if (empty) {
                        ++in;
                    } else while (*in && !isspace(*in)) in++;
                }
            }
        } else {
            for (start = in++; *in && *in != '%'; in++);
            Stringncat(dest, start, in - start);
        }
    }
}

/* Execute sequence is always terminated by a SEND, so simply process as
 * many SUBINCLUDEs and concatenate as many EXECUTEs as found onto buffer,
 * which already includes the text of the first command.
 */
static void include_subs(c, buffer, argc, argv)
    Cmd *c;
    Stringp buffer;
    int argc;
    Arg **argv;
{
    Stringp temp1, temp2;
    Cmd *p, *pn;

    Stringinit(temp1);
    Stringinit(temp2);
    p = c->next;
    while (p != NULL && p->type != SEND) {
        if (p->type == SUBINCLUDE) {
            argsub(p->text->s, temp1, argc, argv);
            get_macro_body(temp1->s, temp2);
            if (mecho && temp1->len)
                oprintf("%S$%S --> %S", mprefix, temp1, temp2);
            SStringcat(buffer, temp2);
        } else if (p->type == EXECUTE) {
            argsub(p->text->s, temp1, argc, argv);
            SStringcat(buffer, temp1);
        } else oputs("% Unexpected type in include_subs()");
        pn = p->next;
        Stringfree(p->text);
        FREE(p);
        p = pn;
    }
    Stringfree(temp1);
    Stringfree(temp2);
    c->next = p;
}

void process_macro(body, args, dest, toplevel)
    Stringp dest;
    char *body, *args;
    int toplevel;
{
    Stringp buffer, sublevel;
    Line *l, *ln;
    Cmd *c, *cn;
    char *in;
    int argc = 0, vecsize = 20;
    Arg **argv;

    if (++recur_count > MAX_RECUR) {
        oputs("% Too many recursions.");
        recur_count--;
        return;
    }
    argv = (Arg **) MALLOC(vecsize * sizeof(Arg *));
    for (in = args; isspace(*in); in++);
    while (*in) {
        if (argc == vecsize)
            argv = (Arg**)REALLOC((char*)argv, sizeof(Arg*) * (vecsize += 10));
        argv[argc] = (Arg *) MALLOC(sizeof(Arg));
        Stringinit(argv[argc]->text);
        in = getword(argv[argc]->text, in);
        for (argv[argc]->spaces = 0; isspace(*in); in++) argv[argc]->spaces++;
        argc++;
    }

    Stringinit(buffer);
    Stringinit(sublevel);
    l = newline_sub(body);
    while (l != NULL) {
        c = reentrance_sub(l->text->s);
        Stringfree(l->text);
        while (c != NULL) {
            if (c->text->len) {
                argsub(c->text->s, buffer, argc, argv);
                if (c->type == SEND) SStringcat(dest, buffer);
                else {
                    Stringterm(sublevel, 0);
                    if (c->type == EXECUTE) {
                        include_subs(c, buffer, argc, argv);
                        if (mecho && buffer->len)
                            oprintf("%S/%S", mprefix, buffer);
                        handle_command(buffer->s, sublevel);
                    }
                    if (c->type == SUBSTITUTE) {
                        get_macro_body(buffer->s, sublevel);
                        if (mecho && buffer->len)
                            oprintf("%S$%S --> %S", mprefix, buffer,
                             sublevel);
                    }
                    SStringcat(dest, sublevel);
                }
            }
            Stringfree(c->text);
            cn = c->next;
            FREE(c);
            c = cn;
        }
        ln = l->next;
        FREE(l);
        l = ln;
        if (l != NULL || toplevel) {
            newline_package(dest, 1);
            if (dest->len > 1) {
                newline_package(dest, 0);
                if (mecho) oprintf("%S%S", mprefix, dest);
                newline_package(dest, 1);
                transmit(dest->s, dest->len);
            }
            Stringterm(dest, 0);
        }
    }
    while (--argc >= 0) {
        Stringfree(argv[argc]->text);
        FREE(argv[argc]);
    }
    FREE(argv);
    Stringfree(sublevel);
    Stringfree(buffer);
    recur_count--;
}
