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

extern void NDECL(do_ps);
extern void FDECL(do_kill,(int pid));
extern void FDECL(kill_procs_by_world,(struct World *world));
extern void NDECL(kill_procs);
extern void FDECL(runall,(TIME_T now));
extern void FDECL(start_quote,(char *args));
extern void FDECL(start_repeat,(char *args));
