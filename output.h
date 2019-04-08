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

#ifdef TERMCAP
# define VISUAL
#endif
#ifdef HARDCODE
# define VISUAL
#endif


extern void NDECL(ipos);
extern void NDECL(clr);
extern void FDECL(putch,(int c));
extern void NDECL(init_term);
extern void NDECL(setup_screen);
extern void NDECL(oflush);
extern void FDECL(clear_more,(int new));
#ifdef VISUAL
extern void FDECL(put_world,(char *name));
extern void FDECL(put_mail,(int flag));
extern void FDECL(put_logging,(int flag));
extern void FDECL(put_active,(int count));
#else
# define put_world(name)
# define put_mail(flag)
# define put_logging(flag)
# define put_active(count)
#endif
extern void NDECL(toggle_insert);
extern void NDECL(fix_screen);
extern void NDECL(clear_input_window);
extern void FDECL(iputs,(char *s));
extern void NDECL(inewline);
extern void NDECL(ibs);
extern void FDECL(ibackword,(int place));
extern void FDECL(newpos,(int place));
extern void NDECL(dEOL);
extern void NDECL(do_refresh);
extern void NDECL(do_page);
extern void NDECL(do_hpage);
extern void NDECL(do_line);
extern void NDECL(do_flush);
extern void NDECL(do_line_refresh);
extern void NDECL(do_replace);
extern void NDECL(reset_outcount);
extern void FDECL(aoutput,(struct Aline *aline));
#define output(s, attrs) aoutput(new_aline((s), (attrs)))
#define oputs(s) output((s), F_NEWLINE)
extern void FDECL(localoutput,(struct Aline *aline));
extern void FDECL(enable_wrap,(int column));
extern void NDECL(disable_wrap);
extern int  NDECL(getwrap);
extern void FDECL(setprompt,(char *s));
