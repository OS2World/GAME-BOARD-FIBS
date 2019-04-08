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

/****************************************
 * TinyFugue global types and variables. *
 ****************************************/

#include "prototype.h"

#ifdef __STDC__
# if __STDC__
#  define STANDARD_C_YES_SIREE
# endif
#endif

/* standard stuff */
#ifdef STANDARD_C_YES_SIREE
# include <stdlib.h>
#else
#if 0
extern char *FDECL(getenv,(char *envvar));
extern FILE *FDECL(popen,(char *command, char *type));
extern char *FDECL(malloc,(long unsigned size));
extern char *FDECL(realloc,(char *ptr, long unsigned size));
extern void  FDECL(free,(char *ptr));
#endif
#endif

#ifndef hpux
# ifndef SYSVTTY
#  define BSD
# endif
#endif

typedef void NDECL((NFunc));
typedef void FDECL((Handler),(char *arguments));
#define HANDLER(name) void FDECL(name,(char *arguments))

/* This works if you cast all time_t's to TIME_T. */
# define TIME_T         unsigned long

#define TRUE 1
#define FALSE 0

#ifndef TFLIBRARY
#define TFLIBRARY      "/usr/local/lib/tfrc"
#endif
#define PRIVATEINIT    "tfrc"
#define CONFIGFILE     "tinytalk"          /* for backward compatibility */
#define MAILDIR        "/usr/spool/mail/"

#define SAVEGLOBAL    1000     /* global history size */
#define SAVEWORLD     1000     /* world history size */
#define SAVELOCAL      100     /* local history size */
#define SAVEINPUT       50     /* command history buffer size */
#define WATCHLINES       5     /* number for watchdog to change */
#define NAMEMATCHNUM     4     /* ask to gag if this many last lines by same */
#define STRINGMATCHNUM   2     /* ignore if this many of last lines identical */
#define MAXQUIET        25     /* max # of lines to suppress during login */

typedef char smallstr[65];     /* Short buffer */

#define F_NORM       00001
#define F_GAG        00002
#define F_NORECORD   00004
#define F_SUPERGAG   (F_GAG | F_NORECORD)
#define F_UNDERLINE  00010
#define F_REVERSE    00020
#define F_FLASH      00040
#define F_DIM        00100
#define F_HILITE     00200
#define F_BELL       00400
#define F_ALL        00777
#define F_NEWLINE    01000

