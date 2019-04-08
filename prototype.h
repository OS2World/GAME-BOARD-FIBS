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

/***************************************
 * ANSI/traditional C prototype macros *
 ***************************************/

#ifdef __STDC__
# ifndef PROTOTYPES
#  define PROTOTYPES
# endif
# define PDECL(f, p)      f p
#else
# define PDECL(f, p)      f()
#endif

#ifdef PROTOTYPES
# define NDECL(f)        f(void)
# define FDECL(f, p)     f p
# define VDECL(f, p)     f p
#else
# define NDECL(f)        f()
# define FDECL(f, p)     f()
# define VDECL(f, p)     f()
#endif
