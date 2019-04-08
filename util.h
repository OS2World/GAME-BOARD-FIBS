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

#ifdef STANDARD_C_YES_SIREE
# include <stdarg.h>
#else
# include <varargs.h>
#endif

#ifndef BSD
# include <string.h>
# define bcopy(src, dst, len) memcpy((dst), (src), (len))
#else
# ifdef __GNUC__
extern char *strchr(), *strrchr();
# else
#  include <strings.h>
#  ifndef STANDARD_C_YES_SIREE
#   define strchr(x, y) index((x), (y))
#   define strrchr(x, y) rindex((x), (y))
#  endif
# endif
#endif

extern int errno;
#ifndef STANDARD_C_YES_SIREE
#define _ERRLIST
#endif
#ifdef __GNUC__
#define _ERRLIST
#endif
#ifdef _ERRLIST
extern int sys_nerr;
extern const char * const sys_errlist[];
#define STRERROR(n) (((n) > 0 && (n) < sys_nerr) ? sys_errlist[(n)] : \
    "unknown error")
#endif

#ifdef _STRSTR
#define STRSTR(s1, s2) tf_strstr((s1), (s2))
#else
#define STRSTR(s1, s2) strstr((s1), (s2))
#endif

#ifdef DMALLOC
#define MALLOC(size) dmalloc((size), __FILE__, __LINE__)
#define REALLOC(ptr, size) drealloc((ptr), (size), __FILE__, __LINE__)
#define FREE(ptr) dfree((char*)(ptr), __FILE__, __LINE__)
#else
#define MALLOC(size) dmalloc((size))
#define REALLOC(ptr, size) drealloc((ptr), (size))
#define FREE(ptr) free((char*)(ptr))
#endif

#ifdef DMALLOC
# define STRDUP(src, file, line) \
    (strcpy(MALLOC(strlen(src) + 1, (file), (line)), (src)))
#else
# define STRDUP(src) (strcpy(MALLOC(strlen(src) + 1), (src)))
#endif

extern char lowercase_values[128], uppercase_values[128];
#define lcase(x) (lowercase_values[(x)])
#define ucase(x) (uppercase_values[(x)])

typedef struct Aline {
    char *str;
    short links, attrs;
} Aline;                       /* shared line, with attributes */

typedef struct TFILE {
    FILE *fp;
    int ispipe;
    char *name;
} TFILE;                       /* file or pipe with standard i/o */

#include <errno.h>

#define operror(str) (oprintf("%s: %s", str, STRERROR(errno)))
#define equalstr(s, t) (!smatch((s), (t)))
#define tfgetS(S, file) fgetS((S), (file)->fp)
#define tfputs(s, file) fputs((s), (file)->fp)
#define tfputc(c, file) fputc((c), (file)->fp)
#define tfflush(file) fflush((file)->fp)
#define tfjump(file, offset) \
    ((file)->ispipe ? \
        pipejump((file), (offset)) : \
        fseek((file)->fp, (offset), 0))
#define tfclose(file) \
    do { \
        file->ispipe ? pclose(file->fp) : fclose(file->fp);\
        FREE(file->name);\
        FREE(file);\
    } while (0)

extern void  NDECL(init_table);
extern TFILE *FDECL(tfopen,(char *fname, char *dname, char *mode));
extern int    FDECL(pipejump,(TFILE *file, long offset));
#ifdef DMALLOC
extern char *FDECL(dmalloc,(long unsigned size, char *file, int line));
extern char *FDECL(drealloc,(char *ptr, long unsigned size, char *file, int line));
extern void  FDECL(dfree,(char *ptr, char *file, int line));
#else
extern char *FDECL(dmalloc,(long unsigned size));
extern char *FDECL(drealloc,(char *ptr, long unsigned size));
#endif
extern char *FDECL(cstrchr,(char *s, int c));
extern char *FDECL(estrchr,(char *s, int c, int e));
extern int   FDECL(cstrcmp,(char *s, char *t));
extern int   FDECL(cstrncmp,(char *s, char *t, int n));
#ifdef _STRSTR
extern char *FDECL(tf_strstr,(char *s1, char *s2));
#endif
extern int   FDECL(numarg,(char **str));
extern int   FDECL(smatch,(char *s, char *t));
extern int   FDECL(smatch_check,(char *s));
extern char *FDECL(stripstr,(char *s));
extern void  FDECL(vSprintf,(Stringp buf, char *fmt, va_list ap));
extern void  VDECL(oprintf,(char *fmt, ...));
extern void  VDECL(Sprintf,(Stringp buf, char *fmt, ...));
extern char  NDECL(getch);
extern void  FDECL(startopt,(char *args, char *opts));
extern char  FDECL(nextopt,(char **arg, int *num));
#ifdef DMALLOC
extern Aline *FDECL(dnew_aline,(char *str, int attrs, char *file, int line));
#else
extern Aline *FDECL(new_aline,(char *str, int attrs));
#endif
extern void  FDECL(free_aline,(Aline *aline));
#ifdef MAILDELAY
extern void  NDECL(check_mail);
#endif
extern void  NDECL(cleanup);
extern void  FDECL(die,(char *why));
