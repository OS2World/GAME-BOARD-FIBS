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

typedef struct Macro {
    struct Macro *next, *prev;           /* list by number */
    struct Macro *tnext, *tprev;         /* list by trigger priority */
    struct Macro *hnext, *hprev;         /* list by hook */
    struct Macro *pnext;                 /* temp ptr for collision list */
    struct Macro *bnext, *bprev;         /* list in hash bucket */
    char *name, *trig, *hargs, *bind, *body;
    void NDECL((*func));                 /* ptr to func, saves lookup */
    int hook;                            /* at least 32 bits anywhere TF runs */
    struct World *world;                 /* only trigger on text from here */
    int pri, num;
    short attr, prob, shots, invis, temp;
} Macro;

enum Hooks {
  H_ACTIVITY,
  H_BACKGROUND,
  H_BAMF,
  H_CONFAIL,
  H_CONNECT,
  H_DISCONNECT,
  H_HISTORY,
  H_KILL,
  H_LOAD,
  H_LOADFAIL,
  H_LOGIN,
  H_MAIL,
  H_MORE,
  H_PENDING,
  H_PROCESS,
  H_REDEF,
  H_RESIZE,
  H_RESUME,
  H_SEND,
  H_SHELL,
  H_WORLD
};

#define AnyHook 07777777                 /* all bits in hook bitfield */

extern void   NDECL(init_macros);
extern short  FDECL(parse_attrs,(char *args));
extern Macro *FDECL(macro_spec,(char *args));
extern struct Macro *FDECL(new_macro,(char *name, char *trig, char *binding,
    int hook, char *hargs, char *body, struct World *world, int pri, int prob,
    int attr, int shots, int invis));
extern void  FDECL(add_macro,(struct Macro *macro));
extern int   FDECL(install_bind,(struct Macro *spec));
extern void  FDECL(add_hook,(char *name, char *body));
extern void  FDECL(do_add,(struct Macro *spec));
extern void  FDECL(do_edit,(struct Macro *spec));
extern void  FDECL(remove_macro,(char *args, int attr, int byhook));
extern void  FDECL(nuke_macro,(struct Macro *macro));
extern void  FDECL(purge_macro,(struct Macro *spec));
extern void  FDECL(remove_by_number,(char *args));
extern void  FDECL(save_macros,(char *args));
extern void  FDECL(list_macros,(char *args));
extern int   FDECL(world_subs,(char *src, Stringp dest));
extern void  FDECL(do_macro,(char *name, char *args, 
    Stringp dest, int toplevel));
extern short VDECL(do_hook,(int index, char *fmt, char *argfmt, ...));
extern void  FDECL(get_macro_body,(char *name, Stringp dest));
extern short FDECL(check_trigger,(char *s));

