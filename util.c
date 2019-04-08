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

/***********************************
 * Fugue utilities.                *
 *                                 *
 * Written by Greg Hudson and Ken  *
 * Keys.                           *
 *                                 *
 * Uppercase/lowercase table       *
 * Memory allocation routines      *
 * String handling routines        *
 * File utilities                  *
 * Mail checker                    *
 * Cleanup routine                 *
 ***********************************/

#include <sys/types.h>
#ifdef MAILDELAY
#include <sys/stat.h>
#endif
#include <ctype.h>
#include <stdio.h>
#include "tf.h"
#include "dstring.h"
#include "util.h"
#include "output.h"
#include "macro.h"
#include "socket.h"
#include "keyboard.h"

static int FDECL(cmatch,(char *s1, int c1));
static int FDECL(wmatch,(char *wlist, char **s2));

int    FDECL(smatch,(char *s1, char *s2));
int    FDECL(smatch_check,(char *s1));
TFILE *FDECL(tfopen,(char *fname, char *dname, char *mode));
void   NDECL(init_table);
#ifndef DMALLOC
char  *FDECL(dmalloc,(long unsigned size));
char  *FDECL(drealloc,(char *ptr, long unsigned size));
Aline *FDECL(new_aline,(char *str, int attrs));
#endif
void   FDECL(free_aline,(Aline *aline));
#ifdef _STRSTR
char  *FDECL(tf_strstr,(char *s1, char *s2));
#endif
char  *FDECL(cstrchr,(char *s, int c));
char  *FDECL(estrchr,(char *s, int c, int e));
int    FDECL(cstrcmp,(char *s, char *t));
int    FDECL(cstrncmp,(char *s, char *t, int n));
int    FDECL(numarg,(char **str));
char  *FDECL(stripstr,(char *s));
void   FDECL(vSprintf,(Stringp buf, char *fmt, va_list ap));
void   VDECL(oprintf,(char *fmt, ...));
void   VDECL(Sprintf,(Stringp buf, char *fmt, ...));
char   NDECL(getch);
void   FDECL(startopt,(char *args, char *opts));
char   FDECL(nextopt,(char **arg, int *num));
#ifdef MAILDELAY
void   NDECL(check_mail);
#endif
void   NDECL(cleanup);
void   FDECL(die,(char *why));

int mail_size = 0;                                    /* size of mail file */
char lowercase_values[128], uppercase_values[128];    /* for lcase(), ucase() */

/* A number of non-standard compilers don't properly handle tolower()
 * or toupper() called on a character that wouldn't ordinarily be
 * converted.  Fugue uses its own table (referred to by the ucase()
 * and lcase() macros in util.h) to ensure correct conversions.
 */
void init_table()
{
    int i;

    for (i = 0; i < 128; i++) {
        lowercase_values[i] = isupper(i) ? tolower(i) : i;
        uppercase_values[i] = islower(i) ? toupper(i) : i;
    }
}

TFILE *tfopen(fname, dname, mode)
    char *fname, *dname, *mode;
{
    smallstr filter;
    int ispipe = FALSE;
    FILE *fp;
    TFILE *result = NULL;
    static Stringp filename, command, suffix;
    static int buffers_initted = FALSE;

    if (!buffers_initted) {
        Stringinit(filename);
        Stringinit(command);
        Stringinit(suffix);
    }

    if (fname && *fname) {
        Stringcpy(filename, fname);
    } else if (dname) {
        get_macro_body(dname, filename);
    } else {
        oputs("% No filename");
        return NULL;
    }
    Stringexpand(filename);
    if ((fp = fopen(filename->s, mode)) == NULL && errno == ENOENT) {
        get_macro_body("compress_suffix", suffix);
        SStringcat(filename, suffix);
        if ((fp = fopen(filename->s, mode)) != NULL) {
            fclose(fp);
            sprintf(filter, "compress_%s", (*mode == 'r') ? "read" : "write");
            get_macro_body(filter, command);
            Sprintf(command, "\200 %S", filename);
            fp = popen(command->s, mode);
            ispipe = TRUE;
        }
        Stringterm(filename, filename->len - suffix->len);
    }
    if (!fp)
        do_hook(H_LOADFAIL, "%% %S: %s", "%S %s", filename, STRERROR(errno));
    else {
        result = (TFILE*)MALLOC(sizeof(TFILE));
        result->fp = fp;
        result->ispipe = ispipe;
        result->name = STRDUP(filename->s);
    }
    return result;
}

/* can't fseek() on a pipe */
int pipejump(file, offset)
    TFILE *file;
    long offset;
{
    char buffer[BUFSIZ];

    while ((offset > BUFSIZ) && fread(buffer, sizeof(char), BUFSIZ, file->fp))
        offset -= BUFSIZ;
    return fread(buffer, sizeof(char), offset, file->fp) ? 1 : 0;
}

/* Error-checking memory allocation routines.
 */

#ifndef DMALLOC
char *dmalloc(size)
    long unsigned size;
{
    char *ret;

    if (size == 0) die("% dmalloc(0) called");
    if ((ret = malloc(size)) == NULL) die("% malloc failed");
    return ret;
}

char *drealloc(ptr, size)
    char *ptr;
    long unsigned size;
{
    char *ret;

    if (size == 0) die("% drealloc(ptr, 0) called");
    if (ret = realloc(ptr, size)) return ret;
    die("% realloc failed");
    return NULL;
}
#endif

/* String handlers
 * Some of these are already present in most C libraries, but go by
 * different names or are not always there.  Since they're small, TF
 * simply uses its own routines with non-standard but consistant naming.
 * These are heavily used functions, so speed is favored over simplicity.
 */

/* case-insensitive strchr() */
char *cstrchr(s, c)
    register char *s, c;
{
    for (c = lcase(c); *s; s++) if (lcase(*s) == c) return s;
    return (c) ? NULL : s;
}

/* c may be escaped by preceeding it with e */
char *estrchr(s, c, e)
    register char *s, c, e;
{
    while (*s) {
        if (*s == c) return s;
        if (*s == e) {
            if (*++s) s++;
        } else s++;
    }
    return NULL;
}

#ifdef _STRSTR
char *tf_strstr(s1, s2) 
    register char *s1, *s2;
{
    int len;

    for (len = strlen(s2); s1 = strchr(s1, *s2); s1++) {
        if (strncmp(s1, s2, len) == 0) return s1;
    }
    return NULL;
}
#endif

/* case-insensitive strcmp() */
int cstrcmp(s, t)
    register char *s, *t;
{
    while (*s && *t && lcase(*s) == lcase(*t)) s++, t++;
    return lcase(*s) - lcase(*t);
}

/* case-insensitive strncmp() */
int cstrncmp(s, t, n)
    register char *s, *t;
    int n;
{
    while (n && *s && *t && lcase(*s) == lcase(*t)) s++, t++, n--;
    return (n == 0) ? 0 : lcase(*s) - lcase(*t);
}

/* convert numeric argument.  *str will advance to beginning of next */
/* argument, or be NULL for failure. */
int numarg(str)
    char **str;
{
    char *start, *temp;
    for (start = temp = *str; isdigit(*temp); temp++);
    if (temp == *str) {
        *str = NULL;
        return 0;
    }
    for (*str = temp; isspace(**str); (*str)++);
    return atoi(start);
}

static int cmatch(s1, c1)
    char *s1, c1;
{
    int result = FALSE;

    c1 = lcase(c1);
    if (*s1 == '^') {
        s1++;
        result = TRUE;
    }
    while (*s1) {
        if (*s1 == '\\') s1++;                   /* note that *(s1+1) != '\0' */
        if (*(s1 + 1) == '-' && *(s1 + 2)) {
            char c = *s1, *end = s1 + 2;

            if (*end == '\\') end++;
            if (c > *end) {
                s1++;
                if (lcase(c) == c1) return result;
            } else {
                while (c <= *end) if (lcase(c++) == c1) return result;
                s1 = end + 1;
            }
        } else if (lcase(*s1++) == c1) return result;
    }
    return !result;
}

static int wmatch(wlist, s2)
    char *wlist;     /* word list                      */
    char **s2;       /* buffer to match from           */
{
    char *matchstr,  /* which word to find             */
         *strend,    /* end of current word from wlist */
         *matchbuf,  /* where to find from             */
         *bufend;    /* end of match buffer            */
    int  result = 1; /* intermediate result            */

    if (!wlist || !*s2) return 1;
    matchbuf = *s2;
    matchstr = wlist;
    if ((bufend = strchr(matchbuf, ' ')) == NULL)
        *s2 += strlen(*s2);
    else
        *(*s2 = bufend) = '\0';
    do {
        if ((strend = estrchr(matchstr, '|', '\\')) != NULL)
            *strend = '\0';
        result = smatch(matchstr, matchbuf);
        if (strend != NULL) *strend++ = '|';
    } while (result && (matchstr = strend) != NULL);
    if (bufend != NULL) *bufend = ' ';
    return result;
}

/* smatch_check() should be used on s1 to check pattern syntax before
 * calling smatch().
 */

int smatch(s1, s2)
    char *s1, *s2;
{
    char ch, *start = s2;

    while (*s1) {
        switch (*s1) {
        case '\\':
            s1++;
            if (lcase(*s1++) != lcase(*s2++)) return 1;
            break;
        case '?':
            if (!*s2++) return 1;
            s1++;
            break;
        case '*':
            while (*s1 == '*' || (*s1 == '?' && *s2++)) s1++;
            if (*s1 == '?') return 1;
            if (*s1 == '{') {
                if (s2 == start || *(s2 - 1) == ' ')
                    if (!smatch(s1, s2)) return 0;
                while ((s2 = strchr(s2, ' ')) != NULL)
                    if (!smatch(s1, ++s2)) return 0;
                return 1;
            } else if (*s1 == '[') {
                while (*s2) if (!smatch(s1, s2++)) return 0;
                return 1;
            }
            ch = (*s1 == '\\' && *(s1 + 1)) ? *(s1 + 1) : *s1;
            while ((s2 = cstrchr(s2, ch)) != NULL) {
                if (!smatch(s1, s2++)) return 0;
            }
            return 1;
        case '[':
            {
                char *end;

                if (!(end = estrchr(s1, ']', '\\'))) {  /* Shouldn't happen  */
                    oputs("% smatch: unmatched '['");   /* if smatch_check() */
                    return 1;                           /* was used first.   */
                }
                *end = '\0';
                if (cmatch(s1 + 1, *s2++)) {
                    *end = ']';
                    return 1;
                }
                *end = ']';
                s1 = end + 1;
            }
            break;
        case '{':
            if (s2 != start && !isspace(*(s2 - 1))) return 1;
            {
                char *end;

                if (!(end = estrchr(s1, '}', '\\'))) {  /* Shouldn't happen  */
                    oputs("% smatch: unmatched '{'");   /* if smatch_check() */
                    return 1;                           /* was used first.   */
                }
                *end = '\0';
                if (wmatch(s1 + 1, &s2)) {
                    *end = '}';
                    return 1;
                }
                *end = '}';
                s1 = end + 1;
            }
            break;
        default:
            if(lcase(*s1++) != lcase(*s2++)) return 1;
            break;
        }
    }
    return lcase(*s1) - lcase(*s2);
}

/* verify syntax of smatch pattern */
int smatch_check(s1)
    char *s1;
{
    int inword = FALSE;

    while (*s1) {
        switch (*s1) {
        case '\\':
            if (*++s1) s1++;
            break;
        case '[':
            if (!(s1 = estrchr(s1, ']', '\\'))) {
                oputs("% pattern error: unmatched '['");
                return 0;
            }
            s1++;
            break;
        case '{':
            if (inword) {
                oputs("% pattern error: nested '{'");
                return 0;
            }
            inword = TRUE;
            s1++;
            break;
        case '}':
            inword = FALSE;
            s1++;
            break;
        case '?':
        case '*':
        default:
            s1++;
            break;
        }
    }
    if (inword) oputs("% pattern error: unmatched '{'");
    return !inword;
}

/* remove leading and trailing spaces */
char *stripstr(s)
    char *s;
{
    char *start, *end;

    if (!*s) return 0;
    for (start = s; isspace(*start); start++);
    if (*start) {
        for (end = start + strlen(start) - 1; isspace(*end); end--);
        *++end = '\0';
    } else end = start;
    if (start != s) strcpy(s, start);
    return s;
}

/*
 * vSprintf take a format string similar to vsprintf, except:
 * if first char of fmt is \200, args will be appended to buffer;
 * no length formating for %s; %S expects a Stringp argument.
 */

void vSprintf(buf, fmt, ap)
    Stringp buf;
    char *fmt;
    va_list ap;
{
    static smallstr lfmt, tempbuf;
    char *p, *q, cval, *sval, *lfmtptr;
    String *Sval;
    int ival;
    unsigned uval;
    double dval;

    if (*fmt == '\200') fmt++;
    else Stringterm(buf, 0);
    for (p = fmt; *p; p++) {
        if (*p != '%' || *++p == '%') {
            for (q = p + 1; *q && *q != '%'; q++);
            Stringncat(buf, p, q - p);
            p = q - 1;
            continue;
        }
        lfmtptr = lfmt;
        *lfmtptr++ = '%';
        while (*p && !isalpha(*p)) *lfmtptr++ = *p++;
        *lfmtptr++ = *p;
        *lfmtptr = '\0';
        switch (*p) {
        case 'd':
        case 'i':
            ival = va_arg(ap, int);
            sprintf(tempbuf, lfmt, ival);
            Stringcat(buf, tempbuf);
            break;
        case 'x':
        case 'X':
        case 'u':
        case 'o':
            uval = va_arg(ap, unsigned);
            sprintf(tempbuf, lfmt, uval);
            Stringcat(buf, tempbuf);
            break;
        case 'f':
            dval = va_arg(ap, double);
            sprintf(tempbuf, lfmt, dval);
            Stringcat(buf, tempbuf);
            break;
        case 'c':
            cval = (char)va_arg(ap, int);
            Stringadd(buf, cval);
            break;
        case 's':                       /* Sorry, no length formatting */
            sval = va_arg(ap, char *);
            Stringcat(buf, sval);
            break;
        case 'S':
            Sval = va_arg(ap, String *);
            SStringcat(buf, Sval);
            break;
        default:
            Stringcat(buf, lfmt);
            break;
        }
    }
}

/* oprintf */
/* Newlines are not allowed in the format string (this is not enforced). */
/* A newline will appended to the string. */

#ifdef STANDARD_C_YES_SIREE
void oprintf(char *fmt, ...)
#else
/* VARARGS */
void oprintf(va_alist)
va_dcl
#endif
{
    va_list ap;
#ifndef STANDARD_C_YES_SIREE
    char *fmt;
#endif
    STATIC_BUFFER(buffer)

#ifdef STANDARD_C_YES_SIREE
    va_start(ap, fmt);
#else
    va_start(ap);
    fmt = va_arg(ap, char *);
#endif
    vSprintf(buffer, fmt, ap);
    va_end(ap);
    oputs(buffer->s);
}

#ifdef STANDARD_C_YES_SIREE
void Sprintf(String *buffer, char *fmt, ...)
#else
/* VARARGS */
void Sprintf(va_alist)
va_dcl
#endif
{
    va_list ap;
#ifndef STANDARD_C_YES_SIREE
    String *buffer;
    char *fmt;
#endif

#ifdef STANDARD_C_YES_SIREE
    va_start(ap, fmt);
#else
    va_start(ap);
    buffer = va_arg(ap, String *);
    fmt = va_arg(ap, char *);
#endif
    vSprintf(buffer, fmt, ap);
    va_end(ap);
}

/* Input handlers
 */
char getch()
{
    char c;
    fd_set readers;

    FD_ZERO(&readers);
    FD_SET(0, &readers);
    while(select(1, &readers, NULL, NULL, NULL) <= 0);
    read(0, &c, 1);
    return c;
}

/* General command option parser

   startopt should be called before nextopt.  args is the argument list
   to be parsed, opts is a string containing valid options.  Options which
   take string arguments should be followed by a ':'; options which take
   numeric argumens should be followed by a '#'.  String arguments may be
   omitted.

   nextopt returns the next option character.  If option takes a string
   argument, a pointer to it is returned in *arg; an integer argument
   is returned in *num.  If end of options is reached, nextopt returns
   '\0', and *arg points to remainder of argument list.  End of options
   is marked by '\0', '=', '-' by itself, or a word not beggining with
   '-'.  If an invalid option is encountered, an error message is
   printed and '?' is returned.

   Option Syntax Rules:
      All options must be preceded by '-'.
      Options may be grouped after a single '-'.
      There must be no space between an option and its argument.
      String option-arguments may be quoted.  Quotes in the arg must be escaped.
      All options must precede operands.
      A '-' with no option may be used to mark the end of the options.
      The relative order of the options should not matter (not enforced).
*/

static char *argp, *options;
static int inword;

void startopt(args, opts)
    char *args, *opts;
{
    argp = args;
    options = opts;
    inword = 0;
}

char nextopt(arg, num)
    char **arg;
    int *num;
{
    short error = FALSE;
    char *q, opt;
    STATIC_BUFFER(buffer)

    if (!inword) {
        while (isspace(*argp)) argp++;
        if (*argp != '-' || !*++argp || isspace(*argp)) {
            for (*arg = argp; isspace(**arg); ++*arg);
            return '\0';
        }
    } else if (*argp == '=') {        /* '=' marks end, & is part of parms */
        *arg = argp;                  /*... for stuff like  /def -t"foo"=bar */
        return '\0';
    }
    opt = *argp;
    if (opt == ':' || opt == '#') error = TRUE;
    else if ((q = strchr(options, opt)) != NULL) ;
    else if (isdigit(opt) && (q = strchr(options, '0'))) ;
    else error = TRUE;
    if (error) {
        oprintf("%% invalid option: %c", opt);
        return '?';
    }
    if (*q == '0') {
        *num = atoi(argp);
        while (isdigit(*++argp));
        return '0';
    } else if (*++q == ':') {
        Stringterm(buffer, 0);
        if (*++argp == '"') {
            for (argp++; *argp && *argp != '"'; Stringadd(buffer, *argp++))
                if (*argp == '\\' && (argp[1] == '"' || argp[1] == '\\'))
                    argp++;
            if (!*argp) {
                oprintf("%% unmatched \" in %c option", opt);
                return '?';
            } else argp++;
        } else while (*argp && !isspace(*argp)) Stringadd(buffer, *argp++);
        *arg = buffer->s;
    } else if (*q == '#') {
        argp++;
        if (!isdigit(*argp)) {
            oprintf("%% %c option requires numeric argument", opt);
            return '?';
        }
        *num = atoi(argp);
        while (isdigit(*++argp));
    } else argp++;
    inword = (*argp && !isspace(*argp));
    return opt;
}

#ifndef DMALLOC
Aline *new_aline(str, attrs)
    char *str;
    int attrs;
{
    Aline *aline;

    aline = (Aline *)MALLOC(sizeof(Aline));
    aline->str = str ? STRDUP(str) : NULL;
    aline->attrs = attrs;
    aline->links = 0;
    return aline;
}
#endif

void free_aline(aline)
    Aline *aline;
{
    if (!--(aline->links)) {
        FREE(aline->str);
        FREE(aline);
    }
}

/* Mail checker */
#ifdef MAILDELAY
void check_mail()
{
    static Stringp fname;
    static int fname_inited = FALSE;
    static int error = FALSE, new = FALSE;
    struct stat statbuf;

    if (error) return;
    if (!fname_inited) {
        char *env;
        fname_inited = TRUE;
        Stringinit(fname);
        if (env = getenv("MAIL")) {
            Stringcpy(fname, env);
        } else if (env = getenv("USER")) {
            Sprintf(fname, "%s%s", MAILDIR, env);
        } else {
            oputs("% Warning: can't find name of mail file.");
            error = TRUE;
            return;
        }
    }
    if (stat(fname->s, &statbuf) == -1) {
        if (mail_size) put_mail(mail_size = 0);
    } else {
        if (statbuf.st_size > mail_size) {
            put_mail(TRUE);
            if (new) do_hook(H_MAIL, "%% You have new mail.", "");
        } else if (mail_size != 0 && statbuf.st_size == 0)
            put_mail(FALSE);
        mail_size = statbuf.st_size;
    }
    new = TRUE;
}
#endif

/* Cleanup and error routines. */
void cleanup()
{
    extern int screen_setup;

    cooked_echo_mode();
    disconnect_all();
    if (screen_setup) fix_screen();
}

void die(why)
    char *why;
{
    cleanup();
    puts(why);
    exit(1);
}

