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

#ifdef DMALLOC
#define Stringinit(str) dStringinit(str, __FILE__, __LINE__)
#endif

/* This saves time, but make sure function it is used in cannot call
 * itself, or overwriting may occur.
 */
#define STATIC_BUFFER(name) \
    static Stringp (name); \
    static int buffer_inited = FALSE; \
    if (!buffer_inited) { \
        Stringinit((name)); \
        buffer_inited = TRUE; \
    }

typedef struct String {
    char *s;
    int len, maxlen;
} String, Stringp[1];          /* Stretchybuffer */


#ifdef DMALLOC
extern void  FDECL(dStringinit,(Stringp str, char *file, int line));
#else
extern void  FDECL(Stringinit,(Stringp str));
#endif
extern void  FDECL(Stringfree,(Stringp str));
extern void  FDECL(Stringadd,(Stringp str, int c));
extern void  FDECL(Stringnadd,(Stringp str, int c, int n));
extern void  FDECL(Stringterm,(Stringp str, int len));
extern void  FDECL(Stringcpy,(Stringp dest, char *src));
extern void  FDECL(SStringcpy,(Stringp dest, Stringp src));
extern void  FDECL(Stringncpy,(Stringp dest, char *src, int len));
extern void  FDECL(Stringcat,(Stringp dest, char *src));
extern void  FDECL(SStringcat,(Stringp dest, Stringp src));
extern void  FDECL(Stringncat,(Stringp dest, char *src, int len));
extern void  FDECL(Stringexpand,(Stringp str));
extern char *FDECL(fgetS,(Stringp str, FILE *stream));
extern char *FDECL(getword,(Stringp str, char *line));
extern void  FDECL(stripString,(Stringp str));
extern void  FDECL(newline_package,(Stringp buffer, int nl));

