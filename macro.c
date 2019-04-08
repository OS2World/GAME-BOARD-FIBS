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

/**********************************************
 * Fugue macro package                        *
 *                                            *
 * Macros, hooks, triggers, hilites and gags  *
 * are all processed here.                    *
 **********************************************/

#include <stdio.h>
#include <ctype.h>
#include "tf.h"
#include "dstring.h"
#include "util.h"
#include "history.h"
#include "world.h"
#include "socket.h"
#include "macro.h"
#include "keyboard.h"
#include "output.h"
#include "expand.h"
#include "command1.h"

#ifndef BSD
# define RANDOM rand
#else
# define RANDOM random
#endif

static Macro  *FDECL(find_macro,(char *name));
static int     FDECL(macro_match,(Macro *spec, Macro *macro,
               int FDECL((*cmp),(char *s1, char *s2))));
static Macro  *FDECL(match_exact,(int hook, char *str, int flags));
static Macro  *FDECL(find_match,(int hook, char *text));
static void    FDECL(insert_by_priority,(Macro *macro, int hook));
static void    FDECL(list_defs,(TFILE *file, Macro *spec, int abbrev));
static String *FDECL(escape,(char *src));
static int     FDECL(parse_hook,(char *args));
static int     FDECL(find_hook,(char *name));
static int     FDECL(hash_string,(char *str, int size));
static void    FDECL(hash_insert,(Macro *macro));
static void    FDECL(hash_remove,(Macro *macro));
static int     FDECL(run_trig_or_hook,(Macro *macro, char *text));

void   NDECL(init_macros);
Macro *FDECL(new_macro,(char *name, char *trig, char *bind, int hook,
       char *hargs, char *body, World *world, int pri, int prob,
       int attr, int shots, int invis));
void   FDECL(add_macro,(Macro *macro));
void   FDECL(add_hook,(char *proto, char *body));
int    FDECL(install_bind,(Macro *spec));
void   FDECL(do_add,(Macro *spec));
void   FDECL(do_edit,(Macro *spec));
void   FDECL(nuke_macro,(Macro *macro));
void   FDECL(remove_macro,(char *args, int attr, int byhook));
void   FDECL(purge_macro,(Macro *spec));
void   FDECL(remove_by_number,(char *args));
void   FDECL(save_macros,(char *args));
void   FDECL(list_macros,(char *args));
int    FDECL(world_subs,(char *src, Stringp dest));
void   FDECL(do_macro,(char *name, char *args, Stringp dest, int toplevel));
short  VDECL(do_hook,(int hook, char *fmt, char *argfmt, ...));
void   FDECL(get_macro_body,(char *name, Stringp dest));
short  FDECL(check_trigger,(char *text));

extern int redef;

#define HASH_SIZE 197

static Macro *head = NULL;             /* head of complete macro list */
static Macro *tail = NULL;             /* tail of complete macro list */
static Macro *thead = NULL;            /* head of trigger macro list */
static Macro *hhead = NULL;            /* head of hook macro list */
static Macro *hash_table[HASH_SIZE];   /* macros hashed by name */
static World NoWorld, AnyWorld;        /* explicit "no" and "don't care" */
static int mnum = 1;                   /* macro ID number */

/* It is IMPORTANT that these be in the same order as enum Hooks */
static char *hook_table[] = {
  "ACTIVITY",   
  "BACKGROUND",
  "BAMF",
  "CONFAIL",
  "CONNECT",
  "DISCONNECT",
  "HISTORY",
  "KILL",
  "LOAD",
  "LOADFAIL",
  "LOGIN",
  "MAIL",
  "MORE",
  "PENDING",
  "PROCESS",
  "REDEF",
  "RESIZE",
  "RESUME",
  "SEND",
  "SHELL",
  "WORLD"
};

#define NUM_HOOKS (sizeof(hook_table) / sizeof(char*))

#define macmatch(spec, macro) macro_match((spec), (macro), smatch)
#define maccmp(spec, macro)   macro_match((spec), (macro), cstrcmp)

#define Unlink(Mac, Head, Next, Prev)      /* delete Mac from linked list */ \
    do {\
        if (Mac->Next) Mac->Next->Prev = Mac->Prev;\
        if (Mac->Prev) Mac->Prev->Next = Mac->Next;\
        else Head = Mac->Next;\
    } while (0)

#define Insert(Mac, Head, Next, Prev)      /* add Mac to beginning of list */ \
    do {\
        if (Head) Head->Prev = Mac;\
        Mac->Next = Head;\
        Mac->Prev = NULL;\
        Head = Mac;\
    } while (0)

/* These macros allow easy sharing of trigger and hook code. The address and
 * dereference is necessary to allow the expression to be used portably as
 * the Left Hand Side of '='.
 */
#define Head         (*(hook ? &hhead      : &thead))
#define Next(mac)    (*(hook ? &mac->hnext : &mac->tnext))
#define Prev(mac)    (*(hook ? &mac->hprev : &mac->tprev))
#define Pattern(mac) (*(hook ? &mac->hargs : &mac->trig))
#define Flag(mac)    ((hook ? mac->hook  : (int)mac->attr))  /* can't be lhs */


void init_macros()
{
    int i;

    for (i = 0; i < HASH_SIZE; i++) hash_table[i] = NULL;
}

/***************************************
 * Routines for parsing macro commands *
 ***************************************/

short parse_attrs(args)            /* convert attribute string to bitfields */
    char *args;
{
    short attrs = 0;

    while (*args) {
        switch(*args++) {
        case 'n':  break;
        case 'G':  attrs |= F_NORECORD;  break;
        case 'g':  attrs |= F_GAG;       break;
        case 'u':  attrs |= F_UNDERLINE; break;
        case 'r':  attrs |= F_REVERSE;   break;
        case 'f':  attrs |= F_FLASH;     break;
        case 'd':  attrs |= F_DIM;       break;
        case 'h':  attrs |= F_HILITE;    break;
        case 'b':  attrs |= F_BELL;      break;
        default:
            oputs("% Invalid attribute in 'a' option");
            return -1;
        }
    }
    return attrs;
}

static int parse_hook(args)            /* convert hook string to bitfields */
    char *args;
{
    char *in;
    int hook, result = 0, done;

    if (strcmp(args, "*") == 0) return AnyHook;
    for (done = FALSE; !done; args = in) {
        if ((in = strchr(args, '|')) == NULL) done = TRUE;
        else *in++ = '\0';
        if ((hook = find_hook(args)) == 0) return 0;
        result |= hook;
    }
    return result;
}

Macro *macro_spec(args)           /* convert user macro string to Macro */
    char *args;
{
    Macro *spec;
    char opt, *ptr, *trig = NULL, *bind = NULL, *hargs = NULL;
    int num, error = FALSE;

    spec = (Macro *)MALLOC(sizeof(struct Macro));
    spec->next = spec->tnext = spec->hnext = NULL;
    spec->prev = spec->tprev = spec->hprev = NULL;
    spec->name = spec->trig = spec->bind = spec->hargs = spec->body = NULL;
    spec->hook = 0;
    spec->func = NULL;
    spec->world = NULL;
    spec->pri = spec->prob = spec->shots = -1;
    spec->invis = 0;
    spec->attr = 0;
    spec->temp = TRUE;

    startopt(args, "p#c#b:t:w:h:a:f:iIn#1:");
    while (opt = nextopt(&ptr, &num)) {
        switch (opt) {
        case 'p':
            spec->pri = num;
            break;
        case 'c':
            spec->prob = num;
            break;
        case 'i':
            spec->invis = 1;
            break;
        case 'I':
            spec->invis = 2;
            break;
        case 'b':
            if (bind) FREE(bind);
            else bind = STRDUP(translate_keystring(ptr));
            break;
        case 't':
            if (trig) FREE(trig);
            if (!smatch_check(trig = STRDUP(ptr))) error = TRUE;
            break;
        case 'w':
            if (!*ptr || strcmp(ptr, "+") == 0) spec->world = &AnyWorld;
            else if (strcmp(ptr, "-") == 0) spec->world = &NoWorld;
            else if ((spec->world = find_world(ptr)) == NULL) {
                oprintf("%% No world %s", ptr);
                error = TRUE;
            }
            break;
        case 'h':
            if (hargs) FREE(hargs);
            if (!*ptr || strcmp(ptr, "+") == 0) spec->hook = AnyHook;
            else if (strcmp(ptr, "-") == 0) spec->hook = 0;
            else {
                if ((hargs = strchr(ptr, ' ')) != NULL) *hargs++ = '\0';
                if ((spec->hook = parse_hook(ptr)) == 0) error = TRUE;
                else hargs = STRDUP(hargs ? stripstr(hargs) : "");
            }
            break;
        case 'a': case 'f':
            if ((spec->attr |= parse_attrs(ptr)) < 0) error = TRUE;
            break;
        case 'n':
            spec->shots = num;
            break;
        case '1':
            if (*ptr && *(ptr + 1)) error = TRUE;
            else if (!*ptr || *ptr == '+') spec->shots = 1;
            else if (*ptr == '-') spec->shots = 0;
            else error = TRUE;
            if (error) oputs("% Invalid argument to 1 option");
            break;
        default:
            error = TRUE;
        }

        if (error) {
            nuke_macro(spec);
            return NULL;
        }
    }

    if (trig) spec->trig = trig;
    if (bind) spec->bind = bind;
    spec->hargs = (hargs) ? hargs : STRDUP("");
    if (!*ptr) return spec;
    spec->name = ptr;
    if ((ptr = strchr(ptr, '=')) != NULL) {
        *ptr = '\0';
        spec->body = ptr + 1;
        stripstr(spec->body);
        if (*spec->body) {
            spec->body = STRDUP(spec->body);
            if (cstrncmp(spec->body, "/DOKEY ", 7) == 0)
                spec->func = find_efunc(stripstr(spec->body + 7));
        } else spec->body = NULL;
    }
    stripstr(spec->name);
    spec->name = *spec->name ? STRDUP(spec->name) : NULL;
    return spec;
}


/********************************
 * Routines for locating macros *
 ********************************/

static int macro_match(spec, macro, cmp)     /* compare spec to macro */
    Macro *spec, *macro;
    int FDECL((*cmp),(char *s1, char *s2));  /* string comparison function */
{
    if (!spec->invis && macro->invis) return 1;
    if (spec->invis == 2 && !macro->invis) return 1;
    if (spec->shots != -1 && spec->shots != macro->shots) return 1;
    if (spec->prob != -1 && spec->prob != macro->prob) return 1;
    if (spec->pri != -1 && spec->pri != macro->pri) return 1;
    if (spec->world && spec->world != macro->world &&
        (spec->world != &AnyWorld || macro->world == &NoWorld)) return 1;
    if (spec->hook) {
        if ((spec->hook & macro->hook) == 0) return 1;
        if (*spec->hargs && (*cmp)(spec->hargs, macro->hargs) != 0) return 1;
    }
    if (spec->attr && (spec->attr & macro->attr) == 0) return 1;
    if (spec->trig) {
        if (strcmp(spec->trig, "+") == 0 || strcmp(spec->trig, "") == 0) {
            if (!*macro->trig) return 1;
        } else if (strcmp(spec->trig, "-") == 0) {
            if (*macro->trig) return 1;
        } else if ((*cmp)(spec->trig, macro->trig) != 0) return 1;
    }
    if (spec->bind) {
        if (strcmp(spec->bind, "+") == 0 || strcmp(spec->bind, "") == 0) {
            if (!*macro->bind) return 1;
        } else if (strcmp(spec->bind, "-") == 0) {
            if (*macro->bind) return 1;
        } else if ((*cmp)(spec->bind, macro->bind) != 0) return 1;
    }
    if (spec->name && (*cmp)(spec->name, macro->name) != 0) return 1;
    if (spec->body && (*cmp)(spec->body, macro->body) != 0) return 1;
    return 0;
}

static Macro *find_macro(name)              /* find Macro by name */
    char *name;
{
    Macro *p;

    if (!*name) return NULL;
    for (p = hash_table[hash_string(name, HASH_SIZE)]; p; p = p->bnext) {
        if (cstrcmp(name, p->name) == 0) return p;
    }
    return NULL;
}

static int find_hook(name)                  /* convert hook name to int */
    char *name;
{
    int bottom, top, mid, value;

    bottom = 0;
    top = NUM_HOOKS - 1;
    while (bottom <= top) {
        mid = (top + bottom) / 2;
        value = cstrcmp(name, hook_table[mid]);
        if (value == 0) return (1 << mid);
        else if (value < 0) top = mid - 1;
        else bottom = mid + 1;
    }
    oprintf("%% No hook for \"%s\"", name);
    return 0;
}

static Macro *match_exact(hook, str, flags)   /* find single exact match */
    int hook, flags;
    char *str;
{
    Macro *macro;
  
    if ((hook && !flags) || (!hook && !*str)) return NULL;
    for (macro = Head; macro; macro = Next(macro))
        if ((Flag(macro) & flags) && !cstrcmp(Pattern(macro), str)) break;
    return macro;
}

static Macro *find_match(hook, text)       /* find a matching hook or trig */
    char *text;
    int hook;
{
    Macro *first = NULL, *macro;
    int num = 0, pri = -1;

    for (macro = Head; macro && macro->pri >= pri; macro = Next(macro)) {
        if (hook && !(macro->hook & hook)) continue;
        if (macro->world && macro->world != xworld()) continue;
        if ((hook && !*macro->hargs) || equalstr(Pattern(macro), text)) {
            if (macro->pri > pri) {
                macro->pnext = NULL;
                num = 1;
                pri = macro->pri;
            } else {
                macro->pnext = first;
                num++;
            }
            first = macro;
        }
    }
    if (!first) return NULL;
    for (num = (int)(RANDOM() % num); num--; first = first->pnext);
    return first;
}


/**************************
 * Routines to add macros *
 **************************/

/* create a Macro */
Macro *new_macro(name, trig, bind, hook, hargs, body, world, pri, prob,
  attr, shots, invis)
    char *name, *trig, *bind, *body, *hargs;
    int hook, pri, prob, attr, shots, invis;
    World *world;
{
    Macro *new;

    new = (Macro *) MALLOC(sizeof(struct Macro));
    new->next = new->prev = new->bnext = new->bprev = NULL;
    new->tnext = new->tprev = new->hnext = new->hprev = NULL;
    new->name = STRDUP(name);
    new->trig = STRDUP(trig);
    new->bind = STRDUP(bind);
    new->hook = hook;
    new->hargs = STRDUP(hargs);
    new->body = STRDUP(body);
    new->world = world;
    new->hook = hook;
    new->pri = pri;
    new->prob = prob;
    new->attr = attr;
    new->shots = shots;
    new->invis = invis;
    new->temp = TRUE;
    if (cstrncmp(new->body, "/DOKEY ", 7) == 0)
        new->func = find_efunc(stripstr(new->body + 7));
    else new->func = NULL;

    return new;
}

/* add permanent Macro to appropriate structures */
/* add_macro() assumes there is no confict with existing names */
void add_macro(macro)
    Macro *macro;
{
    macro->num = mnum++;
    Insert(macro, head, next, prev);
    if (tail == NULL) tail = macro;
    if (*macro->name) hash_insert(macro);
    if (*macro->trig) insert_by_priority(macro, FALSE);
    if (macro->hook) insert_by_priority(macro, TRUE);
    macro->temp = FALSE;
}

static void insert_by_priority(macro, hook)        /* insert trig or hook */
    Macro *macro;
    int hook;
{
    Macro *p;

    if (Head == NULL || macro->pri >= Head->pri) {
        Prev(macro) = NULL;
        Next(macro) = Head;
        Head = macro;
    } else {
        for (p = Head; Next(p) && macro->pri < Next(p)->pri; p = Next(p));
        Prev(macro) = p;
        Next(macro) = Next(p);
        Next(p) = macro;
    }
    if (Next(macro)) Prev(Next(macro)) = macro;
}

int install_bind(spec)          /* install Macro's binding in key structures */
    Macro *spec;
{
    Macro *macro;

    if (macro = find_key(spec->bind)) {
        if (redef) {
            do_hook(H_REDEF, "%% Redefined %s %s", "%s %s",
                "binding", keyname(spec->bind));
            nuke_macro(macro);
        } else return 0;
    }
    if (bind_key(spec)) return 1;  /* fails if is prefix or has prefix */
    nuke_macro(spec);
    return 0;
}

void do_add(spec)                          /* define a new Macro (general) */
    Macro *spec;
{
    Macro *macro = NULL;

    if (spec->name && find_command(spec->name)) {
        oprintf("%% /%s is a builtin command", spec->name);
        nuke_macro(spec);
        return;
    }

    if (spec->world == &AnyWorld) spec->world = NULL;
    if (spec->pri == -1) spec->pri = 1;
    if (spec->prob == -1) spec->prob = 100;
    if (spec->shots == -1) spec->shots = 0;
    if (spec->invis) spec->invis = 1;
    if (spec->attr & F_NORM || !spec->attr) spec->attr = F_NORM;
    if (!spec->trig) spec->trig = STRDUP("");
    if (!spec->bind) spec->bind = STRDUP("");
    if (!spec->name) spec->name = STRDUP("");
    if (!spec->body) spec->body = STRDUP("");

    if (*spec->name && (macro = find_macro(spec->name)) && !redef) {
        oprintf("%% Macro %s already exists", spec->name);
        nuke_macro(spec);
        return;
    }
    if (*spec->bind && !install_bind(spec)) return;
    add_macro(spec);
    if (macro) {
        do_hook(H_REDEF, "%% Redefined %s %s", "%s %s", "macro", spec->name);
        nuke_macro(macro);
    }
}

void add_hook(name, body)                  /* define a new Macro with hook */
    char *name, *body;
{
    int hook;
    char *hargs;

    if ((hargs = strchr(name, ' ')) != NULL) *hargs++ = '\0';
    if (!(hook = parse_hook(name))) return;
    hargs = STRDUP(hargs ? stripstr(hargs) : "");
    add_macro(new_macro("", "", "", hook, hargs, body, NULL, 0, 100, 1, 0, 0));
}

void do_edit(spec)                         /* edit an existing Macro */
    Macro *spec;
{
    Macro *macro;
    int number;

    if (spec->name == NULL) {
        oputs("% You must specify a macro.");
        nuke_macro(spec);
        return;
    } else if (spec->name[0] == '#') {
        number = atoi(spec->name + 1);
        for (macro = head; macro != NULL; macro = macro->next)
            if (macro->num == number) break;
        if (macro == NULL) {
            oprintf("%% Macro #%d does not exist", number);
            nuke_macro(spec);
            return;
        }
    } else if (spec->name[0] == '$') {
        if ((macro = match_exact(FALSE, spec->name + 1, F_ALL)) == NULL) {
            oprintf("%% Trigger %s does not exist", spec->name + 1);
            nuke_macro(spec);
            return;
        }
    } else if ((macro = find_macro(spec->name)) == NULL) {
        oprintf("%% Macro %s does not exist", spec->name);
        nuke_macro(spec);
        return;
    }

    if (spec->body && *spec->body) {
        FREE(macro->body);
        macro->body = STRDUP(spec->body);
    }
    if (spec->trig && *spec->trig) {
        FREE(macro->trig);
        macro->trig = STRDUP(spec->trig);
        if (spec->pri != -1) {                    /* re-link if pri changed */
            Unlink(macro, thead, tnext, tprev);
            insert_by_priority(macro, FALSE);
        }
    }
    if (spec->hook) {
        macro->hook = spec->hook;
        if (*macro->hargs) FREE(macro->hargs);
        macro->hargs = STRDUP((spec->hargs && *spec->hargs) ? spec->hargs : "");
        if (spec->pri != -1) {                    /* re-link if pri changed */
            Unlink(macro, hhead, hnext, hprev);
            insert_by_priority(macro, TRUE);
        }
    }
    if (spec->world && spec->world != &AnyWorld) macro->world = spec->world;
    if (spec->pri != -1) macro->pri = spec->pri;
    if (spec->prob != -1) macro->prob = spec->prob;
    if (spec->shots != -1) macro->shots = spec->shots;
    if (spec->attr & F_NORM) macro->attr = F_NORM;
    else if (spec->attr & F_GAG) macro->attr = spec->attr & F_SUPERGAG;
    else if (spec->attr) macro->attr = spec->attr;
    macro->invis = (spec->invis) ? 1 : 0;
    nuke_macro(spec);
}

static int hash_string(str, size)          /* convert string to hash index */
    char *str;
    int size;
{
    unsigned int hashval;

    for (hashval = 0; *str; str++) hashval = lcase(*str) + 33 * hashval;
    return (int)(hashval % (unsigned int)size);
}

static void hash_insert(macro)             /* add Macro to hash table by name */
    Macro *macro;
{
    int index;

    index = hash_string(macro->name, HASH_SIZE);
    Insert(macro, hash_table[index], bnext, bprev);
}


/********************************
 * Routines for removing macros *
 ********************************/

void nuke_macro(macro)            /* remove Macro from all lists, and free it */
    Macro *macro;
{
    if (!macro->temp) {
        if (*macro->name) hash_remove(macro);
        if (macro == tail) tail = macro->prev;
        Unlink(macro, head, next, prev);
        if (macro->trig && *macro->trig) Unlink(macro, thead, tnext, tprev);
        if (macro->hook) Unlink(macro, hhead, hnext, hprev);
        if (*macro->bind) unbind_key(macro);
    }

    if (macro->name) FREE(macro->name);
    if (macro->body) FREE(macro->body);
    if (macro->trig) FREE(macro->trig);
    if (macro->hargs) FREE(macro->hargs);
    if (macro->bind) FREE(macro->bind);
    FREE(macro);
}

void remove_macro(str, flags, byhook)        /* delete a macro */
    char *str;
    int flags;
    int byhook;
{
    Macro *macro = NULL;
    char *args;

    if (byhook) {
        if ((args = strchr(str, ' ')) != NULL) *args++ = '\0';
        if ((flags = parse_hook(str)) == 0) return;
        if ((macro = match_exact(byhook, args ? args : "", flags)) == NULL)
            oprintf("%% Hook on \"%s\" was not defined.", str);
    } else if (flags) {
        if ((macro = match_exact(byhook, str, flags)) == NULL)
            oprintf("%% Trigger on \"%s\" was not defined.", str);
    } else {
        if ((macro = find_macro(str)) == NULL)
            oprintf("%% Macro \"%s\" was not defined.", str);
    }
    if (macro) nuke_macro(macro);
}

void purge_macro(spec)                   /* delete all specified macros */
    Macro *spec;
{
    Macro *macro, *next;

    if (!spec) return;
    if (spec->name && !smatch_check(spec->name)) return;
    if (spec->trig && !smatch_check(spec->trig)) return;
    if (spec->hargs && !smatch_check(spec->hargs)) return;
    if (spec->bind && !smatch_check(spec->bind)) return;
    if (spec->body && !smatch_check(spec->body)) return;
    for (macro = head; macro; macro = next) {
        next = macro->next;
        if (macmatch(spec, macro) == 0) nuke_macro(macro);
    }
    nuke_macro(spec);
}

void remove_by_number(args)                 /* delete macro by number */
    char *args;
{
    char *ptr;
    int num;
    Macro *p;

    ptr = args;
    do {
        while (isspace(*ptr)) ptr++;
        num = atoi(ptr);
        if (num > 0 && num < mnum) {
            for (p = head; p != NULL; p = p->next) if (p->num == num) break;
            if (p != NULL) nuke_macro(p);
            else oprintf("%% No macro with number %d", num);
        } else oprintf("%% Invalid number %d", num);
        ptr = strchr(ptr, ' ');
    } while (ptr != NULL);
}

static void hash_remove(macro)              /* remove macro from hash table */
    Macro *macro;
{
    int index;

    index = hash_string(macro->name, HASH_SIZE);
    Unlink(macro, hash_table[index], bnext, bprev);
}


/**************************
 * Routine to list macros *
 **************************/

static String *hook_name(hook)        /* convert hook bitfield to string */
    int hook;
{
    int n;
    STATIC_BUFFER(buf)

    Stringterm(buf, 0);
    for (n = 0; n < NUM_HOOKS; n++) {
        if (!((1 << n) & hook)) continue;
        if (buf->len) Stringadd(buf, '|');
        Stringcat(buf, hook_table[n]);
    }
    return buf;
}

static String *escape(src)                /* insert '\' before each '"' */
    char *src;
{
    char *ptr;
    STATIC_BUFFER(dest)

    for (Stringterm(dest, 0); *src; src = ptr) {
        if (*src == '\"' || *src == '\\') Sprintf(dest, "\200\\%c", *src++);
        for (ptr = src; *ptr && *ptr != '\"' && *ptr != '\\'; ptr++);
        Stringncat(dest, src, ptr - src);
    }
    return dest;
}

static void list_defs(file, spec, abbrev)    /* list all specified macros */
    TFILE *file;
    Macro *spec;
    int abbrev;
{
    Macro *p;
    STATIC_BUFFER(buffer)

    if (spec->name && !smatch_check(spec->name)) return;
    if (spec->trig && !smatch_check(spec->trig)) return;
    if (spec->hargs && !smatch_check(spec->hargs)) return;
    if (spec->bind && !smatch_check(spec->bind)) return;
    if (spec->body && !smatch_check(spec->body)) return;

    for (p = tail; p != NULL; p = p->prev) {
        if (macmatch(spec, p) != 0) continue;

        if (abbrev) {
            Sprintf(buffer, "%% %d: ", p->num);
            if (p->attr & F_NORECORD) Stringcat(buffer, "(norecord) ");
            if (p->attr & F_GAG) Stringcat(buffer, "(gag) ");
            else if (p->attr & ~F_NORM) {
                if (p->attr & F_UNDERLINE) Stringcat(buffer, "(underline) ");
                if (p->attr & F_REVERSE)   Stringcat(buffer, "(reverse) ");
                if (p->attr & F_FLASH)     Stringcat(buffer, "(flash) ");
                if (p->attr & F_DIM)       Stringcat(buffer, "(dim) ");
                if (p->attr & F_HILITE)    Stringcat(buffer, "(hilite) ");
                if (p->attr & F_BELL)      Stringcat(buffer, "(bell) ");
            } else if (*p->trig) Stringcat(buffer, "(trig) ");
            if (*p->trig) Sprintf(buffer, "\200\"%S\" ", escape(p->trig));
            if (*p->bind)
                Sprintf(buffer, "\200(bind) \"%S\" ", escape(keyname(p->bind)));
            if (p->hook) Sprintf(buffer, "\200(hook) %S ", hook_name(p->hook));
            if (*p->name) Sprintf(buffer, "\200%s ", p->name);

        } else {
            if (!file) Sprintf(buffer, "%% %d: /def ", p->num);
            else Stringcpy(buffer, "/def ");
            if (p->invis) Stringcat(buffer, "-i ");
            if ((*p->trig || p->hook) && p->pri)
                Sprintf(buffer, "\200-p%d ", p->pri);
            if (*p->trig && p->prob != 100)
                Sprintf(buffer, "\200-c%d ", p->prob);
            if (p->attr & F_GAG) {
                Stringcat(buffer, "-ag");
                if (p->attr & F_NORECORD)  Stringadd(buffer, 'G');
                Stringadd(buffer, ' ');
            } else if (p->attr & ~F_NORM) {
                Stringcat(buffer, "-a");
                if (p->attr & F_NORECORD)  Stringadd(buffer, 'G');
                if (p->attr & F_UNDERLINE) Stringadd(buffer, 'u');
                if (p->attr & F_REVERSE)   Stringadd(buffer, 'r');
                if (p->attr & F_FLASH)     Stringadd(buffer, 'f');
                if (p->attr & F_DIM)       Stringadd(buffer, 'd');
                if (p->attr & F_HILITE)    Stringadd(buffer, 'h');
                if (p->attr & F_BELL)      Stringadd(buffer, 'b');
                Stringadd(buffer, ' ');
            }
            if (p->world) Sprintf(buffer, "\200-w%s ", p->world->name);
            if (p->shots) Sprintf(buffer, "\200-n%d ", p->shots);
            if (*p->trig) Sprintf(buffer, "\200-t\"%S\" ", escape(p->trig));
            if (p->hook) {
                Sprintf(buffer, "\200-h\"%S", hook_name(p->hook));
                if (*p->hargs) Sprintf(buffer, "\200 %S", escape(p->hargs));
                Stringcat(buffer, "\" ");
            }
            if (*p->bind) 
                Sprintf(buffer, "\200-b\"%S\" ", escape(keyname(p->bind)));
            if (*p->name) Sprintf(buffer, "\200%s ", p->name);
            if (*p->body) Sprintf(buffer, "\200= %s", p->body);
        }

        if (file) {
            Stringadd(buffer, '\n');
            tfputs(buffer->s, file);
        } else oputs(buffer->s);
    }
}

void save_macros(args)              /* write specified macros to file */
    char *args;
{
    char *p;
    Macro *spec;
    TFILE *file;

    for (p = args; *p && !isspace(*p); p++);
    if (*p) *p++ = '\0';
    if ((file = tfopen(args, NULL, "w")) == NULL) return;
    oprintf("%% Saving macros to %s", file->name);
    if ((spec = macro_spec(p)) == NULL) return;
    list_defs(file, spec, FALSE);
    tfclose(file);
    nuke_macro(spec);
}

void list_macros(args)                    /* list specified macros on screen */
    char *args;
{
    Macro *spec;
    char *p;
    int abbrev = FALSE;

    for (p = args; isspace(*p); p++);
    if (p[0] == '-' && p[1] == 's' && (p[2] == '\0' || isspace(p[2]))) {
        args = p + 2;
        abbrev = TRUE;
    }
    if ((spec = macro_spec(args)) == NULL) return;
    list_defs(NULL, spec, abbrev);
    nuke_macro(spec);
}


/**************************
 * Routines to use macros *
 **************************/

int world_subs(src, dest)            /* evaulate "world_*" subtitutions */
    char *src;
    Stringp dest;
{
    char *ptr = src + 6;
    World *world, *def;

    if (cstrncmp("world_", src, 6) != 0) return 0;
    if (!(world = xworld())) return 1;
    def = get_default_world();

    if (!cstrcmp("name", ptr)) Stringcpy(dest, world->name);
    else if (!cstrcmp("character", ptr)) {
        if (*world->character) Stringcpy(dest, world->character);
        else if (def != NULL) Stringcpy(dest, def->character);
    } else if (!cstrcmp("password", ptr)) {
        if (*world->pass) Stringcpy(dest, world->pass);
        else if (def != NULL) Stringcpy(dest, def->pass);
    } else if (!cstrcmp("host", ptr)) Stringcpy(dest,  world->address);
    else if (!cstrcmp("port", ptr)) Stringcpy(dest, world->port);
    else if (!cstrcmp("mfile", ptr)) {
        if (*world->mfile) Stringcpy(dest, world->mfile);
        else if (def != NULL) Stringcpy(dest, def->mfile);
    }
    return 1;
}

void do_macro(name, args, dest, toplevel)       /* Do a macro! */
    char *name, *args;
    Stringp dest;
    int toplevel;
{
    Macro *macro;

    if (world_subs(name, dest)) return;
    if (!(macro = find_macro(name))) oprintf("%% Macro not defined: %s", name);
    else if (macro->func) (*macro->func)();
    else process_macro(macro->body, args, dest, toplevel);
}

void get_macro_body(name, dest)             /* get body of macro, duh */
    char *name;
    Stringp dest;
{
    Macro *m;

    if (world_subs(name, dest)) return;
    if ((m = find_macro(name)) == NULL)
        oprintf("%% Macro not defined: %s", name);
    else Stringcpy(dest, m->body);
}


/****************************************
 * Routines to check triggers and hooks *
 ****************************************/

#ifdef STANDARD_C_YES_SIREE
short do_hook(int idx, char *fmt, char *argfmt, ...)     /* do a hook event */
#else
/* VARARGS */
short do_hook(va_alist)
va_dcl
#endif
{
#ifndef STANDARD_C_YES_SIREE
    int idx;
    char *fmt, *argfmt;
#endif
    va_list ap;
    Macro *macro;
    short attr = 0;
    static Stringp buf, args;
    static int buffers_initted = FALSE;
    extern int hookflag;

    if (!buffers_initted) {
        Stringinit(buf);
        Stringinit(args);
        buffers_initted = TRUE;
    }

#ifdef STANDARD_C_YES_SIREE
    va_start(ap, argfmt);
#else
    va_start(ap);
    idx = va_arg(ap, int);
    fmt = va_arg(ap, char *);
    argfmt = va_arg(ap, char *);
#endif
    Stringterm(args, 0);
    vSprintf(args, argfmt, ap);
    va_end(ap);

    if (macro = find_match((1<<idx), args->s)) attr = macro->attr;

    if (fmt) {
#ifdef STANDARD_C_YES_SIREE
        va_start(ap, argfmt);
#else
        va_start(ap);
        idx = va_arg(ap, int);
        fmt = va_arg(ap, char *);
        argfmt = va_arg(ap, char *);
#endif
        vSprintf(buf, fmt, ap);
        output(buf->s, attr | F_NEWLINE);
    }
    va_end(ap);

    if (hookflag && macro && !(RANDOM() % 100 > macro->prob))
        run_trig_or_hook(macro, args->s);
    return attr;
}

short check_trigger(text)                 /* look for trigger, and call it */
    char *text;
{
    extern int borg;
    Macro *macro;
    short attr;

    if ((macro = find_match(0, text)) == NULL) return 1;
    attr = macro->attr;
    if (borg && RANDOM() % 100 <= macro->prob && run_trig_or_hook(macro, text))
        background_hook(text);
    return attr;
}

static int run_trig_or_hook(macro, text)
    Macro *macro;
    char *text;
{
    int result = 0, need_dup;
    char *body;
    Stringp dst;
    void NDECL((*func));

    need_dup = (!(func = macro->func) && macro->shots == 1);
    body = need_dup ? STRDUP(macro->body) : macro->body;
    if (macro->shots && !--macro->shots) nuke_macro(macro);
    if (func) {
        result = TRUE;
        (*func)();
    } else if (*body) {
        result = TRUE;
        Stringinit(dst);
        process_macro(body, text, dst, 1);
        Stringfree(dst);
    }
    if (need_dup) FREE(body);
    return result;
}

